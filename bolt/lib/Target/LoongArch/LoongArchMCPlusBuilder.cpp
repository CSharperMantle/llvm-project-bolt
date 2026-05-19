//===- bolt/Target/LoongArch/LoongArchMCPlusBuilder.cpp -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides LoongArch-specific MCPlus builder.
//
//===----------------------------------------------------------------------===//

#include "LoongArchMCSymbolizer.h"
#include "MCTargetDesc/LoongArchFixupKinds.h"
#include "MCTargetDesc/LoongArchMCAsmInfo.h"
#include "MCTargetDesc/LoongArchMCTargetDesc.h"
#include "bolt/Core/BinaryBasicBlock.h"
#include "bolt/Core/MCPlusBuilder.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/ErrorHandling.h"

#define DEBUG_TYPE "mcplus"

using namespace llvm;
using namespace bolt;

namespace {

// ── Instruction pattern matchers for dispatch reconstruction ──

static bool matchPcaddi(const MCInst &Inst, MCRegister Rd) {
  return Inst.getOpcode() == LoongArch::PCADDI && Inst.getNumOperands() >= 2 &&
         Inst.getOperand(0).isReg() && Inst.getOperand(0).getReg() == Rd &&
         Inst.getOperand(1).isExpr();
}

static bool matchPcaddu18i(const MCInst &Inst, MCRegister Rd) {
  return Inst.getOpcode() == LoongArch::PCADDU18I &&
         Inst.getNumOperands() >= 2 && Inst.getOperand(0).isReg() &&
         Inst.getOperand(0).getReg() == Rd && Inst.getOperand(1).isExpr();
}

static bool matchAddiD(const MCInst &Inst, MCRegister Rd, MCRegister &RsOut) {
  if (Inst.getOpcode() != LoongArch::ADDI_D || Inst.getNumOperands() < 3)
    return false;
  if (!Inst.getOperand(0).isReg() || Inst.getOperand(0).getReg() != Rd)
    return false;
  if (!Inst.getOperand(1).isReg())
    return false;
  RsOut = Inst.getOperand(1).getReg();
  return true;
}

static bool matchPcalau12i(const MCInst &Inst, MCRegister Rd) {
  return Inst.getOpcode() == LoongArch::PCALAU12I &&
         Inst.getNumOperands() >= 2 && Inst.getOperand(0).isReg() &&
         Inst.getOperand(0).getReg() == Rd && Inst.getOperand(1).isExpr();
}

static bool matchSlliD(const MCInst &Inst, MCRegister Rd, unsigned Shift,
                       MCRegister &RsOut) {
  if (Inst.getOpcode() != LoongArch::SLLI_D || Inst.getNumOperands() < 3)
    return false;
  if (!Inst.getOperand(0).isReg() || Inst.getOperand(0).getReg() != Rd)
    return false;
  if (!Inst.getOperand(1).isReg())
    return false;
  if (!Inst.getOperand(2).isImm() || Inst.getOperand(2).getImm() != Shift)
    return false;
  RsOut = Inst.getOperand(1).getReg();
  return true;
}

static bool matchLdxD(const MCInst &Inst, MCRegister &RjOut,
                      MCRegister &RkOut) {
  if (Inst.getOpcode() != LoongArch::LDX_D || Inst.getNumOperands() < 3)
    return false;
  if (!Inst.getOperand(1).isReg() || !Inst.getOperand(2).isReg())
    return false;
  RjOut = Inst.getOperand(1).getReg();
  RkOut = Inst.getOperand(2).getReg();
  return true;
}

static bool matchLdxW(const MCInst &Inst, MCRegister &RjOut,
                      MCRegister &RkOut) {
  if (Inst.getOpcode() != LoongArch::LDX_W || Inst.getNumOperands() < 3)
    return false;
  if (!Inst.getOperand(1).isReg() || !Inst.getOperand(2).isReg())
    return false;
  RjOut = Inst.getOperand(1).getReg();
  RkOut = Inst.getOperand(2).getReg();
  return true;
}

static bool matchLdD(const MCInst &Inst, MCRegister &RjOut) {
  unsigned Opc = Inst.getOpcode();
  if (Opc != LoongArch::LDPTR_D && Opc != LoongArch::LD_D)
    return false;
  if (!Inst.getOperand(1).isReg())
    return false;
  RjOut = Inst.getOperand(1).getReg();
  return true;
}

static bool matchAlslD(const MCInst &Inst, unsigned Shift, MCRegister &RjOut,
                       MCRegister &RkOut) {
  if (Inst.getOpcode() != LoongArch::ALSL_D || Inst.getNumOperands() < 4)
    return false;
  if (!Inst.getOperand(1).isReg() || !Inst.getOperand(2).isReg() ||
      !Inst.getOperand(3).isImm())
    return false;
  if (Inst.getOperand(3).getImm() != Shift)
    return false;
  RjOut = Inst.getOperand(1).getReg();
  RkOut = Inst.getOperand(2).getReg();
  return true;
}

static bool matchAddD(const MCInst &Inst, MCRegister &RjOut,
                      MCRegister &RkOut) {
  if (Inst.getOpcode() != LoongArch::ADD_D || Inst.getNumOperands() < 3)
    return false;
  if (!Inst.getOperand(1).isReg() || !Inst.getOperand(2).isReg())
    return false;
  RjOut = Inst.getOperand(1).getReg();
  RkOut = Inst.getOperand(2).getReg();
  return true;
}

class LoongArchMCPlusBuilder : public MCPlusBuilder {
public:
  using MCPlusBuilder::MCPlusBuilder;

  std::unique_ptr<MCSymbolizer> createTargetSymbolizer(BinaryFunction &Function,
                                                       bool) const override {
    return std::make_unique<LoongArchMCSymbolizer>(Function);
  }

  bool shouldRecordCodeRelocation(uint32_t RelType) const override {
    switch (RelType) {
    case ELF::R_LARCH_32:
    case ELF::R_LARCH_B16:
    case ELF::R_LARCH_B21:
    case ELF::R_LARCH_B26:
    case ELF::R_LARCH_ABS_HI20:
    case ELF::R_LARCH_ABS_LO12:
    case ELF::R_LARCH_ABS64_LO20:
    case ELF::R_LARCH_ABS64_HI12:
    case ELF::R_LARCH_PCALA_LO12:
    case ELF::R_LARCH_PCALA_HI20:
    case ELF::R_LARCH_PCALA64_LO20:
    case ELF::R_LARCH_PCALA64_HI12:
    case ELF::R_LARCH_GOT_PC_LO12:
    case ELF::R_LARCH_GOT_PC_HI20:
    case ELF::R_LARCH_GOT64_PC_LO20:
    case ELF::R_LARCH_GOT64_PC_HI12:
    case ELF::R_LARCH_GOT_HI20:
    case ELF::R_LARCH_GOT_LO12:
    case ELF::R_LARCH_GOT64_LO20:
    case ELF::R_LARCH_GOT64_HI12:
    case ELF::R_LARCH_TLS_LE_HI20:
    case ELF::R_LARCH_TLS_LE_LO12:
    case ELF::R_LARCH_TLS_IE_PC_HI20:
    case ELF::R_LARCH_TLS_IE_PC_LO12:
    case ELF::R_LARCH_TLS_LD_PC_HI20:
    case ELF::R_LARCH_TLS_GD_PC_HI20:
    case ELF::R_LARCH_32_PCREL:
    case ELF::R_LARCH_PCREL20_S2:
    case ELF::R_LARCH_64_PCREL:
    case ELF::R_LARCH_CALL36:
    case ELF::R_LARCH_TLS_DESC_PC_HI20:
    case ELF::R_LARCH_TLS_DESC_PC_LO12:
    case ELF::R_LARCH_TLS_DESC64_PC_LO20:
    case ELF::R_LARCH_TLS_DESC64_PC_HI12:
    case ELF::R_LARCH_TLS_DESC_HI20:
    case ELF::R_LARCH_TLS_DESC_LO12:
    case ELF::R_LARCH_TLS_DESC64_LO20:
    case ELF::R_LARCH_TLS_DESC64_HI12:
    case ELF::R_LARCH_TLS_LD_PCREL20_S2:
    case ELF::R_LARCH_TLS_GD_PCREL20_S2:
    case ELF::R_LARCH_TLS_DESC_PCREL20_S2:
    case ELF::R_LARCH_PCADD_HI20:
    case ELF::R_LARCH_PCADD_LO12:
    case ELF::R_LARCH_GOT_PCADD_HI20:
    case ELF::R_LARCH_GOT_PCADD_LO12:
      return true;
    case ELF::R_LARCH_TLS_DESC_LD:
    case ELF::R_LARCH_TLS_DESC_CALL:
      return false;
    default:
      llvm_unreachable("Unexpected LoongArch relocation type in code");
    }
  }

  bool isIndirectCall(const MCInst &Inst) const override {
    if (!isCall(Inst))
      return false;

    switch (Inst.getOpcode()) {
    default:
      return false;
    case LoongArch::JIRL:
      return true;
    }
  }

  bool isNoop(const MCInst &Inst) const override {
    return Inst.getOpcode() == LoongArch::ANDI && Inst.getNumOperands() == 3 &&
           Inst.getOperand(0).isReg() &&
           Inst.getOperand(0).getReg() == LoongArch::R0 &&
           Inst.getOperand(1).isReg() &&
           Inst.getOperand(1).getReg() == LoongArch::R0;
  }

