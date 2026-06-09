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
#include "bolt/Core/MCInstUtils.h"
#include "bolt/Core/MCPlusBuilder.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/DataExtractor.h"
#include "llvm/Support/ErrorHandling.h"

#define DEBUG_TYPE "mcplus"

using namespace llvm;
using namespace bolt;

namespace {

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
    case ELF::R_LARCH_64:
    case ELF::R_LARCH_ADD32:
    case ELF::R_LARCH_ADD64:
    case ELF::R_LARCH_SUB32:
    case ELF::R_LARCH_SUB64:
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
    using namespace llvm::bolt::LowLevelInstMatcherDSL;
    return isCall(Inst) && matchInst(Inst, LoongArch::JIRL);
  }

  bool isNoop(const MCInst &Inst) const override {
    using namespace llvm::bolt::LowLevelInstMatcherDSL;
    return matchInst(Inst, LoongArch::ANDI, Reg(LoongArch::R0),
                     Reg(LoongArch::R0), Skip());
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
    using namespace llvm::bolt::LowLevelInstMatcherDSL;

    MemLocInstr = nullptr;
    BaseRegNum = 0;
    IndexRegNum = 0;
    DispValue = 0;
    DispExpr = nullptr;
    PCRelBaseOut = nullptr;
    FixedEntryLoadInst = nullptr;

    Reg JirlRd, JirlRj;
    if (!matchInst(Instruction, LoongArch::JIRL, JirlRd, JirlRj, Skip())) {
      LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match indirect branch: "
                        << "terminator is not jirl\n");
      return IndirectBranchType::UNKNOWN;
    }

    // Filter out returns.
    if (JirlRd.get() == LoongArch::R0 && JirlRj.get() == LoongArch::R1) {
      LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match indirect branch: "
                        << "return instruction\n");
      return IndirectBranchType::UNKNOWN;
    }

    const DenseMap<const MCInst *, SmallVector<MCInst *>> UDChain =
        computeLocalUDChain(&Instruction, Begin, End);

    // Helper: For a provided `ld.d TargetReg, $sp, StackOffset`, find its
    // matching `st.d ???, $sp, StackOffset`.
    const auto findStackStoreForLoad = [&](const MCInst *Load,
                                           MCRegister TargetReg)
        -> std::optional<std::pair<MCInst *, MCRegister>> {
      if (!Load || !isStackPtrLoad(*Load) || Load->getNumOperands() < 3 ||
          !Load->getOperand(0).isReg() ||
          Load->getOperand(0).getReg() != TargetReg ||
          !Load->getOperand(2).isImm())
        return std::nullopt;

      InstructionIterator StoreIt = Begin;
      while (StoreIt != End && &*StoreIt != Load)
        ++StoreIt;
      if (StoreIt == End)
        return std::nullopt;

      const int64_t StackOffset = Load->getOperand(2).getImm();
      while (StoreIt != Begin) {
        --StoreIt;
        if (!isStackPtrStore(*StoreIt) || StoreIt->getNumOperands() < 3 ||
            !StoreIt->getOperand(2).isImm() ||
            StoreIt->getOperand(2).getImm() != StackOffset)
          continue;
        return std::make_pair(&*StoreIt, StoreIt->getOperand(0).getReg());
      }
      return std::nullopt;
    };

    // Helper: Resolve simple PC-relative base materialization for JT dispatch:
    //    pcaddi    $Reg, %pcrel_20(label)
    //    # --- or ---
    //    pcalau12i $Reg, %pc_hi20(label)
    //    addi.d    $Reg, $Reg, %pc_lo12(label)
    const auto resolveDirectPcRelBase = [&](MCInst *Def, MCRegister TargetReg,
                                            const MCExpr *&DispExprOut,
                                            MCInst *&PCRelBaseOut) -> bool {
      if (!Def)
        return false;
      Expr DispExpr;
      if (matchInst(*Def, LoongArch::PCADDI, Reg(TargetReg), DispExpr)) {
        PCRelBaseOut = Def;
        DispExprOut = DispExpr.get();
        return true;
      }
      Reg AddiSrc;
      if (matchInst(*Def, LoongArch::ADDI_D, Reg(TargetReg), AddiSrc)) {
        MCInst *const Pcalau = findRegDef(UDChain, AddiSrc.get(), *Def);
        if (Pcalau &&
            matchInst(*Pcalau, LoongArch::PCALAU12I, AddiSrc, DispExpr)) {
          PCRelBaseOut = Pcalau;
          DispExprOut = DispExpr.get();
          return true;
        }
      }
      return false;
    };

    // Helper: Resolve both simple and spilled PC-relative base materialization
    // for JT dispatch.
    const auto resolvePcRelBase = [&](MCInst *Def, MCRegister TargetReg,
                                      const MCExpr *&DispExprOut,
                                      MCInst *&PCRelBaseOut) -> bool {
      if (resolveDirectPcRelBase(Def, TargetReg, DispExprOut, PCRelBaseOut))
        return true;

      std::optional<std::pair<MCInst *, MCRegister>> Store =
          findStackStoreForLoad(Def, TargetReg);
      if (!Store)
        return false;

      MCInst *StoredRegDef = findRegDef(UDChain, Store->second, *Store->first);
      return StoredRegDef && resolveDirectPcRelBase(StoredRegDef, Store->second,
                                                    DispExprOut, PCRelBaseOut);
    };

    const auto resolveSlliIndex = [&](const MCInst &Use, MCRegister ScaledReg,
                                      unsigned Shift, MCRegister &IndexReg,
                                      bool AllowSpill = false) -> bool {
      MCInst *Def = findRegDef(UDChain, ScaledReg, Use);
      if (!Def)
        return false;

      Reg TheIndexReg;
      if (matchInst(*Def, LoongArch::SLLI_D, Reg(ScaledReg), TheIndexReg,
                    Imm(Shift))) {
        IndexReg = TheIndexReg.get();
        return true;
      }

      if (AllowSpill && isStackPtrLoad(*Def) && Def->getNumOperands() >= 3 &&
          Def->getOperand(0).isReg() &&
          Def->getOperand(0).getReg() == ScaledReg &&
          Def->getOperand(2).isImm()) {
        IndexReg = ScaledReg;
        return true;
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
      MCInst *const LdxD = findRegDef(UDChain, JirlRj.get(), Instruction);
      if (!LdxD) {
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match indirect branch: "
                          << "JirlRj has no local def\n");
        // Can't even find the instruction defining JirlRj.
        break;
      }

      Reg LdxBase, LdxIndex;
      if (!matchInst(*LdxD, LoongArch::LDX_D, Reg(), LdxBase, LdxIndex)) {
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match LLVM non-PIE jump "
                          << "table: JirlRj def is not ldx.d\n");
        // Insn defining JirlRj is not an ldx.d.
        break;
      }

      MCInst *const BaseDef = findRegDef(UDChain, LdxBase.get(), *LdxD);
      if (!BaseDef) {
        // Can't even find the defn site of LdxBase.
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match LLVM non-PIE jump "
                          << "table: LdxBase has no local def\n");
        break;
      }

      if (!resolvePcRelBase(BaseDef, LdxBase.get(), DispExpr, PCRelBaseOut)) {
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match LLVM non-PIE jump "
                          << "table: can't resolve LdxBase\n");
        // Can't resolve LdxBase loading sequence.
        break;
      }

      MCRegister IndexSrc;
      if (!resolveSlliIndex(*LdxD, LdxIndex.get(), 3, IndexSrc)) {
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match LLVM non-PIE jump "
                          << "table: can't resolve scaled index\n");
        // Can't resolve IndexSrc.
        break;
      }

      MemLocInstr = LdxD;
      BaseRegNum = LdxBase.get();
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
      MCInst *AddD = findRegDef(UDChain, JirlRj.get(), Instruction);
      if (!AddD) {
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match indirect branch: "
                          << "JirlRj has no local def\n");
        break;
      }

      // JirlRj spilled to stack?
      if (std::optional<std::pair<MCInst *, MCRegister>> Store =
              findStackStoreForLoad(AddD, JirlRj.get())) {
        AddD = findRegDef(UDChain, Store->second, *Store->first);
        if (!AddD) {
          LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match LLVM PIC jump "
                            << "table: spilled add source has no local def\n");
          break;
        }
      }

      Reg AddOp1, AddOp2;
      if (!matchInst(*AddD, LoongArch::ADD_D, Reg(), AddOp1, AddOp2)) {
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match LLVM PIC jump "
                          << "table: JirlRj def is not add.d\n");
        break;
      }

      Reg LdxRdReg, LdxBaseReg, LdxIndexReg;
      MCInst *LdxW = findRegDef(UDChain, AddOp1.get(), *AddD);
      if (!LdxW || !matchInst(*LdxW, LoongArch::LDX_W, LdxRdReg, LdxBaseReg,
                              LdxIndexReg)) {
        LdxW = findRegDef(UDChain, AddOp2.get(), *AddD);
        if (!LdxW || !matchInst(*LdxW, LoongArch::LDX_W, LdxRdReg, LdxBaseReg,
                                LdxIndexReg)) {
          LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match LLVM PIC jump "
                            << "table: add.d operand is not ldx.w\n");
          break;
        }
      }
      const MCRegister LdxBase = LdxBaseReg.get();
      const MCRegister LdxIndex = LdxIndexReg.get();
      const MCRegister AddBase =
          (AddOp1.get() == LdxRdReg.get()) ? AddOp2.get() : AddOp1.get();

      MCInst *const BaseDef = findRegDef(UDChain, AddBase, *AddD);
      if (!BaseDef) {
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match LLVM PIC jump "
                          << "table: AddBase has no local def\n");
        break;
      }

      if (!resolvePcRelBase(BaseDef, AddBase, DispExpr, PCRelBaseOut)) {
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match LLVM PIC jump "
                          << "table: can't resolve AddBase\n");
        break;
      }

      MCRegister IndexSrc;
      if (!resolveSlliIndex(*LdxW, LdxIndex, 2, IndexSrc,
                            /*AllowSpill=*/true)) {
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match LLVM PIC jump "
                          << "table: can't resolve scaled index\n");
        break;
      }

      MemLocInstr = LdxW;
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
      MCInst *const Load = findRegDef(UDChain, JirlRj.get(), Instruction);
      if (!Load) {
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match indirect branch: "
                          << "JirlRj has no local def\n");
        break;
      }

      Reg AddrReg;
      if (!matchInst(*Load, LoongArch::LD_D, Reg(), AddrReg) &&
          !matchInst(*Load, LoongArch::LDPTR_D, Reg(), AddrReg)) {
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match GCC non-PIE jump "
                          << "table: JirlRj def is not ld.d/ldptr.d\n");
        break;
      }

      MCInst *const AddrDef = findRegDef(UDChain, AddrReg.get(), *Load);
      if (!AddrDef) {
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match GCC non-PIE jump "
                          << "table: AddrReg has no local def\n");
        break;
      }

      const unsigned ExpectedShift = Log2_32(PtrSize);

      MCRegister BaseReg, IdxReg;
      Reg BaseRegObj, IdxRegObj;
      if (!matchInst(*AddrDef, LoongArch::ALSL_D, Reg(), IdxRegObj, BaseRegObj,
                     Imm(ExpectedShift))) {
        Reg AddOp1Reg, AddOp2Reg;
        if (!matchInst(*AddrDef, LoongArch::ADD_D, Reg(), AddOp1Reg,
                       AddOp2Reg)) {
          LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match GCC non-PIE jump "
                            << "table: AddrReg def is not alsl.d/add.d\n");
          break;
        }

        MCInst *const Def1 = findRegDef(UDChain, AddOp1Reg.get(), *AddrDef);
        MCInst *const Def2 = findRegDef(UDChain, AddOp2Reg.get(), *AddrDef);
        Reg Tmp;
        const bool Op1IsSlli =
            Def1 && matchInst(*Def1, LoongArch::SLLI_D, Reg(AddOp1Reg.get()),
                              Tmp, Imm(ExpectedShift));
        const bool Op2IsSlli =
            Def2 && matchInst(*Def2, LoongArch::SLLI_D, Reg(AddOp2Reg.get()),
                              Tmp, Imm(ExpectedShift));
        if (Op1IsSlli == Op2IsSlli) {
          LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match GCC non-PIE jump "
                            << "table: can't identify scaled index\n");
          break;
        }

        IdxReg = Tmp.get();
        BaseReg = Op1IsSlli ? AddOp2Reg.get() : AddOp1Reg.get();
      } else {
        IdxReg = IdxRegObj.get();
        BaseReg = BaseRegObj.get();
      }

      MCInst *const BaseDef = findRegDef(UDChain, BaseReg, *AddrDef);
      if (!BaseDef) {
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match GCC non-PIE jump "
                          << "table: BaseReg has no local def\n");
        break;
      }

      if (!resolvePcRelBase(BaseDef, BaseReg, DispExpr, PCRelBaseOut)) {
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match GCC non-PIE jump "
                          << "table: can't resolve BaseReg\n");
        break;
      }

      MemLocInstr = Load;
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
      if (JirlRd.get() != LoongArch::R0) {
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match tail call: "
                          << "jirl writes return address\n");
        break;
      }

      MCInst *const Def = findRegDef(UDChain, JirlRj.get(), Instruction);
      if (!Def) {
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match tail call: "
                          << "JirlRj has no local def\n");
        break;
      }

      if (matchInst(*Def, LoongArch::PCADDI, Reg(JirlRj)) ||
          matchInst(*Def, LoongArch::PCADDU18I, Reg(JirlRj)))
        return IndirectBranchType::POSSIBLE_TAIL_CALL;

      Reg AddiSrc;
      if (!matchInst(*Def, LoongArch::ADDI_D, Reg(JirlRj), AddiSrc)) {
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match tail call: "
                          << "target materialization is not pcaddi/"
                          << "pcaddu18i/pcalau12i+addi.d\n");
        break;
      }

      MCInst *const BaseDef = findRegDef(UDChain, AddiSrc.get(), *Def);
      if (!BaseDef || !matchInst(*BaseDef, LoongArch::PCALAU12I, AddiSrc)) {
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match tail call: "
                          << "addi.d base is not pcalau12i\n");
        break;
      }

      return IndirectBranchType::POSSIBLE_TAIL_CALL;
    } while (0);

    LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match indirect branch: "
                      << "unknown pattern\n");
    return IndirectBranchType::UNKNOWN;
  }

  bool analyzeVirtualMethodCall(InstructionIterator Begin,
                                InstructionIterator End,
                                std::vector<MCInst *> &MethodFetchInsns,
                                unsigned &VtableRegNum, unsigned &MethodRegNum,
                                uint64_t &MethodOffset) const override {
    using namespace llvm::bolt::LowLevelInstMatcherDSL;

    VtableRegNum = LoongArch::NoRegister;
    MethodRegNum = LoongArch::NoRegister;
    MethodOffset = 0;

    assert(Begin != End && "empty instruction range");

    auto I = End;
    const MCInst &Jirl = *(--I);
    Reg JirlRd, JirlRj;
    Imm JirlImm;
    if (!matchInst(Jirl, LoongArch::JIRL, JirlRd, JirlRj, JirlImm) ||
        JirlImm.get() != 0) {
      LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match virtual method call: "
                        << "terminator is not zero-offset jirl\n");
      return false;
    }

    // Filter out returns.
    if (JirlRd.get() == LoongArch::R0 && JirlRj.get() == LoongArch::R1) {
      LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match virtual method call: "
                        << "return instruction\n");
      return false;
    }

    const DenseMap<const MCInst *, SmallVector<MCInst *>> UDChain =
        computeLocalUDChain(nullptr, Begin, End);

    // Path 1: LLVM/GCC fixed-slot virtual call
    //
    // Vptr        ld.d     $VtableReg, $ThisReg, 0
    //             # --- or ---
    //             ldptr.d  $VtableReg, $ThisReg, 0
    // Method      ld.d     $MethodReg, $VtableReg, MethodOffset
    //             # --- or ---
    //             ldptr.d  $MethodReg, $VtableReg, MethodOffset
    // Call        jirl     $zero, $MethodReg, 0
    //             # --- or ---
    //             jirl     $ra, $MethodReg, 0
    //
    // Cf.
    //   llvm/lib/Target/LoongArch/LoongArchISelLowering.cpp
    //     LoongArchTargetLowering::LowerCall
    //   llvm/lib/Target/LoongArch/LoongArchInstrInfo.td
    //     PseudoCALLIndirect, PseudoTAILIndirect,
    //     LdPat<load, LD_D, i64>, LDPTR_D load pattern
    //   gcc/config/loongarch/loongarch.md
    //     call_internal/call_value_internal,
    //     sibcall_internal/sibcall_value_internal
    do {
      MCInst *const Load = findRegDef(UDChain, JirlRj.get(), Jirl);
      if (!Load) {
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match virtual method "
                          << "call: method register has no local def\n");
        break;
      }

      Reg VtableReg;
      Imm VtableImm;
      if (!matchInst(*Load, LoongArch::LD_D, JirlRj, VtableReg, VtableImm) &&
          !matchInst(*Load, LoongArch::LDPTR_D, JirlRj, VtableReg, VtableImm)) {
        LLVM_DEBUG(dbgs() << "BOLT-DEBUG: failed to match virtual method "
                          << "call: method load is not fixed-slot ld.d/"
                          << "ldptr.d\n");
        break;
      }

      VtableRegNum = VtableReg.get();
      MethodRegNum = JirlRj.get();
      MethodOffset = VtableImm.get();
      MethodFetchInsns.push_back(Load);
      return true;
    } while (0);

    // TODO: indexed vtable-slot load
    //
    // Method      ldx.d  $MethodReg, $VtableReg, $IndexReg
    //
    // This needs MethodOffset recovery from $IndexReg before it is safe for
    // vtable ICP. Do not accept it until constant/scaled-index recovery is
    // implemented and tested.

    return false;
  }

  bool getJTLabelRef(const MCInst &IndJmp, InstructionIterator Begin,
                     InstructionIterator End, MCInst *&JTLoadInst,
                     const MCSymbol *&JTSymbol) override {
    using namespace llvm::bolt::LowLevelInstMatcherDSL;

    Reg JirlRd, JirlRj;
    if (!matchInst(IndJmp, LoongArch::JIRL, JirlRd, JirlRj, Skip())) {
      LLVM_DEBUG(dbgs() << "BOLT-DEBUG: getJTLabelRef: not a JIRL\n");
      return false;
    }

    // Filter out returns.
    if (JirlRd.get() == LoongArch::R0 && JirlRj.get() == LoongArch::R1) {
      LLVM_DEBUG(dbgs() << "BOLT-DEBUG: getJTLabelRef: JIRL is a return\n");
      return false;
    }

    const DenseMap<const MCInst *, SmallVector<MCInst *>> UDChain =
        computeLocalUDChain(&IndJmp, Begin, End);

    // Helper: given a register-defining instruction in a JT dispatch sequence,
    // find the pcaddi/pcalau12i base instruction and extract the JT symbol.
    const auto resolveBase = [&](MCInst *Def,
                                 MCRegister TargetReg) -> MCInst * {
      if (!Def)
        return nullptr;
      Expr DispExpr;
      if (matchInst(*Def, LoongArch::PCADDI, Reg(TargetReg), DispExpr)) {
        JTSymbol = getTargetSymbol(DispExpr.get());
        return Def;
      }
      Reg AddiSrc;
      if (matchInst(*Def, LoongArch::ADDI_D, Reg(TargetReg), AddiSrc)) {
        MCInst *const Pcalau = findRegDef(UDChain, AddiSrc.get(), *Def);
        if (Pcalau &&
            matchInst(*Pcalau, LoongArch::PCALAU12I, AddiSrc, DispExpr)) {
          JTSymbol = getTargetSymbol(DispExpr.get());
          return Pcalau;
        }
      }
      return nullptr;
    };

    // Note: See analyzeIndirectBranch() for complete descriptions of the
    // matching shapes below.

    // Path 1: LLVM non-PIE
    do {
      MCInst *const LdxD = findRegDef(UDChain, JirlRj.get(), IndJmp);
      if (!LdxD)
        break;
      Reg LdxBase, LdxIndex;
      if (!matchInst(*LdxD, LoongArch::LDX_D, Reg(), LdxBase, LdxIndex))
        break;
      MCInst *const BaseDef = findRegDef(UDChain, LdxBase.get(), *LdxD);
      JTLoadInst = resolveBase(BaseDef, LdxBase.get());
      if (JTLoadInst)
        return true;
    } while (0);

    // Path 2: LLVM PIE
    do {
      MCInst *AddD = findRegDef(UDChain, JirlRj.get(), IndJmp);
      if (!AddD)
        break;
      Reg AddOp1, AddOp2;
      if (!matchInst(*AddD, LoongArch::ADD_D, Reg(), AddOp1, AddOp2))
        break;
      Reg LdxRdReg, LdxBaseReg, LdxIndexReg;
      MCInst *LdxW = findRegDef(UDChain, AddOp1.get(), *AddD);
      if (!LdxW || !matchInst(*LdxW, LoongArch::LDX_W, LdxRdReg, LdxBaseReg,
                              LdxIndexReg)) {
        LdxW = findRegDef(UDChain, AddOp2.get(), *AddD);
        if (!LdxW || !matchInst(*LdxW, LoongArch::LDX_W, LdxRdReg, LdxBaseReg,
                                LdxIndexReg))
          break;
      }
      const MCRegister AddBase =
          (AddOp1.get() == LdxRdReg.get()) ? AddOp2.get() : AddOp1.get();
      MCInst *const BaseDef = findRegDef(UDChain, AddBase, *AddD);
      JTLoadInst = resolveBase(BaseDef, AddBase);
      if (JTLoadInst)
        return true;
    } while (0);

    // Path 3: GCC non-PIE
    do {
      MCInst *const Load = findRegDef(UDChain, JirlRj.get(), IndJmp);
      if (!Load)
        break;
      Reg AddrReg;
      if (!matchInst(*Load, LoongArch::LD_D, Reg(), AddrReg) &&
          !matchInst(*Load, LoongArch::LDPTR_D, Reg(), AddrReg))
        break;
      MCInst *const AddrDef = findRegDef(UDChain, AddrReg.get(), *Load);
      if (!AddrDef)
        break;
      MCRegister BaseReg;
      Reg BaseRegObj, IdxRegObj;
      if (!matchInst(*AddrDef, LoongArch::ALSL_D, Reg(), IdxRegObj, BaseRegObj,
                     Imm(3))) {
        Reg AddOp1Reg, AddOp2Reg, Tmp;
        if (!matchInst(*AddrDef, LoongArch::ADD_D, Reg(), AddOp1Reg, AddOp2Reg))
          break;
        MCInst *const Def1 = findRegDef(UDChain, AddOp1Reg.get(), *AddrDef);
        MCInst *const Def2 = findRegDef(UDChain, AddOp2Reg.get(), *AddrDef);
        const bool Op1IsSlli =
            Def1 && matchInst(*Def1, LoongArch::SLLI_D, Reg(AddOp1Reg.get()),
                              Tmp, Imm(3));
        const bool Op2IsSlli =
            Def2 && matchInst(*Def2, LoongArch::SLLI_D, Reg(AddOp2Reg.get()),
                              Tmp, Imm(3));
        if (Op1IsSlli == Op2IsSlli)
          break;
        BaseReg = Op1IsSlli ? AddOp2Reg.get() : AddOp1Reg.get();
      } else {
        BaseReg = BaseRegObj.get();
      }
      MCInst *const BaseDef = findRegDef(UDChain, BaseReg, *AddrDef);
      JTLoadInst = resolveBase(BaseDef, BaseReg);
      if (JTLoadInst)
        return true;
    } while (0);

    return false;
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
    case LoongArch::ADDI_W:  // addi.w $rd, $rj, imm
    case LoongArch::ADDI_D:  // addi.d $rd, $rj, imm
    case LoongArch::LD_B:    // ld.b $rd, $rj, imm
    case LoongArch::LD_BU:   // ld.bu $rd, $rj, imm
    case LoongArch::LD_H:    // ld.h $rd, $rj, imm
    case LoongArch::LD_HU:   // ld.hu $rd, $rj, imm
    case LoongArch::LD_W:    // ld.w $rd, $rj, imm
    case LoongArch::LD_WU:   // ld.wu $rd, $rj, imm
    case LoongArch::LD_D:    // ld.d $rd, $rj, imm
    case LoongArch::LDPTR_W: // ldptr.w $rd, $rj, imm
    case LoongArch::LDPTR_D: // ldptr.d $rd, $rj, imm
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
    using namespace llvm::bolt::LowLevelInstMatcherDSL;
    return matchInst(Inst, LoongArch::BREAK, Imm(0));
  }

  StringRef getTrapFillValue() const override {
    return StringRef("\x00\x00\x2a\x00", 4);
  }

  bool evaluateMemOperandTarget(const MCInst &Inst, uint64_t &Target,
                                uint64_t Address = 0,
                                uint64_t Size = 0) const override {
    (void)Inst;
    (void)Target;
    (void)Address;
    (void)Size;
    return false;
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
    case LoongArch::LU12I_W:
    case LoongArch::PCADDI:
    case LoongArch::PCADDU18I:
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
    if (auto *const SpecifierExpr = dyn_cast<MCSpecifierExpr>(Expr))
      if (SpecifierExpr->getSubExpr())
        return getTargetSymbol(SpecifierExpr->getSubExpr());

    auto *const BinExpr = dyn_cast<MCBinaryExpr>(Expr);
    if (BinExpr)
      return getTargetSymbol(BinExpr->getLHS());

    return MCPlusBuilder::getTargetSymbol(Expr);
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
    using namespace llvm::bolt::LowLevelInstMatcherDSL;

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
      Imm PCRelImm;
      CHECK_(matchInst(PCRelInst, LoongArch::PCADDU12I, Reg(LoongArch::R15),
                       PCRelImm));
      PCRelOffset = BeginPC + (PCRelImm.get() << 12);

      CHECK_(I != End);
      const auto &LdInst = *I++;
      Imm LdImm;
      CHECK_(matchInst(LdInst, LoongArch::LD_D, Reg(LoongArch::R15),
                       Reg(LoongArch::R15), LdImm) ||
             matchInst(LdInst, LoongArch::LD_W, Reg(LoongArch::R15),
                       Reg(LoongArch::R15), LdImm));
      LdOffset = LdImm.get();

      CHECK_(I != End);
      const auto &JirlInst = *I++;
      CHECK_(matchInst(JirlInst, LoongArch::JIRL, Reg(LoongArch::R13),
                       Reg(LoongArch::R15)));

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
      int64_t PCRelOffset, LdOffset;
      auto I = Begin;

      CHECK_(I != End);
      const auto &PCRelInst = *I++;
      Imm PCRelImm;
      CHECK_(matchInst(PCRelInst, LoongArch::PCALAU12I, Reg(LoongArch::R15),
                       PCRelImm));
      PCRelOffset = (BeginPC + (PCRelImm.get() << 12)) & ~0xfffULL;

      CHECK_(I != End);
      const auto &LdInst = *I++;
      Imm LdImm;
      CHECK_(matchInst(LdInst, LoongArch::LD_D, Reg(LoongArch::R15),
                       Reg(LoongArch::R15), LdImm) ||
             matchInst(LdInst, LoongArch::LD_W, Reg(LoongArch::R15),
                       Reg(LoongArch::R15), LdImm));
      LdOffset = LdImm.get();

      CHECK_(I != End);
      const auto &JirlInst = *I++;
      CHECK_(matchInst(JirlInst, LoongArch::JIRL, Reg(LoongArch::R13),
                       Reg(LoongArch::R15)));

      CHECK_(I != End);
      const auto &BreakInst = *I++;
      CHECK_(matchInst(BreakInst, LoongArch::BREAK));

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

  bool isCleanRegXOR(const MCInst &Inst) const override {
    using namespace llvm::bolt::LowLevelInstMatcherDSL;
    Reg Rd;
    // Get rd first.
    if (!matchInst(Inst, LoongArch::XOR, Rd)) {
      return false;
    }
    // Then see if every operand is rd.
    return matchInst(Inst, LoongArch::XOR, Rd, Rd, Rd);
  }

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

  BitVector getRegsUsedAsParams() const override {
    BitVector Regs = BitVector(RegInfo->getNumRegs(), false);
    Regs.set(LoongArch::R4);
    Regs.set(LoongArch::R5);
    Regs.set(LoongArch::R6);
    Regs.set(LoongArch::R7);
    Regs.set(LoongArch::R8);
    Regs.set(LoongArch::R9);
    Regs.set(LoongArch::R10);
    Regs.set(LoongArch::R11);
    return Regs;
  }

  void getDefaultDefIn(BitVector &Regs) const override {
    assert(Regs.size() >= RegInfo->getNumRegs() &&
           "The size of BitVector is less than RegInfo->getNumRegs().");
    Regs.set(LoongArch::R4);
    Regs.set(LoongArch::R5);
    Regs.set(LoongArch::R6);
    Regs.set(LoongArch::R7);
    Regs.set(LoongArch::R8);
    Regs.set(LoongArch::R9);
    Regs.set(LoongArch::R10);
    Regs.set(LoongArch::R11);
  }

  void getDefaultLiveOut(BitVector &Regs) const override {
    assert(Regs.size() >= RegInfo->getNumRegs() &&
           "The size of BitVector is less than RegInfo->getNumRegs().");
    Regs.set(LoongArch::R4);
    Regs.set(LoongArch::R5);
  }

  void getGPRegs(BitVector &Regs, bool IncludeAlias) const override {
    // LoongArch has no aliases for GPRs.
    (void)IncludeAlias;
    Regs.set(LoongArch::R0);
    Regs.set(LoongArch::R1);
    Regs.set(LoongArch::R2);
    Regs.set(LoongArch::R3);
    Regs.set(LoongArch::R4);
    Regs.set(LoongArch::R5);
    Regs.set(LoongArch::R6);
    Regs.set(LoongArch::R7);
    Regs.set(LoongArch::R8);
    Regs.set(LoongArch::R9);
    Regs.set(LoongArch::R10);
    Regs.set(LoongArch::R11);
    Regs.set(LoongArch::R12);
    Regs.set(LoongArch::R13);
    Regs.set(LoongArch::R14);
    Regs.set(LoongArch::R15);
    Regs.set(LoongArch::R16);
    Regs.set(LoongArch::R17);
    Regs.set(LoongArch::R18);
    Regs.set(LoongArch::R19);
    Regs.set(LoongArch::R20);
    Regs.set(LoongArch::R21);
    Regs.set(LoongArch::R22);
    Regs.set(LoongArch::R23);
    Regs.set(LoongArch::R24);
    Regs.set(LoongArch::R25);
    Regs.set(LoongArch::R26);
    Regs.set(LoongArch::R27);
    Regs.set(LoongArch::R28);
    Regs.set(LoongArch::R29);
    Regs.set(LoongArch::R30);
    Regs.set(LoongArch::R31);
  }

  std::optional<Relocation>
  createRelocation(const MCFixup &Fixup,
                   const MCAsmBackend &MAB) const override {
    const uint64_t RelOffset = Fixup.getOffset();

    uint32_t RelType;
    switch (Fixup.getKind()) {
    default:
      // TODO: Need more consideration. Refs to x86 or AArch64.
      return std::nullopt;
    case MCFixupKind(LoongArch::fixup_loongarch_b16):
      RelType = ELF::R_LARCH_B16;
      break;
    case MCFixupKind(LoongArch::fixup_loongarch_b21):
      RelType = ELF::R_LARCH_B21;
      break;
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
    // <https://searchfox.org/firefox-main/rev/13c921de977264c9684ce23b0bfc46578babda18/js/src/jit/loong64/MacroAssembler-loong64.cpp#196-253>

    const int64_t SImm = static_cast<int64_t>(Imm);

    InstructionListType Insts;

    if (isInt<12>(SImm)) {
      Insts.emplace_back(MCInstBuilder(LoongArch::ADDI_D)
                             .addReg(Dest)
                             .addReg(LoongArch::R0)
                             .addImm(SImm));
      return Insts;
    }
    if (isUInt<12>(SImm)) {
      Insts.emplace_back(MCInstBuilder(LoongArch::ORI)
                             .addReg(Dest)
                             .addReg(LoongArch::R0)
                             .addImm(SImm));
      return Insts;
    }

    const uint32_t Bits11To0 = Imm & 0xFFF;
    const uint32_t Bits31To12 = (Imm >> 12) & 0xFFFFF;
    const uint32_t Bits51To32 = (Imm >> 32) & 0xFFFFF;
    const uint32_t Bits63To52 = (Imm >> 52) & 0xFFF;

    // The mnemonics of these imm-generating instructions may be a bit
    // misleading. From
    // <https://loongson.github.io/LoongArch-Documentation/LoongArch-Vol1-EN.html#_lu12i_w_lu32i_d_lu52i_d>:
    //
    // * LU12I.W: GR[rd] = SignExtend({si20, 12'b0}, GRLEN)
    //   Build from 0 and imm
    // * LU32I.D: GR[rd] = {SignExtend(si20, 32), GR[rd][31:0]}
    //   Concat(!) rd and imm
    // * LU52I.D: GR[rd] = {si12, GR[rj][51:0]}
    //   Concat rj(!), imm and store to rd
    //
    // These matter when building large numbers.

    if (isInt<32>(SImm)) {
      Insts.emplace_back(
          MCInstBuilder(LoongArch::LU12I_W).addReg(Dest).addImm(Bits31To12));
    } else if (isUInt<32>(Imm)) {
      Insts.emplace_back(
          MCInstBuilder(LoongArch::LU12I_W).addReg(Dest).addImm(Bits31To12));
      if ((Bits31To12 >> 19) & 1)
        Insts.emplace_back(MCInstBuilder(LoongArch::BSTRINS_D)
                               .addReg(Dest)
                               .addReg(LoongArch::R0)
                               .addImm(63)
                               .addImm(32));
    } else if (isInt<52>(SImm)) {
      Insts.emplace_back(
          MCInstBuilder(LoongArch::LU12I_W).addReg(Dest).addImm(Bits31To12));
      Insts.emplace_back(MCInstBuilder(LoongArch::LU32I_D)
                             .addReg(Dest)
                             .addReg(Dest)
                             .addImm(Bits51To32));
    } else if (isUInt<52>(Imm)) {
      Insts.emplace_back(
          MCInstBuilder(LoongArch::LU12I_W).addReg(Dest).addImm(Bits31To12));
      Insts.emplace_back(MCInstBuilder(LoongArch::LU32I_D)
                             .addReg(Dest)
                             .addReg(Dest)
                             .addImm(Bits51To32));
      Insts.emplace_back(MCInstBuilder(LoongArch::BSTRINS_D)
                             .addReg(Dest)
                             .addReg(LoongArch::R0)
                             .addImm(63)
                             .addImm(52));
    } else if (Bits31To12 == 0 && Bits51To32 == 0) {
      Insts.emplace_back(MCInstBuilder(LoongArch::LU52I_D)
                             .addReg(Dest)
                             .addReg(LoongArch::R0)
                             .addImm(Bits63To52));
    } else if (Bits31To12 != 0 && ((Bits31To12 >> 19) & 1) == 0 &&
               Bits51To32 == 0) {
      Insts.emplace_back(
          MCInstBuilder(LoongArch::LU12I_W).addReg(Dest).addImm(Bits31To12));
      Insts.emplace_back(MCInstBuilder(LoongArch::LU52I_D)
                             .addReg(Dest)
                             .addReg(Dest)
                             .addImm(Bits63To52));
    } else {
      Insts.emplace_back(
          MCInstBuilder(LoongArch::LU12I_W).addReg(Dest).addImm(Bits31To12));
      Insts.emplace_back(MCInstBuilder(LoongArch::LU32I_D)
                             .addReg(Dest)
                             .addReg(Dest)
                             .addImm(Bits51To32));
      Insts.emplace_back(MCInstBuilder(LoongArch::LU52I_D)
                             .addReg(Dest)
                             .addReg(Dest)
                             .addImm(Bits63To52));
    }

    if (Bits11To0 != 0)
      Insts.emplace_back(MCInstBuilder(LoongArch::ORI)
                             .addReg(Dest)
                             .addReg(Dest)
                             .addImm(Bits11To0));
    return Insts;
  }

  bool replaceMemOperandWithImm(MCInst &Inst, InstructionListType &NewInsts,
                                StringRef ConstantData,
                                uint64_t Offset) const override {
    if (Inst.getNumOperands() < 1 || !Inst.getOperand(0).isReg()) {
      LLVM_DEBUG(
          dbgs()
          << "BOLT-DEBUG: replaceMemOperandWithImm called with insane Inst\n");
      return false;
    }

    const MCRegister DestReg = Inst.getOperand(0).getReg();

    unsigned DataSize;
    switch (Inst.getOpcode()) {
    case LoongArch::LD_B:
    case LoongArch::LD_BU:
      DataSize = 1;
      break;
    case LoongArch::LD_H:
    case LoongArch::LD_HU:
      DataSize = 2;
      break;
    case LoongArch::LD_W:
    case LoongArch::LD_WU:
    case LoongArch::LDPTR_W:
      DataSize = 4;
      break;
    case LoongArch::LD_D:
    case LoongArch::LDPTR_D:
      DataSize = 8;
      break;
    default:
      return false;
    }
    const bool IsUnsigned = Inst.getOpcode() == LoongArch::LD_BU ||
                            Inst.getOpcode() == LoongArch::LD_HU ||
                            Inst.getOpcode() == LoongArch::LD_WU;

    if (Offset + DataSize > ConstantData.size()) {
      LLVM_DEBUG(dbgs() << "BOLT-DEBUG: replaceMemOperandWithImm called with "
                           "invalid offset for given constant data\n");
      return false;
    }

    if (IsUnsigned) {
      const uint64_t ImmVal =
          DataExtractor(ConstantData, true, 8).getUnsigned(&Offset, DataSize);
      NewInsts = createLoadImmediate(DestReg, ImmVal);
    } else {
      const int64_t ImmVal =
          DataExtractor(ConstantData, true, 8).getSigned(&Offset, DataSize);
      NewInsts = createLoadImmediate(DestReg, static_cast<uint64_t>(ImmVal));
    }
    return true;
  }

  BlocksVectorTy indirectCallPromotion(
      const MCInst &CallInst,
      const std::vector<std::pair<MCSymbol *, uint64_t>> &Targets,
      const std::vector<std::pair<MCSymbol *, uint64_t>> &VtableSyms,
      const std::vector<MCInst *> &MethodFetchInsns,
      const bool MinimizeCodeSize, MCContext *Ctx) override {
    (void)MinimizeCodeSize;

    BlocksVectorTy Results;

    if (CallInst.getOpcode() != LoongArch::JIRL ||
        CallInst.getNumOperands() < 2 || !CallInst.getOperand(0).isReg() ||
        !CallInst.getOperand(1).isReg())
      return Results;

    const bool IsTailCall = isTailCall(CallInst);
    const bool IsJumpTable = getJumpTable(CallInst) != 0;
    const bool LoadElim = !VtableSyms.empty();
    assert((!LoadElim || VtableSyms.size() == Targets.size()) &&
           "There must be a vtable entry for every method in the targets "
           "vector.");

    if (LoadElim && (MethodFetchInsns.empty() ||
                     MethodFetchInsns.back()->getNumOperands() < 2 ||
                     !MethodFetchInsns.back()->getOperand(1).isReg()))
      return Results;

    const bool IsKnownCall =
        !IsJumpTable &&
        (IsTailCall || CallInst.getOperand(0).getReg() == LoongArch::R1);

    const MCPhysReg TargetReg = CallInst.getOperand(1).getReg();
    const MCPhysReg CompareReg =
        LoadElim ? static_cast<MCPhysReg>(
                       MethodFetchInsns.back()->getOperand(1).getReg())
                 : TargetReg;
    // Use $t8 only if we're absolutely sure it's a call. Otherwise, we have to
    // use ABI-reserved $r21 for a safe temp.
    const MCPhysReg TempReg = IsKnownCall && CompareReg != LoongArch::R20
                                  ? LoongArch::R20
                                  : LoongArch::R21;

    // Label for the current code block.
    MCSymbol *NextTarget = nullptr;

    // The join block which contains all the instructions following CallInst.
    // MergeBlock remains null if CallInst is a tail call.
    MCSymbol *MergeBlock = nullptr;

    const auto appendBranchToMerge = [&](InstructionListType &NewCall) {
      assert(MergeBlock);
      NewCall.emplace_back();
      createUncondBranch(NewCall.back(), MergeBlock, Ctx);
    };

    for (unsigned I = 0; I < Targets.size(); ++I) {
      if (IsJumpTable && !Targets[I].first)
        return BlocksVectorTy();

      Results.emplace_back(NextTarget, InstructionListType());
      InstructionListType *NewCall = &Results.back().second;

      InstructionListType Addr;
      if (LoadElim)
        Addr = materializeAddress(VtableSyms[I].first, Ctx, TempReg,
                                  VtableSyms[I].second);
      else if (Targets[I].first)
        Addr = materializeAddress(Targets[I].first, Ctx, TempReg);
      else
        Addr = createLoadImmediate(TempReg, Targets[I].second);
      NewCall->insert(NewCall->end(), Addr.cbegin(), Addr.cend());

      NextTarget = Ctx->createNamedTempSymbol();
      NewCall->push_back(
          MCInstBuilder(IsJumpTable ? LoongArch::BEQ : LoongArch::BNE)
              .addReg(CompareReg)
              .addReg(TempReg)
              .addExpr(MCSymbolRefExpr::create(
                  IsJumpTable ? Targets[I].first : NextTarget, *Ctx)));

      if (IsJumpTable)
        continue;

      Results.emplace_back(Ctx->createNamedTempSymbol(), InstructionListType());
      NewCall = &Results.back().second;
      NewCall->emplace_back();
      if (Targets[I].first)
        createDirectCall(NewCall->back(), Targets[I].first, Ctx, IsTailCall);
      else
        createIndirectCallInst(NewCall->back(), IsTailCall, TempReg, 0);

      if (std::optional<uint32_t> Offset = getOffset(CallInst))
        setOffset(NewCall->back(), *Offset);

      if (!IsTailCall) {
        if (I == 0)
          MergeBlock = Ctx->createNamedTempSymbol();
        else
          appendBranchToMerge(*NewCall);
      }
    }

    // Cold call block.
    Results.emplace_back(NextTarget, InstructionListType());
    InstructionListType &NewCall = Results.back().second;
    for (const MCInst *Inst : MethodFetchInsns)
      if (Inst != &CallInst)
        NewCall.push_back(*Inst);
    NewCall.push_back(CallInst);

    if (!IsTailCall && !IsJumpTable) {
      appendBranchToMerge(NewCall);
      // Record merge block
      Results.emplace_back(MergeBlock, InstructionListType());
    }

    return Results;
  }

  BlocksVectorTy jumpTablePromotion(
      const MCInst &IJmpInst,
      const std::vector<std::pair<MCSymbol *, uint64_t>> &Targets,
      const std::vector<MCInst *> &TargetFetchInsns,
      MCContext *Ctx) const override {
    assert(getJumpTable(IJmpInst) != 0);

    if (IJmpInst.getNumOperands() < 2 || !IJmpInst.getOperand(1).isReg())
      return BlocksVectorTy();

    const MCPhysReg IndexReg = getJumpTableIndexReg(IJmpInst);
    const MCPhysReg TargetReg = IJmpInst.getOperand(1).getReg();
    // Since this is a jump rather than a call, we have to use ABI-reserved $r21
    // for a safe temp.
    const MCPhysReg TempReg = LoongArch::R21;

    BlocksVectorTy Results;

    // Label for the current code block.
    MCSymbol *NextTarget = nullptr;

    for (unsigned I = 0; I < Targets.size(); ++I) {
      if (!Targets[I].first)
        return BlocksVectorTy();

      Results.emplace_back(NextTarget, InstructionListType());
      InstructionListType *const CurBB = &Results.back().second;

      // Load the index.
      const InstructionListType IndexSeq =
          IndexReg != LoongArch::NoRegister
              ? createLoadImmediate(TempReg, Targets[I].second)
              : materializeAddress(Targets[I].first, Ctx, TempReg);
      CurBB->insert(CurBB->end(), IndexSeq.cbegin(), IndexSeq.cend());

      // Compare current index to a specific index.
      NextTarget = Ctx->createNamedTempSymbol();
      CurBB->push_back(
          MCInstBuilder(LoongArch::BEQ)
              .addReg(IndexReg != LoongArch::NoRegister ? IndexReg : TargetReg)
              .addReg(TempReg)
              .addExpr(MCSymbolRefExpr::create(Targets[I].first, *Ctx)));
    }

    // Cold call block.
    Results.emplace_back(NextTarget, InstructionListType());
    InstructionListType &CurBB = Results.back().second;
    for (const MCInst *Inst : TargetFetchInsns)
      if (Inst != &IJmpInst)
        CurBB.push_back(*Inst);

    CurBB.push_back(IJmpInst);

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
    using namespace llvm::bolt::LowLevelInstMatcherDSL;
    return matchInst(Inst, LoongArch::LDPTR_D, Skip(), Reg(LoongArch::R3)) ||
           matchInst(Inst, LoongArch::LD_D, Skip(), Reg(LoongArch::R3));
  }

  bool isStackPtrStore(const MCInst &Inst) const {
    using namespace llvm::bolt::LowLevelInstMatcherDSL;
    return matchInst(Inst, LoongArch::STPTR_D, Skip(), Reg(LoongArch::R3)) ||
           matchInst(Inst, LoongArch::ST_D, Skip(), Reg(LoongArch::R3));
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
    if (const auto *E = dyn_cast<MCSpecifierExpr>(Expr)) {
      if (E->getSpecifier() == ELF::R_LARCH_PCREL20_S2)
        return E->getSubExpr();
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

  DenseMap<const MCInst *, SmallVector<MCInst *>>
  computeLocalUDChain(const MCInst *CurInstr, InstructionIterator Begin,
                      InstructionIterator End) const {
    DenseMap<int, MCInst *> RegAliasTable;
    DenseMap<const MCInst *, SmallVector<MCInst *>> Uses;

    auto addInstrOperands = [&](const MCInst &Instr) {
      // Update Uses table
      for (const MCOperand &Operand : MCPlus::primeOperands(Instr)) {
        if (!Operand.isReg())
          continue;
        unsigned Reg = Operand.getReg();
        MCInst *AliasInst = RegAliasTable[Reg];
        Uses[&Instr].push_back(AliasInst);
        LLVM_DEBUG({
          dbgs() << "Adding reg operand " << Reg << " refs ";
          if (AliasInst != nullptr)
            AliasInst->dump();
          else
            dbgs() << "\n";
        });
      }
    };

    LLVM_DEBUG(dbgs() << "computeLocalUDChain\n");
    bool TerminatorSeen = false;
    for (auto II = Begin; II != End; ++II) {
      MCInst &Instr = *II;
      // Ignore nops and CFIs
      if (isPseudo(Instr) || isNoop(Instr))
        continue;
      if (TerminatorSeen) {
        RegAliasTable.clear();
        Uses.clear();
      }

      LLVM_DEBUG(dbgs() << "Now updating for:\n ");
      LLVM_DEBUG(Instr.dump());
      addInstrOperands(Instr);

      BitVector Regs = BitVector(RegInfo->getNumRegs(), false);
      getWrittenRegs(Instr, Regs);

      // Update register definitions after this point
      for (int Idx : Regs.set_bits()) {
        RegAliasTable[Idx] = &Instr;
        LLVM_DEBUG(dbgs() << "Setting reg " << Idx
                          << " def to current instr.\n");
      }

      TerminatorSeen = isTerminator(Instr);
    }

    // Process the last instruction, which is not currently added into the
    // instruction stream
    if (CurInstr)
      addInstrOperands(*CurInstr);

    return Uses;
  }

  MCInst *
  findRegDef(const DenseMap<const MCInst *, SmallVector<MCInst *>> &UDChain,
             MCRegister Reg, const MCInst &StartInst) const {
    auto Uses = UDChain.find(&StartInst);
    if (Uses == UDChain.end())
      return nullptr;

    unsigned RegOpNo = 0;
    for (const MCOperand &Operand : MCPlus::primeOperands(StartInst)) {
      if (!Operand.isReg())
        continue;
      if (Operand.getReg() == Reg)
        return RegOpNo < Uses->second.size() ? Uses->second[RegOpNo] : nullptr;
      ++RegOpNo;
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
