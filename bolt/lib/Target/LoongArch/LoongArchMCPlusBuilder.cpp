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
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
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
           Inst.getOperand(1).getReg() == LoongArch::R0 &&
           Inst.getOperand(2).isImm() && Inst.getOperand(2).getImm() == 0;
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
    return IndirectBranchType::UNKNOWN;
  }

  bool convertJmpToTailCall(MCInst &Inst) override {
    if (isTailCall(Inst))
      return false;

    setTailCall(Inst);
    return true;
  }

  void createReturn(MCInst &Inst) const override {
    Inst.setOpcode(LoongArch::JIRL);
    Inst.clear();
    Inst.addOperand(MCOperand::createReg(LoongArch::R0));
    Inst.addOperand(MCOperand::createReg(LoongArch::R1));
    Inst.addOperand(MCOperand::createImm(0));
  }

  void createNoop(MCInst &Inst) const override {
    Inst.setOpcode(LoongArch::ANDI);
    Inst.clear();
    Inst.addOperand(MCOperand::createReg(LoongArch::R0));
    Inst.addOperand(MCOperand::createReg(LoongArch::R0));
    Inst.addOperand(MCOperand::createImm(0));
  }

  void createUncondBranch(MCInst &Inst, const MCSymbol *TBB,
                          MCContext *Ctx) const override {
    Inst.setOpcode(LoongArch::B);
    Inst.clear();
    Inst.addOperand(MCOperand::createExpr(MCSymbolRefExpr::create(TBB, *Ctx)));
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
    Insts[0].setOpcode(LoongArch::PCADDU18I);
    Insts[0].clear();
    Insts[0].addOperand(MCOperand::createReg(LoongArch::R21));
    Insts[0].addOperand(MCOperand::createExpr(LoongArchMCExpr::create(
        MCSymbolRefExpr::create(Target, *Ctx), ELF::R_LARCH_CALL36, *Ctx)));

    // jirl $r0, $r21, 0
    Insts[1].setOpcode(LoongArch::JIRL);
    Insts[1].clear();
    Insts[1].addOperand(MCOperand::createReg(LoongArch::R0));
    Insts[1].addOperand(MCOperand::createReg(LoongArch::R21));
    Insts[1].addOperand(MCOperand::createImm(0));

    if (IsTailCall)
      setTailCall(Insts[1]);

    Seq.swap(Insts);
  }

  void createLongJmp(InstructionListType &Seq, const MCSymbol *Target,
                     MCContext *Ctx, bool IsTailCall) override {
    InstructionListType Insts(5);

    // lu12i.w  $r21, %abs_hi20(target)           # bits 31-12
    Insts[0].setOpcode(LoongArch::LU12I_W);
    Insts[0].clear();
    Insts[0].addOperand(MCOperand::createReg(LoongArch::R21));
    Insts[0].addOperand(MCOperand::createExpr(LoongArchMCExpr::create(
        MCSymbolRefExpr::create(Target, *Ctx), ELF::R_LARCH_ABS_HI20, *Ctx)));

    // ori      $r21, $r21, %abs_lo12(target)     # bits 11-0
    Insts[1].setOpcode(LoongArch::ORI);
    Insts[1].clear();
    Insts[1].addOperand(MCOperand::createReg(LoongArch::R21));
    Insts[1].addOperand(MCOperand::createReg(LoongArch::R21));
    Insts[1].addOperand(MCOperand::createExpr(LoongArchMCExpr::create(
        MCSymbolRefExpr::create(Target, *Ctx), ELF::R_LARCH_ABS_LO12, *Ctx)));

    // lu32i.d  $r21, %abs64_lo20(target)         # bits 51-32
    Insts[2].setOpcode(LoongArch::LU32I_D);
    Insts[2].clear();
    Insts[2].addOperand(MCOperand::createReg(LoongArch::R21));
    Insts[2].addOperand(MCOperand::createReg(LoongArch::R21));
    Insts[2].addOperand(MCOperand::createExpr(LoongArchMCExpr::create(
        MCSymbolRefExpr::create(Target, *Ctx), ELF::R_LARCH_ABS64_LO20, *Ctx)));

    // lu52i.d  $r21, $r21, %abs64_hi12(target)   # bits 63-52
    Insts[3].setOpcode(LoongArch::LU52I_D);
    Insts[3].clear();
    Insts[3].addOperand(MCOperand::createReg(LoongArch::R21));
    Insts[3].addOperand(MCOperand::createReg(LoongArch::R21));
    Insts[3].addOperand(MCOperand::createExpr(LoongArchMCExpr::create(
        MCSymbolRefExpr::create(Target, *Ctx), ELF::R_LARCH_ABS64_HI12, *Ctx)));

    // jirl     $r0, $r21, 0
    Insts[4].setOpcode(LoongArch::JIRL);
    Insts[4].clear();
    Insts[4].addOperand(MCOperand::createReg(LoongArch::R0));
    Insts[4].addOperand(MCOperand::createReg(LoongArch::R21));
    Insts[4].addOperand(MCOperand::createImm(0));

    if (IsTailCall)
      setTailCall(Insts[4]);

    Seq.swap(Insts);
  }

  void createStackPointerIncrement(
      MCInst &Inst, int Size = 8,
      bool NoFlagsClobber = false /* unused */) const override {
    Inst.setOpcode(LoongArch::ADDI_D);
    Inst.clear();
    Inst.addOperand(MCOperand::createReg(LoongArch::R3));
    Inst.addOperand(MCOperand::createReg(LoongArch::R3));
    Inst.addOperand(MCOperand::createImm(-Size));
  }

  void createStackPointerDecrement(
      MCInst &Inst, int Size = 8,
      bool NoFlagsClobber = false /* unused */) const override {
    Inst.setOpcode(LoongArch::ADDI_D);
    Inst.clear();
    Inst.addOperand(MCOperand::createReg(LoongArch::R3));
    Inst.addOperand(MCOperand::createReg(LoongArch::R3));
    Inst.addOperand(MCOperand::createImm(Size));
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
    Inst.clear();
    Inst.setOpcode(LoongArch::BREAK);
    Inst.addOperand(MCOperand::createImm(0));
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
    Insts[0].setOpcode(LoongArch::PCALAU12I);
    Insts[0].clear();
    Insts[0].addOperand(MCOperand::createReg(Reg));
    Insts[0].addOperand(MCOperand::createExpr(
        LoongArchMCExpr::create(SubExpr, ELF::R_LARCH_PCALA_HI20, *Ctx)));

    Insts[1].setOpcode(LoongArch::ADDI_D);
    Insts[1].clear();
    Insts[1].addOperand(MCOperand::createReg(Reg));
    Insts[1].addOperand(MCOperand::createReg(Reg));
    Insts[1].addOperand(MCOperand::createExpr(
        LoongArchMCExpr::create(SubExpr, ELF::R_LARCH_PCALA_LO12, *Ctx)));

    return Insts;
  }

  void createDirectCall(MCInst &Inst, const MCSymbol *Target, MCContext *Ctx,
                        bool IsTailCall) override {
    Inst.clear();
    Inst.setOpcode(IsTailCall ? LoongArch::B : LoongArch::BL);
    Inst.addOperand(
        MCOperand::createExpr(MCSymbolRefExpr::create(Target, *Ctx)));
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
    Insts[2].setOpcode(LoongArch::JIRL);
    Insts[2].clear();
    Insts[2].addOperand(
        MCOperand::createReg(IsTailCall ? LoongArch::R0 : LoongArch::R1));
    Insts[2].addOperand(MCOperand::createReg(LoongArch::R20));
    Insts[2].addOperand(MCOperand::createImm(0));
    moveAnnotations(std::move(DirectCall), Insts[2]);

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
  ///  Typical PLT entry looks like the following:
  ///
  ///    pcaddu12i    t3, 8(0x8)
  ///    ld.d         t3, t3, offset
  ///    jirl         t1, t3, 0
  ///    nop
  ///
  uint64_t analyzePLTEntry(MCInst &Instruction, InstructionIterator Begin,
                           InstructionIterator End,
                           uint64_t BeginPC) const override {
    auto I = Begin;

    assert(I != End);
    auto &PCADD = *I++;
    assert(PCADD.getOpcode() == LoongArch::PCADDU12I);
    assert(PCADD.getOperand(0).getReg() == LoongArch::R15);

    assert(I != End);
    auto &LD = *I++;
    assert(LD.getOpcode() == LoongArch::LD_D);
    assert(LD.getOperand(0).getReg() == LoongArch::R15);
    assert(LD.getOperand(1).getReg() == LoongArch::R15);

    assert(I != End);
    auto &JIRL = *I++;
    (void)JIRL;
    assert(JIRL.getOpcode() == LoongArch::JIRL);
    assert(JIRL.getOperand(0).getReg() == LoongArch::R13);
    assert(JIRL.getOperand(1).getReg() == LoongArch::R15);

    assert(I != End);
    auto &NOP = *I++;
    (void)NOP;
    assert(isNoop(NOP));

    assert(I == End);

    auto PCADDOffset = PCADD.getOperand(1).getImm() << 12;
    auto LDOffset = LD.getOperand(2).getImm();
    return BeginPC + PCADDOffset + LDOffset;
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
      return LoongArchMCExpr::create(Expr, ELF::R_LARCH_PCADD_HI20, Ctx);
    case ELF::R_LARCH_PCADD_LO12:
    case ELF::R_LARCH_GOT_PCADD_LO12:
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

protected:
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