  bool hasPCRelOperand(const MCInst &Inst) const override {
    switch (Inst.getOpcode()) {
    default:
      return false;
    case LoongArch::B:
    case LoongArch::BL:
      return true;
    }
  }

  void replaceBranchTarget(MCInst &Inst, const MCSymbol *TBB,
                           MCContext *Ctx) const override {
    assert((isCall(Inst) || isBranch(Inst)) && !isIndirectBranch(Inst) &&
           "Invalid instruction");

    unsigned SymOpIndex;
    auto Result = getSymbolRefOperandNum(Inst, SymOpIndex);
    (void)Result;
    assert(Result && "unimplemented branch");

    Inst.getOperand(SymOpIndex) =
        MCOperand::createExpr(MCSymbolRefExpr::create(TBB, *Ctx));
  }

  IndirectBranchType analyzeIndirectBranch(
      MCInst &Instruction, InstructionIterator Begin, InstructionIterator End,
      const unsigned PtrSize, MCInst *&MemLocInstr, unsigned &BaseRegNum,
      unsigned &IndexRegNum, int64_t &DispValue, const MCExpr *&DispExpr,
      MCInst *&PCRelBaseOut, MCInst *&FixedEntryLoadInst) const override {
    MemLocInstr = nullptr;
    BaseRegNum = 0;
    IndexRegNum = 0;
    DispValue = 0;
    DispExpr = nullptr;
    PCRelBaseOut = nullptr;
    FixedEntryLoadInst = nullptr;

    if (Instruction.getOpcode() != LoongArch::JIRL ||
        Instruction.getNumOperands() < 3 ||
        !Instruction.getOperand(0).isReg() ||
        !Instruction.getOperand(1).isReg())
      return IndirectBranchType::UNKNOWN;

    const MCRegister JirlRd = Instruction.getOperand(0).getReg();
    const MCRegister JirlRj = Instruction.getOperand(1).getReg();

    // Filter out returns.
    if (JirlRd == LoongArch::R0 && JirlRj == LoongArch::R1)
      return IndirectBranchType::UNKNOWN;

    // Helper: Find the most recent instruction before Start (but at or after
    // Begin) that writes Reg.  Returns Start if no definition is found.
    const auto findRegDef = [&](MCRegister Reg, InstructionIterator Start) {
      InstructionIterator I = Start;
      while (I != Begin) {
        --I;
        MCInst &Cur = *I;
        if (&Cur == &Instruction || isPseudo(Cur) || isCFI(Cur))
          continue;
        BitVector WRegs(RegInfo->getNumRegs(), false);
        getWrittenRegs(Cur, WRegs);
        if (WRegs[Reg])
          return I;
      }
      return Start;
    };

    // Helper: For a provided `ld.d TargetReg, $sp, StackOffset`, find its
    // matching `st.d ???, $sp, StackOffset`.
    const auto findStackStoreForLoad = [&](InstructionIterator LoadIt,
                                           MCRegister TargetReg)
        -> std::optional<std::pair<InstructionIterator, MCRegister>> {
      if (!isStackPtrLoad(*LoadIt) || LoadIt->getNumOperands() < 3 ||
          !LoadIt->getOperand(0).isReg() ||
          LoadIt->getOperand(0).getReg() != TargetReg ||
          !LoadIt->getOperand(2).isImm())
        return std::nullopt;

      const int64_t StackOffset = LoadIt->getOperand(2).getImm();
      InstructionIterator StoreIt = LoadIt;
      while (StoreIt != Begin) {
        --StoreIt;
        if (!isStackPtrStore(*StoreIt) || StoreIt->getNumOperands() < 3 ||
            !StoreIt->getOperand(2).isImm() ||
            StoreIt->getOperand(2).getImm() != StackOffset)
          continue;
        return std::make_pair(StoreIt, StoreIt->getOperand(0).getReg());
      }
      return std::nullopt;
    };

    // Helper: Resolve simple PC-relative base materialization for JT dispatch:
    //    pcaddi    $Reg, %pcrel_20(label)
    //    # --- or ---
    //    pcalau12i $Reg, %pc_hi20(label)
    //    addi.d    $Reg, $Reg, %pc_lo12(label)
    const auto resolveDirectPcRelBase =
        [&](InstructionIterator Def, MCRegister TargetReg,
            const MCExpr *&DispExprOut, MCInst *&PCRelBaseOut) -> bool {
      if (matchPcaddi(*Def, TargetReg)) {
        PCRelBaseOut = &*Def;
        DispExprOut = Def->getOperand(1).getExpr();
        return true;
      }
      MCRegister AddiSrc;
      if (matchAddiD(*Def, TargetReg, AddiSrc)) {
        InstructionIterator PcalauIt = findRegDef(AddiSrc, Def);
        if (PcalauIt != Def && matchPcalau12i(*PcalauIt, AddiSrc)) {
          PCRelBaseOut = &*PcalauIt;
          DispExprOut = PcalauIt->getOperand(1).getExpr();
          return true;
        }
      }
      return false;
    };

    // Helper: Resolve both simple and spilled PC-relative base materialization for JT dispatch.
    const auto resolvePcRelBase =
        [&](InstructionIterator Def, MCRegister TargetReg,
            const MCExpr *&DispExprOut, MCInst *&PCRelBaseOut) -> bool {
      if (resolveDirectPcRelBase(Def, TargetReg, DispExprOut, PCRelBaseOut))
        return true;

      if (std::optional<std::pair<InstructionIterator, MCRegister>> Store =
              findStackStoreForLoad(Def, TargetReg)) {
        InstructionIterator StoredRegDefIt = findRegDef(Store->second,
                                                        Store->first);
        if (StoredRegDefIt == Store->first)
          return false;
        return resolveDirectPcRelBase(StoredRegDefIt, Store->second,
                                      DispExprOut, PCRelBaseOut);
      }

      return false;
    };

    // Path 1: LLVM Clang non-PIE jump table
    //
    // Base       pcaddi      $LdxBase, .LJTI
    //            # --- or ---
    //            pcalau12i   $LdxBase, %pc_hi20(.LJTI)
    //            addi.d      $LdxBase, $LdxBase, %pc_lo12(.LJTI)
    // Index      slli.d      $LdxIndex, $IndexSrc, 3
    // Load       ldx.d       $JirlRj, $LdxBase, $LdxIndex
    //            jr          $JirlRj
    //
    // Entries: 8-byte absolute in .rodata (R_LARCH_64) -> JTT_NORMAL
    //
    // Cf.
    //   llvm/lib/CodeGen/SelectionDAG/LegalizeDAG.cpp
    //     case ISD::BR_JT:
    do {
      auto LdxDIt = findRegDef(JirlRj, End);
      if (LdxDIt == End)
        // Can't even find the instruction defining JirlRj.
        break;

      MCRegister LdxBase;
      MCRegister LdxIndex;
      if (!matchLdxD(*LdxDIt, LdxBase, LdxIndex))
        // Insn defining JirlRj is not an ldx.d.
        break;

      auto BaseDefIt = findRegDef(LdxBase, LdxDIt);
      if (BaseDefIt == LdxDIt)
        // Can't even find the defn site of LdxBase.
        break;

      if (!resolvePcRelBase(BaseDefIt, LdxBase, DispExpr, PCRelBaseOut))
        // Can't resolve LdxBase loading sequence.
        break;

      auto SlliIt = findRegDef(LdxIndex, LdxDIt);
      if (SlliIt == LdxDIt)
        // Can't even find the defn site of LdxIndex.
        break;

      MCRegister IndexSrc;
      if (!matchSlliD(*SlliIt, LdxIndex, 3, IndexSrc))
        // Can't resolve IndexSrc.
        break;

      MemLocInstr = &*LdxDIt;
      BaseRegNum = LdxBase;
      IndexRegNum = IndexSrc;
      return IndirectBranchType::POSSIBLE_JUMP_TABLE;
    } while (0);

    // Path 2: LLVM Clang PIE jump table
    //
    // Base       pcaddi      $AddBase, .LJTI
    //            # --- or ---
    //            pcalau12i   $AddBase, %pc_hi20(.LJTI)
    //            addi.d      $AddBase, $AddBase, %pc_lo12(.LJTI)
    // Index      slli.d      $LdxIndex, $IndexSrc, 2
    // Load       ldx.w       $LdxRd, $LdxBase, $LdxIndex
    // Add        add.d       $JirlRj, $LdxRd, $AddBase
    //            jr          $JirlRj
    //
    // Entries: 4-byte PC-relative in .rodata (R_LARCH_32_PCREL) -> JTT_PIC
    //
    // Cf.
    //   llvm/lib/CodeGen/SelectionDAG/LegalizeDAG.cpp
    //     "// For PIC, the sequence is:"
    do {
      auto AddDIt = findRegDef(JirlRj, End);
      if (AddDIt == End)
        break;

      // JirlRj spilled to stack?
      if (std::optional<std::pair<InstructionIterator, MCRegister>> Store =
              findStackStoreForLoad(AddDIt, JirlRj)) {
        AddDIt = findRegDef(Store->second, Store->first);
        if (AddDIt == Store->first)
          break;
      }

      MCRegister AddOp1;
      MCRegister AddOp2;
      if (!matchAddD(*AddDIt, AddOp1, AddOp2))
        break;

      // One ADD operand is defined by LDX_W (loaded entry), the other by
      // base materialization (table address).  Try both orderings.
      MCRegister LdxBase;
      MCRegister LdxIndex;
      auto LdxWIt = findRegDef(AddOp1, AddDIt);
      if (!matchLdxW(*LdxWIt, LdxBase, LdxIndex)) {
        LdxWIt = findRegDef(AddOp2, AddDIt);
        if (!matchLdxW(*LdxWIt, LdxBase, LdxIndex))
          // Neither worked.
          break;
      }

      MCRegister LdxRd = LdxWIt->getOperand(0).getReg();
      MCRegister AddBase = (AddOp1 == LdxRd) ? AddOp2 : AddOp1;

      auto BaseDefIt = findRegDef(AddBase, AddDIt);
      if (BaseDefIt == AddDIt)
        break;

      if (!resolvePcRelBase(BaseDefIt, AddBase, DispExpr, PCRelBaseOut))
        break;

      MCRegister IndexSrc;
      auto SlliIt = findRegDef(LdxIndex, LdxWIt);
      if (SlliIt == LdxWIt)
        break;
      if (!matchSlliD(*SlliIt, LdxIndex, 2, IndexSrc)) {
        // Can this be a spill?
        if (!isStackPtrLoad(*SlliIt) || SlliIt->getNumOperands() < 3 ||
            !SlliIt->getOperand(0).isReg() ||
            SlliIt->getOperand(0).getReg() != LdxIndex ||
            !SlliIt->getOperand(2).isImm())
          break;
        // Yes it is!
        IndexSrc = LdxIndex;
      }

      MemLocInstr = &*LdxWIt;
      BaseRegNum = AddBase;
      IndexRegNum = IndexSrc;
      return IndirectBranchType::POSSIBLE_PIC_JUMP_TABLE;
    } while (0);

    // Path 3: GCC non-PIE jump table
    //
    // Base       pcaddi      $BaseReg, .LJTI
    //            # --- or ---
    //            pcalau12i   $BaseReg, %pc_hi20(.LJTI)
    //            addi.d      $BaseReg, $BaseReg, %pc_lo12(.LJTI)
    // Addr       alsl.d      $AddrReg, $IdxReg, $BaseReg, 3
    //            # --- or ---
    //            slli.d      $temp, $IdxReg, 3
    //            add.d       $AddrReg, $BaseReg, $temp
    // Load       ldptr.d     $JirlRj, $AddrReg, 0
    //            # --- or ---
    //            ld.d        $JirlRj, $AddrReg, 0
    //            jr          $JirlRj
    //
    // Entries: 8-byte absolute in .rodata (R_LARCH_64) -> JTT_NORMAL
    //
    // Cf.
    //   gcc/config/loongarch/loongarch.md
    //     gen_tablejump
    //     define_expand "tablejump"
    do {
      auto LoadIt = findRegDef(JirlRj, End);
      if (LoadIt == End)
        break;

      MCRegister AddrReg;
      if (!matchLdD(*LoadIt, AddrReg))
        break;

      auto AddrDefIt = findRegDef(AddrReg, LoadIt);
      if (AddrDefIt == LoadIt)
        break;

      const unsigned ExpectedShift = Log2_32(PtrSize);

      MCRegister BaseReg;
      MCRegister IdxReg;
      if (!matchAlslD(*AddrDefIt, ExpectedShift, IdxReg, BaseReg)) {
        MCRegister AddOp1, AddOp2;
        if (!matchAddD(*AddrDefIt, AddOp1, AddOp2))
          break;

        // slli.d + add.d
        auto Def1 = findRegDef(AddOp1, AddrDefIt);
        auto Def2 = findRegDef(AddOp2, AddrDefIt);
        MCRegister Tmp;
        const bool Op1IsSlli =
            Def1 != AddrDefIt && matchSlliD(*Def1, AddOp1, ExpectedShift, Tmp);
        const bool Op2IsSlli =
            Def2 != AddrDefIt && matchSlliD(*Def2, AddOp2, ExpectedShift, Tmp);

        if (Op1IsSlli && !Op2IsSlli) {
          IdxReg = Tmp;
          BaseReg = AddOp2;
        } else if (!Op1IsSlli && Op2IsSlli) {
          IdxReg = Tmp;
          BaseReg = AddOp1;
        } else {
          // No slli.d found.
          break;
        }
      }

      auto BaseDefIt = findRegDef(BaseReg, AddrDefIt);
      if (BaseDefIt == AddrDefIt)
        break;

      if (!resolvePcRelBase(BaseDefIt, BaseReg, DispExpr, PCRelBaseOut))
        break;

      MemLocInstr = &*LoadIt;
      BaseRegNum = BaseReg;
      IndexRegNum = IdxReg;
      return IndirectBranchType::POSSIBLE_JUMP_TABLE;
    } while (0);

    // Tail call detection
    //
    // Target     pcaddi    $JirlRj, ...
    //            # --- or ---
    //            pcaddu18i $JirlRj, ...
    //            # --- or ---
    //            pcalau12i $JirlRj, ...
    //            addi.d    $JirlRj, $JirlRj, ...
    //            jr        $JirlRj
    //
    // Cf.
    //   <https://reviews.llvm.org/D137889>
    do {
      if (JirlRd != LoongArch::R0)
        break;

      InstructionIterator Def = findRegDef(JirlRj, End);
      if (Def == End)
        break;

      if (matchPcaddi(*Def, JirlRj) || matchPcaddu18i(*Def, JirlRj))
        return IndirectBranchType::POSSIBLE_TAIL_CALL;

      MCRegister AddiSrc;
      if (!matchAddiD(*Def, JirlRj, AddiSrc))
        break;

      InstructionIterator BaseDef = findRegDef(AddiSrc, Def);
      if (BaseDef == Def || !matchPcalau12i(*BaseDef, AddiSrc))
        break;

      return IndirectBranchType::POSSIBLE_TAIL_CALL;
    } while (0);

    return IndirectBranchType::UNKNOWN;
  }

  std::pair<const MCSymbol *, uint64_t>
  getTargetSymbolInfo(const MCExpr *Expr) const override {
    // Unwrap LoongArchMCExpr (e.g., %pc_hi20(sym), %pc_lo12(sym))
    if (const auto *LAExpr = dyn_cast<LoongArchMCExpr>(Expr))
      return getTargetSymbolInfo(LAExpr->getSubExpr());
    return MCPlusBuilder::getTargetSymbolInfo(Expr);
  }

  MCInst::iterator getMemOperandDisp(MCInst &Inst) const override {
    switch (Inst.getOpcode()) {
    default:
      return Inst.end();
    case LoongArch::ADDI_D: // addi.d $rd, $rj, imm
      return Inst.getNumOperands() >= 3 ? (Inst.begin() + 2) : Inst.end();
    case LoongArch::PCALAU12I: // pcalau12i $rd, imm
    case LoongArch::PCADDI:    // pcaddi $rd, imm
      return Inst.getNumOperands() >= 2 ? (Inst.begin() + 1) : Inst.end();
    }
  }

  bool replaceMemOperandDisp(MCInst &Inst, MCOperand Operand) const override {
    MCOperand *OI = getMemOperandDisp(Inst);
    if (OI == Inst.end())
      return false;
    *OI = Operand;
    return true;
  }

  bool convertJmpToTailCall(MCInst &Inst) override {
    if (isTailCall(Inst))
      return false;

    setTailCall(Inst);
    return true;
  }

  void createReturn(MCInst &Inst) const override {
    Inst = MCInstBuilder(LoongArch::JIRL)
               .addReg(LoongArch::R0)
               .addReg(LoongArch::R1)
               .addImm(0);
  }

  void createNoop(MCInst &Inst) const override {
    Inst = MCInstBuilder(LoongArch::ANDI)
               .addReg(LoongArch::R0)
               .addReg(LoongArch::R0)
               .addImm(0);
  }

  void createUncondBranch(MCInst &Inst, const MCSymbol *TBB,
                          MCContext *Ctx) const override {
    Inst =
        MCInstBuilder(LoongArch::B).addExpr(MCSymbolRefExpr::create(TBB, *Ctx));
  }

  int getPCRelEncodingSize(const MCInst &Inst) const override {
    switch (Inst.getOpcode()) {
    default:
      llvm_unreachable("Failed to get pcrel encoding size");
    case LoongArch::BEQ:
    case LoongArch::BNE:
    case LoongArch::BLT:
    case LoongArch::BGE:
    case LoongArch::BLTU:
    case LoongArch::BGEU:
      return 18;
    case LoongArch::BEQZ:
    case LoongArch::BNEZ:
    case LoongArch::BCEQZ:
    case LoongArch::BCNEZ:
      return 23;
    case LoongArch::B:
    case LoongArch::BL:
      return 28;
    }
  }

  int getShortJmpEncodingSize() const override { return 38; }

  int getUncondBranchEncodingSize() const override { return 28; }

  void createShortJmp(InstructionListType &Seq, const MCSymbol *Target,
                      MCContext *Ctx, bool IsTailCall) override {
    InstructionListType Insts(2);

    // pcaddu18i $r21, %call36(target)
    Insts[0] = MCInstBuilder(LoongArch::PCADDU18I)
                   .addReg(LoongArch::R21)
                   .addExpr(LoongArchMCExpr::create(
                       MCSymbolRefExpr::create(Target, *Ctx),
                       ELF::R_LARCH_CALL36, *Ctx));

    // jirl $r0, $r21, 0
    Insts[1] = MCInstBuilder(LoongArch::JIRL)
                   .addReg(LoongArch::R0)
                   .addReg(LoongArch::R21)
                   .addImm(0);
    if (IsTailCall)
      setTailCall(Insts[1]);

    Seq.swap(Insts);
  }

  void createLongJmp(InstructionListType &Seq, const MCSymbol *Target,
                     MCContext *Ctx, bool IsTailCall) override {
    InstructionListType Insts(5);

    // lu12i.w  $r21, %abs_hi20(target)           # bits 31-12
    Insts[0] = MCInstBuilder(LoongArch::LU12I_W)
                   .addReg(LoongArch::R21)
                   .addExpr(LoongArchMCExpr::create(
                       MCSymbolRefExpr::create(Target, *Ctx),
                       ELF::R_LARCH_ABS_HI20, *Ctx));

    // ori      $r21, $r21, %abs_lo12(target)     # bits 11-0
    Insts[1] = MCInstBuilder(LoongArch::ORI)
                   .addReg(LoongArch::R21)
                   .addReg(LoongArch::R21)
                   .addExpr(LoongArchMCExpr::create(
                       MCSymbolRefExpr::create(Target, *Ctx),
                       ELF::R_LARCH_ABS_LO12, *Ctx));

    // lu32i.d  $r21, %abs64_lo20(target)         # bits 51-32
    Insts[2] = MCInstBuilder(LoongArch::LU32I_D)
                   .addReg(LoongArch::R21)
                   .addReg(LoongArch::R21)
                   .addExpr(LoongArchMCExpr::create(
                       MCSymbolRefExpr::create(Target, *Ctx),
                       ELF::R_LARCH_ABS64_LO20, *Ctx));

    // lu52i.d  $r21, $r21, %abs64_hi12(target)   # bits 63-52
    Insts[3] = MCInstBuilder(LoongArch::LU52I_D)
                   .addReg(LoongArch::R21)
                   .addReg(LoongArch::R21)
                   .addExpr(LoongArchMCExpr::create(
                       MCSymbolRefExpr::create(Target, *Ctx),
                       ELF::R_LARCH_ABS64_HI12, *Ctx));

    // jirl     $r0, $r21, 0
    Insts[4] = MCInstBuilder(LoongArch::JIRL)
                   .addReg(LoongArch::R0)
                   .addReg(LoongArch::R21)
                   .addImm(0);
    if (IsTailCall)
      setTailCall(Insts[4]);

    Seq.swap(Insts);
  }

  void createStackPointerIncrement(
      MCInst &Inst, int Size = 8,
      bool NoFlagsClobber = false /* unused */) const override {
    Inst = MCInstBuilder(LoongArch::ADDI_D)
               .addReg(LoongArch::R3)
               .addReg(LoongArch::R3)
               .addImm(-Size);
  }

  void createStackPointerDecrement(
      MCInst &Inst, int Size = 8,
      bool NoFlagsClobber = false /* unused */) const override {
    Inst = MCInstBuilder(LoongArch::ADDI_D)
               .addReg(LoongArch::R3)
               .addReg(LoongArch::R3)
               .addImm(Size);
  }

  bool isEpilogue(const BinaryBasicBlock &BB) const override {
    if (BB.succ_size())
      return false;

    for (auto It = BB.rbegin(); It != BB.rend(); ++It) {
      const MCInst &Instr = *It;
      if (isCFI(Instr) || isPseudo(Instr))
        continue;
      return isReturn(Instr);
    }
    return false;
  }

  void createTrap(MCInst &Inst) const override {
    Inst = MCInstBuilder(LoongArch::BREAK).addImm(0);
  }

  bool isTrap(const MCInst &Inst) const override {
    return Inst.getOpcode() == LoongArch::BREAK && Inst.getNumOperands() == 1 &&
           Inst.getOperand(0).isImm() && Inst.getOperand(0).getImm() == 0;
  }

  StringRef getTrapFillValue() const override {
    return StringRef("\x00\x00\x2a\x00", 4);
  }

  const MCExpr *
  tryGetLoongArchPCADDIPCRel20SubExpr(const MCInst &Inst) const override {
    if (Inst.getOpcode() != LoongArch::PCADDI || Inst.getNumOperands() < 2 ||
        !Inst.getOperand(1).isExpr())
      return nullptr;
    return tryGetPCRel20SubExpr(Inst.getOperand(1).getExpr());
  }

  InstructionListType
  undoLoongArchPCRel20Relaxation(const MCInst &Inst,
                                 MCContext *Ctx) const override {
    const MCExpr *SubExpr =
        tryGetPCRel20SubExpr(Inst.getOperand(1).getExpr(), Ctx);

    assert(SubExpr && "PCADDI with R_LARCH_PCREL20_S2 expected");
    assert(Inst.getOperand(0).isReg() && "unexpected PCADDI operand");

    MCPhysReg Reg = Inst.getOperand(0).getReg();
    assert(SubExpr && "missing PCADDI target expression");

    InstructionListType Insts(2);

    Insts[0] = MCInstBuilder(LoongArch::PCALAU12I)
                   .addReg(Reg)
                   .addExpr(LoongArchMCExpr::create(
                       SubExpr, ELF::R_LARCH_PCALA_HI20, *Ctx));

    Insts[1] = MCInstBuilder(LoongArch::ADDI_D)
                   .addReg(Reg)
                   .addReg(Reg)
                   .addExpr(LoongArchMCExpr::create(
                       SubExpr, ELF::R_LARCH_PCALA_LO12, *Ctx));

    return Insts;
  }

  void createDirectCall(MCInst &Inst, const MCSymbol *Target, MCContext *Ctx,
                        bool IsTailCall) override {
    Inst = MCInstBuilder(IsTailCall ? LoongArch::B : LoongArch::BL)
               .addExpr(MCSymbolRefExpr::create(Target, *Ctx));
    if (IsTailCall)
      setTailCall(Inst);
  }

  void createTailCall(MCInst &Inst, const MCSymbol *Target,
                      MCContext *Ctx) override {
    createDirectCall(Inst, Target, Ctx, /*IsTailCall*/ true);
  }

  void createLongTailCall(InstructionListType &Seq, const MCSymbol *Target,
                          MCContext *Ctx) override {
    createShortJmp(Seq, Target, Ctx, /*IsTailCall*/ true);
  }

  InstructionListType createIndirectPLTCall(MCInst &&DirectCall,
                                            const MCSymbol *TargetLocation,
                                            MCContext *Ctx) override {
    const bool IsTailCall = isTailCall(DirectCall);
    assert((DirectCall.getOpcode() == LoongArch::BL ||
            (DirectCall.getOpcode() == LoongArch::B && IsTailCall)) &&
           "64-bit direct (tail) call instruction expected");

    InstructionListType Insts(3);

    // pcalau12i $r20, %pc_hi20(TargetLocation)
    Insts[0].setOpcode(LoongArch::PCALAU12I);
    Insts[0].clear();
    Insts[0].addOperand(MCOperand::createReg(LoongArch::R20));
    Insts[0].addOperand(MCOperand::createImm(0));
    setOperandToSymbolRef(Insts[0], /* OpNum */ 1, TargetLocation,
                          /* Addend */ 0, Ctx, ELF::R_LARCH_PCALA_HI20);

    // ld.d $r20, $r20, %pc_lo12(TargetLocation)
    Insts[1].setOpcode(LoongArch::LD_D);
    Insts[1].clear();
    Insts[1].addOperand(MCOperand::createReg(LoongArch::R20));
    Insts[1].addOperand(MCOperand::createReg(LoongArch::R20));
    Insts[1].addOperand(MCOperand::createImm(0));
    setOperandToSymbolRef(Insts[1], /* OpNum */ 2, TargetLocation,
                          /* Addend */ 0, Ctx, ELF::R_LARCH_PCALA_LO12);

    // jirl $r1, $r20, 0
    // # or
    // jirl $r0, $r20, 0 (tail)
    Insts[2] = MCInstBuilder(LoongArch::JIRL)
                   .addReg(IsTailCall ? LoongArch::R0 : LoongArch::R1)
                   .addReg(LoongArch::R20)
                   .addImm(0);
    moveAnnotations(std::move(DirectCall), Insts[2]);

    return Insts;
  }

  InstructionListType materializeAddress(const MCSymbol *Target, MCContext *Ctx,
                                         MCPhysReg RegName,
                                         int64_t Addend = 0) const override {
    InstructionListType Insts(2);

    // pcalau12i $RegName, %pc_hi20(Target + Addend)
    Insts[0].setOpcode(LoongArch::PCALAU12I);
    Insts[0].clear();
    Insts[0].addOperand(MCOperand::createReg(RegName));
    Insts[0].addOperand(MCOperand::createImm(0));
    setOperandToSymbolRef(Insts[0], /* OpNum */ 1, Target, Addend, Ctx,
                          ELF::R_LARCH_PCALA_HI20);

    // addi.d $RegName, $RegName, %pc_lo12(Target + Addend)
    Insts[1].setOpcode(LoongArch::ADDI_D);
    Insts[1].clear();
    Insts[1].addOperand(MCOperand::createReg(RegName));
    Insts[1].addOperand(MCOperand::createReg(RegName));
    Insts[1].addOperand(MCOperand::createImm(0));
    setOperandToSymbolRef(Insts[1], /* OpNum */ 2, Target, Addend, Ctx,
                          ELF::R_LARCH_PCALA_LO12);

    return Insts;
  }

  bool analyzeBranch(InstructionIterator Begin, InstructionIterator End,
                     const MCSymbol *&TBB, const MCSymbol *&FBB,
                     MCInst *&CondBranch,
                     MCInst *&UncondBranch) const override {
    auto I = End;

    while (I != Begin) {
      --I;

      // Ignore nops and CFIs
      if (isPseudo(*I) || isNoop(*I))
        continue;

      // Stop when we find the first non-terminator
      if (!isTerminator(*I) || isTailCall(*I) || !isBranch(*I))
        break;

      // Handle indirect branches before unconditional branches. On LoongArch
      // JIRL $r0, $rj is both unconditional and indirect; the unconditional
      // path asserts on missing target symbol so we must check indirect first.
      if (isIndirectBranch(*I))
        return false;

      // Handle unconditional branches.
      if (isUnconditionalBranch(*I)) {
        CondBranch = nullptr;
        UncondBranch = &*I;
        const MCSymbol *Sym = getTargetSymbol(*I);
        assert(Sym != nullptr &&
               "Couldn't extract BB symbol from jump operand");
        TBB = Sym;
        continue;
      }

      if (CondBranch == nullptr) {
        const MCSymbol *TargetBB = getTargetSymbol(*I);
        if (TargetBB == nullptr) {
          // Unrecognized branch target
          return false;
        }
        FBB = TBB;
        TBB = TargetBB;
        CondBranch = &*I;
        continue;
      }

      llvm_unreachable("multiple conditional branches in one BB");
    }
    return true;
  }

  bool getSymbolRefOperandNum(const MCInst &Inst, unsigned &OpNum) const {
    switch (Inst.getOpcode()) {
    default:
      return false;
    case LoongArch::B:
    case LoongArch::BL:
      OpNum = 0;
      return true;
    case LoongArch::PCADDI:
      OpNum = 1;
      return true;
    case LoongArch::BEQZ:
    case LoongArch::BNEZ:
    case LoongArch::BCEQZ:
    case LoongArch::BCNEZ:
      OpNum = 1;
      return true;
    case LoongArch::BEQ:
    case LoongArch::BNE:
    case LoongArch::BLT:
    case LoongArch::BGE:
    case LoongArch::BLTU:
    case LoongArch::BGEU:
    case LoongArch::JIRL:
      OpNum = 2;
      return true;
    }
  }

  const MCSymbol *getTargetSymbol(const MCExpr *Expr) const override {
    auto *LoongArchExpr = dyn_cast<LoongArchMCExpr>(Expr);
    if (LoongArchExpr && LoongArchExpr->getSubExpr())
      return getTargetSymbol(LoongArchExpr->getSubExpr());

    auto *BinExpr = dyn_cast<MCBinaryExpr>(Expr);
    if (BinExpr)
      return getTargetSymbol(BinExpr->getLHS());

    auto *SymExpr = dyn_cast<MCSymbolRefExpr>(Expr);
    if (SymExpr && (SymExpr->getKind() == LoongArchMCExpr::VK_None ||
                    SymExpr->getKind() == ELF::R_LARCH_PCREL20_S2))
      return &SymExpr->getSymbol();

    return nullptr;
  }

  const MCSymbol *getTargetSymbol(const MCInst &Inst,
                                  unsigned OpNum = 0) const override {
    if (!getSymbolRefOperandNum(Inst, OpNum))
      return nullptr;

    const MCOperand &Op = Inst.getOperand(OpNum);
    if (!Op.isExpr())
      return nullptr;

    return getTargetSymbol(Op.getExpr());
  }

  ///  Matches PLT entry pattern and returns the associated GOT entry address.
  uint64_t analyzePLTEntry(MCInst &Instruction, InstructionIterator Begin,
                           InstructionIterator End,
                           uint64_t BeginPC) const override {

#define CHECK_(cond_)                                                          \
  if (!(cond_)) {                                                              \
    break;                                                                     \
  }                                                                            \
  do {                                                                         \
  } while (0)

    // lld PLT sequence
    //
    // Target   pcaddu12i   $t3, PCRelOffset
    //          ld.[wd]     $t3, $t3, LdOffset
    // Jump     jirl        $t1, $t3, 0
    //          nop
    //
    // Cf.
    //   lld/ELF/Arch/LoongArch.cpp
    //     LoongArch::writePlt()
    do {
      int64_t PCRelOffset;
      int64_t LdOffset;
      auto I = Begin;

      CHECK_(I != End);
      const auto &PCRelInst = *I++;
      CHECK_(PCRelInst.getOpcode() == LoongArch::PCADDU12I);
      CHECK_(PCRelInst.getOperand(0).isReg() &&
             PCRelInst.getOperand(0).getReg() == LoongArch::R15);
      CHECK_(PCRelInst.getOperand(1).isImm());
      PCRelOffset = BeginPC + (PCRelInst.getOperand(1).getImm() << 12);

      CHECK_(I != End);
      const auto &LdInst = *I++;
      CHECK_(LdInst.getOpcode() == LoongArch::LD_D ||
             LdInst.getOpcode() == LoongArch::LD_W);
      CHECK_(LdInst.getOperand(0).isReg() &&
             LdInst.getOperand(0).getReg() == LoongArch::R15);
      CHECK_(LdInst.getOperand(1).isReg() &&
             LdInst.getOperand(1).getReg() == LoongArch::R15);
      LdOffset = LdInst.getOperand(2).getImm();

      CHECK_(I != End);
      const auto &JirlInst = *I++;
      CHECK_(JirlInst.getOpcode() == LoongArch::JIRL);
      CHECK_(JirlInst.getOperand(0).isReg() &&
             JirlInst.getOperand(0).getReg() == LoongArch::R13);
      CHECK_(JirlInst.getOperand(1).isReg() &&
             JirlInst.getOperand(1).getReg() == LoongArch::R15);

      CHECK_(I != End);
      const auto &NopInst = *I++;
      CHECK_(isNoop(NopInst));

      CHECK_(I == End);
      return PCRelOffset + LdOffset;
    } while (0);

    // mold PLT sequence
    //
    // Target   pcalau12i   $t3, PCRelOffset
    //          ld.[wd]     $t3, $t3, LdOffset
    // Jump     jirl        $t1, $t3, 0
    //          break
    //
    // Cf.
    //   https://github.com/rui314/mold/blob/45970e661d462fd664e7249a4bfc20ca4d0c6f39/src/arch-loongarch.cc#L187
    do {
      int64_t PCRelOffset;
      int64_t LdOffset;
      auto I = Begin;

      CHECK_(I != End);
      const auto &PCRelInst = *I++;
      CHECK_(PCRelInst.getOpcode() == LoongArch::PCALAU12I);
      CHECK_(PCRelInst.getOperand(0).isReg() &&
             PCRelInst.getOperand(0).getReg() == LoongArch::R15);
      CHECK_(PCRelInst.getOperand(1).isImm());
      PCRelOffset =
          (BeginPC + (PCRelInst.getOperand(1).getImm() << 12)) & ~0xfffULL;

      CHECK_(I != End);
      const auto &LdInst = *I++;
      CHECK_(LdInst.getOpcode() == LoongArch::LD_D ||
             LdInst.getOpcode() == LoongArch::LD_W);
      CHECK_(LdInst.getOperand(0).isReg() &&
             LdInst.getOperand(0).getReg() == LoongArch::R15);
      CHECK_(LdInst.getOperand(1).isReg() &&
             LdInst.getOperand(1).getReg() == LoongArch::R15);
      LdOffset = LdInst.getOperand(2).getImm();

      CHECK_(I != End);
      const auto &JirlInst = *I++;
      CHECK_(JirlInst.getOpcode() == LoongArch::JIRL);
      CHECK_(JirlInst.getOperand(0).isReg() &&
             JirlInst.getOperand(0).getReg() == LoongArch::R13);
      CHECK_(JirlInst.getOperand(1).isReg() &&
             JirlInst.getOperand(1).getReg() == LoongArch::R15);

      CHECK_(I != End);
      const auto &BreakInst = *I++;
      CHECK_(BreakInst.getOpcode() == LoongArch::BREAK);

      CHECK_(I == End);
      return PCRelOffset + LdOffset;
    } while (0);

    // No patterns matched
    return 0;

#undef CHECK_
  }

  bool replaceImmWithSymbolRef(MCInst &Inst, const MCSymbol *Symbol,
                               int64_t Addend, MCContext *Ctx, int64_t &Value,
                               uint32_t RelType) const override {
    unsigned ImmOpNo = -1U;
    for (unsigned Index = 0; Index < MCPlus::getNumPrimeOperands(Inst);
         ++Index) {
      if (Inst.getOperand(Index).isImm()) {
        ImmOpNo = Index;
        break;
      }
    }
    if (ImmOpNo == -1U)
      return false;

    Value = Inst.getOperand(ImmOpNo).getImm();

    setOperandToSymbolRef(Inst, ImmOpNo, Symbol, Addend, Ctx, RelType);

    return true;
  }

  const MCExpr *getTargetExprFor(MCInst &Inst, const MCExpr *Expr,
                                 MCContext &Ctx,
                                 uint32_t RelType) const override {
    switch (RelType) {
    default:
      return Expr;
    case 0:
      // When called with RelType=0 from replaceMemOperandDisp for JT labeling,
      // wrap in the appropriate expression for instruction encoding.
      switch (Inst.getOpcode()) {
      case LoongArch::PCALAU12I:
        return LoongArchMCExpr::create(Expr, ELF::R_LARCH_PCALA_HI20, Ctx);
      case LoongArch::PCADDI:
        return LoongArchMCExpr::create(Expr, ELF::R_LARCH_PCREL20_S2, Ctx);
      case LoongArch::ADDI_D:
        return LoongArchMCExpr::create(Expr, ELF::R_LARCH_PCALA_LO12, Ctx);
      default:
        return Expr;
      }
    case ELF::R_LARCH_B16:
      return LoongArchMCExpr::create(Expr, ELF::R_LARCH_B16, Ctx);
    case ELF::R_LARCH_B21:
      return LoongArchMCExpr::create(Expr, ELF::R_LARCH_B21, Ctx);
    case ELF::R_LARCH_B26:
      return LoongArchMCExpr::create(Expr, ELF::R_LARCH_B26, Ctx);
    case ELF::R_LARCH_CALL36:
      return LoongArchMCExpr::create(Expr, ELF::R_LARCH_CALL36, Ctx);
    case ELF::R_LARCH_ABS_HI20:
    case ELF::R_LARCH_GOT_HI20:
    case ELF::R_LARCH_TLS_DESC_HI20:
      return LoongArchMCExpr::create(Expr, ELF::R_LARCH_ABS_HI20, Ctx);
    case ELF::R_LARCH_ABS_LO12:
    case ELF::R_LARCH_GOT_LO12:
    case ELF::R_LARCH_TLS_DESC_LO12:
      return LoongArchMCExpr::create(Expr, ELF::R_LARCH_ABS_LO12, Ctx);
    case ELF::R_LARCH_ABS64_LO20:
    case ELF::R_LARCH_GOT64_LO20:
    case ELF::R_LARCH_TLS_DESC64_LO20:
      return LoongArchMCExpr::create(Expr, ELF::R_LARCH_ABS64_LO20, Ctx);
    case ELF::R_LARCH_ABS64_HI12:
    case ELF::R_LARCH_GOT64_HI12:
    case ELF::R_LARCH_TLS_DESC64_HI12:
      return LoongArchMCExpr::create(Expr, ELF::R_LARCH_ABS64_HI12, Ctx);
    case ELF::R_LARCH_PCALA_LO12:
    case ELF::R_LARCH_GOT_PC_LO12:
    case ELF::R_LARCH_TLS_IE_PC_LO12:
    case ELF::R_LARCH_TLS_DESC_PC_LO12:
      return LoongArchMCExpr::create(Expr, ELF::R_LARCH_PCALA_LO12, Ctx);
    case ELF::R_LARCH_PCALA_HI20:
    case ELF::R_LARCH_GOT_PC_HI20:
    case ELF::R_LARCH_TLS_IE_PC_HI20:
    case ELF::R_LARCH_TLS_LD_PC_HI20:
    case ELF::R_LARCH_TLS_GD_PC_HI20:
    case ELF::R_LARCH_TLS_DESC_PC_HI20:
      return LoongArchMCExpr::create(Expr, ELF::R_LARCH_PCALA_HI20, Ctx);
    case ELF::R_LARCH_PCALA64_LO20:
    case ELF::R_LARCH_GOT64_PC_LO20:
    case ELF::R_LARCH_TLS_DESC64_PC_LO20:
      return LoongArchMCExpr::create(Expr, ELF::R_LARCH_PCALA64_LO20, Ctx);
    case ELF::R_LARCH_PCALA64_HI12:
    case ELF::R_LARCH_GOT64_PC_HI12:
    case ELF::R_LARCH_TLS_DESC64_PC_HI12:
      return LoongArchMCExpr::create(Expr, ELF::R_LARCH_PCALA64_HI12, Ctx);
    case ELF::R_LARCH_TLS_LE_HI20:
      return LoongArchMCExpr::create(Expr, ELF::R_LARCH_TLS_LE_HI20, Ctx);
    case ELF::R_LARCH_TLS_LE_LO12:
      return LoongArchMCExpr::create(Expr, ELF::R_LARCH_TLS_LE_LO12, Ctx);
    case ELF::R_LARCH_TLS_LD_PCREL20_S2:
    case ELF::R_LARCH_TLS_GD_PCREL20_S2:
    case ELF::R_LARCH_TLS_DESC_PCREL20_S2:
    case ELF::R_LARCH_PCREL20_S2:
      return LoongArchMCExpr::create(Expr, ELF::R_LARCH_PCREL20_S2, Ctx);
    case ELF::R_LARCH_PCADD_HI20:
    case ELF::R_LARCH_GOT_PCADD_HI20:
    case ELF::R_LARCH_TLS_IE_PCADD_HI20:
    case ELF::R_LARCH_TLS_LD_PCADD_HI20:
    case ELF::R_LARCH_TLS_GD_PCADD_HI20:
    case ELF::R_LARCH_TLS_DESC_PCADD_HI20:
      return LoongArchMCExpr::create(Expr, ELF::R_LARCH_PCADD_HI20, Ctx);
    case ELF::R_LARCH_PCADD_LO12:
    case ELF::R_LARCH_GOT_PCADD_LO12:
    case ELF::R_LARCH_TLS_IE_PCADD_LO12:
    case ELF::R_LARCH_TLS_LD_PCADD_LO12:
    case ELF::R_LARCH_TLS_GD_PCADD_LO12:
    case ELF::R_LARCH_TLS_DESC_PCADD_LO12:
      return LoongArchMCExpr::create(Expr, ELF::R_LARCH_PCADD_LO12, Ctx);
    }
  }

  unsigned getInvertedBranchOpcode(unsigned Opcode) const {
    switch (Opcode) {
    default:
      llvm_unreachable("Failed to invert branch opcode");
      return Opcode;
    case LoongArch::BEQ:
      return LoongArch::BNE;
    case LoongArch::BNE:
      return LoongArch::BEQ;
    case LoongArch::BEQZ:
      return LoongArch::BNEZ;
    case LoongArch::BNEZ:
      return LoongArch::BEQZ;
    case LoongArch::BCEQZ:
      return LoongArch::BCNEZ;
    case LoongArch::BCNEZ:
      return LoongArch::BCEQZ;
    case LoongArch::BLT:
      return LoongArch::BGE;
    case LoongArch::BGE:
      return LoongArch::BLT;
    case LoongArch::BLTU:
      return LoongArch::BGEU;
    case LoongArch::BGEU:
      return LoongArch::BLTU;
    }
  }

  void reverseBranchCondition(MCInst &Inst, const MCSymbol *TBB,
                              MCContext *Ctx) const override {
    Inst.setOpcode(getInvertedBranchOpcode(Inst.getOpcode()));
    replaceBranchTarget(Inst, TBB, Ctx);
  }

  bool lowerTailCall(MCInst &Inst) override {
    removeAnnotation(Inst, MCPlus::MCAnnotation::kTailCall);
    if (getConditionalTailCall(Inst))
      unsetConditionalTailCall(Inst);
    return true;
  }

  MCPhysReg getStackPointer() const override { return LoongArch::R3; }

  MCPhysReg getFramePointer() const override { return LoongArch::R22; }

  MCPhysReg getFlagsReg() const override { return LoongArch::NoRegister; }

  MCPhysReg getIntArgRegister(unsigned ArgNo) const override {
    if (ArgNo < 8)
      return LoongArch::R4 + ArgNo;
    return LoongArch::NoRegister;
  }

  // LoongArch has no st+add sp in one instruction that qualify as a push.
  bool isPush(const MCInst &Inst) const override { return false; }

  // LoongArch has no ld+add sp in one instruction that qualify as a pop.
  bool isPop(const MCInst &Inst) const override { return false; }

  uint16_t getMinFunctionAlignment() const override { return 4; }

  void getCalleeSavedRegs(BitVector &Regs) const override {
    Regs |= getAliases(LoongArch::R22);
    Regs |= getAliases(LoongArch::R23);
    Regs |= getAliases(LoongArch::R24);
    Regs |= getAliases(LoongArch::R25);
    Regs |= getAliases(LoongArch::R26);
    Regs |= getAliases(LoongArch::R27);
    Regs |= getAliases(LoongArch::R28);
    Regs |= getAliases(LoongArch::R29);
    Regs |= getAliases(LoongArch::R30);
    Regs |= getAliases(LoongArch::R31);
  }

  std::optional<Relocation>
  createRelocation(const MCFixup &Fixup,
                   const MCAsmBackend &MAB) const override {
    const MCFixupKindInfo &FKI = MAB.getFixupKindInfo(Fixup.getKind());

    assert(FKI.TargetOffset == 0 && "0-bit relocation offset expected");
    const uint64_t RelOffset = Fixup.getOffset();

    uint32_t RelType;
    switch (Fixup.getKind()) {
    default:
      // TODO: Need more consideration. Refs to x86 or AArch64.
      return std::nullopt;
    case MCFixupKind(LoongArch::fixup_loongarch_b26):
      RelType = ELF::R_LARCH_B26;
      break;
    }

    auto [RelSymbol, RelAddend] = extractFixupExpr(Fixup);

    return Relocation({RelOffset, RelSymbol, RelType, RelAddend, 0});
  }

  bool equals(const MCSpecifierExpr &A, const MCSpecifierExpr &B,
              CompFuncTy Comp) const override {
    const auto &LoongArchExprA = cast<LoongArchMCExpr>(A);
    const auto &LoongArchExprB = cast<LoongArchMCExpr>(B);
    if (LoongArchExprA.getKind() != LoongArchExprB.getKind())
      return false;

    return MCPlusBuilder::equals(*LoongArchExprA.getSubExpr(),
                                 *LoongArchExprB.getSubExpr(), Comp);
  }

  InstructionListType createInstrIncMemory(const MCSymbol *Target,
                                           MCContext *Ctx, bool IsLeaf,
                                           unsigned CodePointerSize) override {
    // addi.d     $sp, $sp, -16
    // st.d       $a0, $sp, 0
    // st.d       $a1, $sp, 8
    // pcalau12i  $a0, %pc_hi20(Target)
    // addi.d     $a0, $a0, %pc_lo12(Target)
    // addi.d     $a1, $r0, 1
    // amadd.d    $r0, $a1, $a0
    // ld.d       $a0, $sp, 0
    // ld.d       $a1, $sp, 8
    // addi.d     $sp, $sp, 16
    InstructionListType Insts;

    spillRegs(Insts, {LoongArch::R4, LoongArch::R5});
    InstructionListType Addr = materializeAddress(Target, Ctx, LoongArch::R4);
    Insts.insert(Insts.end(), Addr.begin(), Addr.end());
    InstructionListType IncInsts =
        createIncMemory(LoongArch::R4, LoongArch::R5, LoongArch::R0);
    Insts.insert(Insts.end(), IncInsts.begin(), IncInsts.end());
    reloadRegs(Insts, {LoongArch::R4, LoongArch::R5});

    return Insts;
  }

  void convertIndirectCallToLoad(MCInst &Inst, MCPhysReg Reg) override {
    bool IsTC = isTailCall(Inst);
    if (IsTC)
      removeAnnotation(Inst, MCPlus::MCAnnotation::kTailCall);
    // Convert jirl $rd, $rj, offset -> or $reg, $r0, original_rj
    // (The original rj register value is the indirect call target.)
    const MCPhysReg TargetReg = Inst.getOperand(1).getReg();
    Inst.setOpcode(LoongArch::OR);
    Inst.insert(Inst.begin(), MCOperand::createReg(Reg));
    Inst.insert(Inst.begin() + 1, MCOperand::createReg(LoongArch::R0));
    Inst.insert(Inst.begin() + 2, MCOperand::createReg(TargetReg));
  }

  InstructionListType createLoadImmediate(const MCPhysReg Dest,
                                          uint64_t Imm) const override {
    // lu12i.w  $rd, (Imm >> 12) & 0xFFFFF      // rd[31:12] = Imm[31:12]
    // ori      $rd, $rd, Imm & 0xFFF           // rd[11:0]  = Imm[11:0]
    // lu32i.d  $rd, $rd, (Imm >> 32) & 0xFFFFF // rd[51:32] = Imm[51:32]
    // lu52i.d  $rd, $rd, (Imm >> 52) & 0xFFF   // rd[63:52] = Imm[63:52]
    InstructionListType Insts(4);

    Insts[0] = MCInstBuilder(LoongArch::LU12I_W)
                   .addReg(Dest)
                   .addImm((Imm >> 12) & 0xFFFFF);
    Insts[1] = MCInstBuilder(LoongArch::ORI)
                   .addReg(Dest)
                   .addReg(Dest)
                   .addImm(Imm & 0xFFF);
    Insts[2] = MCInstBuilder(LoongArch::LU32I_D)
                   .addReg(Dest)
                   .addReg(Dest)
                   .addImm((Imm >> 32) & 0xFFFFF);
    Insts[3] = MCInstBuilder(LoongArch::LU52I_D)
                   .addReg(Dest)
                   .addReg(Dest)
                   .addImm((Imm >> 52) & 0xFFF);

    return Insts;
  }

  BlocksVectorTy indirectCallPromotion(
      const MCInst &CallInst,
      const std::vector<std::pair<MCSymbol *, uint64_t>> &Targets,
      const std::vector<std::pair<MCSymbol *, uint64_t>> &VtableSyms,
      const std::vector<MCInst *> &MethodFetchInsns,
      const bool MinimizeCodeSize, MCContext *Ctx) override {
    (void)MinimizeCodeSize;

    if (CallInst.getOpcode() != LoongArch::JIRL ||
        CallInst.getNumOperands() < 2 || !CallInst.getOperand(1).isReg() ||
        !VtableSyms.empty() || getJumpTable(CallInst))
      return BlocksVectorTy();

    const bool IsTailCall = isTailCall(CallInst);
    const MCPhysReg TargetReg = CallInst.getOperand(1).getReg();
    const MCPhysReg TmpReg =
        TargetReg == LoongArch::R20 ? LoongArch::R21 : LoongArch::R20;
    BlocksVectorTy Results;
    MCSymbol *NextTarget = nullptr;
    MCSymbol *MergeBlock = nullptr;

    const auto appendBranchToMerge = [&](InstructionListType &NewCall) {
      assert(MergeBlock);
      NewCall.emplace_back();
      createUncondBranch(NewCall.back(), MergeBlock, Ctx);
    };

    for (unsigned I = 0; I < Targets.size(); ++I) {
      Results.emplace_back(NextTarget, InstructionListType());
      InstructionListType *NewCall = &Results.back().second;

      InstructionListType Addr =
          Targets[I].first
              ? materializeAddress(Targets[I].first, Ctx, TmpReg)
              : createLoadImmediate(TmpReg, Targets[I].second);
      NewCall->insert(NewCall->end(), Addr.begin(), Addr.end());

      NextTarget = Ctx->createNamedTempSymbol();
      NewCall->push_back(MCInstBuilder(LoongArch::BNE)
                             .addReg(TargetReg)
                             .addReg(TmpReg)
                             .addExpr(MCSymbolRefExpr::create(NextTarget, *Ctx)));

      Results.emplace_back(Ctx->createNamedTempSymbol(), InstructionListType());
      NewCall = &Results.back().second;
      NewCall->emplace_back();
      if (Targets[I].first)
        createDirectCall(NewCall->back(), Targets[I].first, Ctx, IsTailCall);
      else
        createIndirectCallInst(NewCall->back(), IsTailCall, TmpReg, 0);

      if (std::optional<uint32_t> Offset = getOffset(CallInst))
        setOffset(NewCall->back(), *Offset);

      if (!IsTailCall) {
        if (I == 0)
          MergeBlock = Ctx->createNamedTempSymbol();
        else
          appendBranchToMerge(*NewCall);
      }
    }

    Results.emplace_back(NextTarget, InstructionListType());
    InstructionListType &NewCall = Results.back().second;
    for (const MCInst *Inst : MethodFetchInsns)
      if (Inst != &CallInst)
        NewCall.push_back(*Inst);
    NewCall.push_back(CallInst);

    if (!IsTailCall) {
      appendBranchToMerge(NewCall);
      Results.emplace_back(MergeBlock, InstructionListType());
    }

    return Results;
  }

  InstructionListType createInstrumentedIndirectCall(MCInst &&CallInst,
                                                     MCSymbol *HandlerFuncAddr,
                                                     int CallSiteID,
                                                     MCContext *Ctx) override {
    // spill      $a0, $a1                  (save args)
    // convert call to or $a0, $r0, rj      (pass original target in a0)
    // createLoadImmediate $a1, CallSiteID  (pass callsite id in a1)
    // spill      $a0, $a1                  (save the prepared args for the
    // handler) pcalau12i  $t0, Handler              (load handler address)
    // addi.d     $t0, $t0, ...
    // jirl       $ra, $t0, 0               (call handler)
    // carry over annotations
    InstructionListType Insts;

    spillRegs(Insts, {LoongArch::R4, LoongArch::R5});
    Insts.emplace_back(CallInst);
    convertIndirectCallToLoad(Insts.back(), LoongArch::R4);
    InstructionListType LoadImm =
        createLoadImmediate(LoongArch::R5, CallSiteID);
    Insts.insert(Insts.end(), LoadImm.begin(), LoadImm.end());
    spillRegs(Insts, {LoongArch::R4, LoongArch::R5});
    InstructionListType Addr =
        materializeAddress(HandlerFuncAddr, Ctx, LoongArch::R13);
    Insts.insert(Insts.end(), Addr.begin(), Addr.end());
    Insts.emplace_back();
    createIndirectCallInst(Insts.back(), isTailCall(CallInst), LoongArch::R13,
                           0);
    stripAnnotations(Insts.back());
    moveAnnotations(std::move(CallInst), Insts.back());

    return Insts;
  }

  InstructionListType
  createInstrumentedIndCallHandlerEntryBB(const MCSymbol *InstrTrampoline,
                                          const MCSymbol *IndCallHandler,
                                          MCContext *Ctx) override {
    // Check whether InstrTrampoline was initialized and call it if so,
    // then jump to IndCallHandler.
    //
    // spill      $a0, $zero
    // pcalau12i  $a0, %pc_hi20(InstrTrampoline)
    // addi.d     $a0, $a0, %pc_lo12(InstrTrampoline)
    // ld.d       $a0, $a0, 0
    // beqz       $a0, IndCallHandler
    // addi.d     $sp, $sp, -16
    // st.d       $ra, $sp, 0
    // jirl       $ra, $a0, 0
    // ld.d       $ra, $sp, 0
    // addi.d     $sp, $sp, 16
    // b          IndCallHandler
    InstructionListType Insts;

    spillRegs(Insts, {LoongArch::R4, LoongArch::R0});
    InstructionListType Addr =
        materializeAddress(InstrTrampoline, Ctx, LoongArch::R4);
    Insts.insert(Insts.end(), Addr.begin(), Addr.end());
    Insts.emplace_back();
    loadReg(Insts.back(), LoongArch::R4, LoongArch::R4, 0);
    Insts.emplace_back();
    createRegCmpJZ(Insts.back(), LoongArch::R4, IndCallHandler, Ctx);
    Insts.emplace_back();
    createStackPointerIncrement(Insts.back(), 16);
    Insts.emplace_back();
    storeReg(Insts.back(), LoongArch::R1, LoongArch::R3, 0);
    Insts.emplace_back();
    createIndirectCallInst(Insts.back(), false, LoongArch::R4, 0);
    Insts.emplace_back();
    loadReg(Insts.back(), LoongArch::R1, LoongArch::R3, 0);
    Insts.emplace_back();
    createStackPointerDecrement(Insts.back(), 16);
    Insts.emplace_back();
    createDirectCall(Insts.back(), IndCallHandler, Ctx, true);

    return Insts;
  }

  InstructionListType createInstrumentedIndCallHandlerExitBB() const override {
    InstructionListType Insts;

    reloadRegs(Insts, {LoongArch::R4, LoongArch::R5});
    Insts.emplace_back();
    loadReg(Insts.back(), LoongArch::R12, LoongArch::R3, 0);
    Insts.emplace_back();
    createStackPointerDecrement(Insts.back(), 16);
    reloadRegs(Insts, {LoongArch::R4, LoongArch::R5});
    Insts.emplace_back();
    createIndirectCallInst(Insts.back(), true, LoongArch::R12, 0);

    return Insts;
  }

  InstructionListType
  createInstrumentedIndTailCallHandlerExitBB() const override {
    return createInstrumentedIndCallHandlerExitBB();
  }

  InstructionListType createSymbolTrampoline(const MCSymbol *TgtSym,
                                             MCContext *Ctx) override {
    InstructionListType Insts;
    createShortJmp(Insts, TgtSym, Ctx, true);
    return Insts;
  }

  InstructionListType createNumCountersGetter(MCContext *Ctx) const override {
    return createGetter(Ctx, "__bolt_num_counters");
  }

  InstructionListType
  createInstrLocationsGetter(MCContext *Ctx) const override {
    return createGetter(Ctx, "__bolt_instr_locations");
  }

  InstructionListType createInstrTablesGetter(MCContext *Ctx) const override {
    return createGetter(Ctx, "__bolt_instr_tables");
  }

  InstructionListType createInstrNumFuncsGetter(MCContext *Ctx) const override {
    return createGetter(Ctx, "__bolt_instr_num_funcs");
  }

private:
  bool isStackPtrLoad(const MCInst &Inst) const {
    const unsigned Opc = Inst.getOpcode();
    if (Opc != LoongArch::LDPTR_D && Opc != LoongArch::LD_D)
      return false;
    return Inst.getNumOperands() >= 2 && Inst.getOperand(1).isReg() &&
           Inst.getOperand(1).getReg() == LoongArch::R3;
  }

  bool isStackPtrStore(const MCInst &Inst) const {
    const unsigned Opc = Inst.getOpcode();
    if (Opc != LoongArch::STPTR_D && Opc != LoongArch::ST_D)
      return false;
    return Inst.getNumOperands() >= 2 && Inst.getOperand(1).isReg() &&
           Inst.getOperand(1).getReg() == LoongArch::R3;
  }

  /// Load a register from a stack slot.
  void loadReg(MCInst &Inst, MCPhysReg To, MCPhysReg From,
               int64_t Offset) const {
    Inst =
        MCInstBuilder(LoongArch::LD_D).addReg(To).addReg(From).addImm(Offset);
  }

  /// Store a register to a stack slot.
  void storeReg(MCInst &Inst, MCPhysReg From, MCPhysReg To,
                int64_t Offset) const {
    Inst =
        MCInstBuilder(LoongArch::ST_D).addReg(From).addReg(To).addImm(Offset);
  }

  /// Spill callee-saved registers used during instrumentation.
  void spillRegs(InstructionListType &Insts,
                 const SmallVector<unsigned> &Regs) const {
    Insts.emplace_back();
    createStackPointerIncrement(Insts.back(), Regs.size() * 8);

    int64_t Offset = 0;
    for (auto Reg : Regs) {
      Insts.emplace_back();
      storeReg(Insts.back(), Reg, LoongArch::R3, Offset);
      Offset += 8;
    }
  }

  /// Reload callee-saved registers after instrumentation.
  void reloadRegs(InstructionListType &Insts,
                  const SmallVector<unsigned> &Regs) const {
    int64_t Offset = 0;
    for (auto Reg : Regs) {
      Insts.emplace_back();
      loadReg(Insts.back(), Reg, LoongArch::R3, Offset);
      Offset += 8;
    }

    Insts.emplace_back();
    createStackPointerDecrement(Insts.back(), Regs.size() * 8);
  }

  /// Atomically add \p Rk to memory at [ \p Rj ].  \p Rd receives the old
  /// memory value.
  void atomicAdd(MCInst &Inst, MCPhysReg Rd, MCPhysReg Rj, MCPhysReg Rk) const {
    Inst = MCInstBuilder(LoongArch::AMADD_D).addReg(Rd).addReg(Rk).addReg(Rj);
  }

  /// Compare register against zero and branch to \p Target if equal.
  void createRegCmpJZ(MCInst &Inst, MCPhysReg Reg, const MCSymbol *Target,
                      MCContext *Ctx) const {
    Inst = MCInstBuilder(LoongArch::BEQZ)
               .addReg(Reg)
               .addExpr(MCSymbolRefExpr::create(Target, *Ctx));
  }

  /// Emit a getter function that loads a pointer from a symbol.
  InstructionListType createGetter(MCContext *Ctx, const char *Name) const {
    InstructionListType Insts(4);
    MCSymbol *Locs = Ctx->getOrCreateSymbol(Name);
    InstructionListType Addr = materializeAddress(Locs, Ctx, LoongArch::R4);
    std::copy(Addr.begin(), Addr.end(), Insts.begin());
    loadReg(Insts[2], LoongArch::R4, LoongArch::R4, 0);
    createReturn(Insts[3]);
    return Insts;
  }

  /// Emit an atomic memory increment by 1 at [ \p Rj ].
  InstructionListType createIncMemory(MCPhysReg Rj, MCPhysReg Rk,
                                      MCPhysReg Rd) const {
    InstructionListType Insts(2);

    Insts[0] = MCInstBuilder(LoongArch::ADDI_D).addReg(Rk).addReg(Rd).addImm(1);
    atomicAdd(Insts[1], Rd, Rj, Rk);

    return Insts;
  }

  /// Emit an indirect call or tail call via register.
  void createIndirectCallInst(MCInst &Inst, bool IsTailCall, MCPhysReg Reg,
                              int64_t Offset) const {
    Inst = MCInstBuilder(LoongArch::JIRL)
               .addReg(IsTailCall ? LoongArch::R0 : LoongArch::R1)
               .addReg(Reg)
               .addImm(Offset);
    if (IsTailCall)
      setTailCall(Inst);
  }

  const MCExpr *tryGetPCRel20SubExpr(const MCExpr *Expr,
                                     MCContext *Ctx = nullptr) const {
    if (const auto *E = dyn_cast<LoongArchMCExpr>(Expr)) {
      if (E->getSpecifier() == ELF::R_LARCH_PCREL20_S2)
        return E->getSubExpr();
      return nullptr;
    }
    if (const auto *E = dyn_cast<MCSymbolRefExpr>(Expr)) {
      if (E->getSpecifier() == ELF::R_LARCH_PCREL20_S2) {
        assert(Ctx && "MCContext required to strip relocation specifier");
        return MCSymbolRefExpr::create(&E->getSymbol(), *Ctx);
      }
      return nullptr;
    }
    if (const auto *E = dyn_cast<MCBinaryExpr>(Expr)) {
      const MCExpr *LHS = tryGetPCRel20SubExpr(E->getLHS(), Ctx);
      if (LHS && Ctx)
        return MCBinaryExpr::create(E->getOpcode(), LHS, E->getRHS(), *Ctx);
      return nullptr;
    }
    return nullptr;
  }
};

} // end anonymous namespace

namespace llvm {
namespace bolt {

MCPlusBuilder *createLoongArchMCPlusBuilder(const MCInstrAnalysis *Analysis,
                                            const MCInstrInfo *Info,
                                            const MCRegisterInfo *RegInfo,
                                            const MCSubtargetInfo *STI) {
  return new LoongArchMCPlusBuilder(Analysis, Info, RegInfo, STI);
}

} // namespace bolt
} // namespace llvm
