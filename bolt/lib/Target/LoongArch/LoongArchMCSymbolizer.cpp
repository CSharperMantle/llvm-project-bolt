//===- bolt/Target/LoongArch/LoongArchMCSymbolizer.cpp --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LoongArchMCSymbolizer.h"
#include "MCTargetDesc/LoongArchMCTargetDesc.h"
#include "bolt/Core/BinaryContext.h"
#include "bolt/Core/MCPlusBuilder.h"
#include "bolt/Core/Relocation.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCInst.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"

#define DEBUG_TYPE "bolt-symbolizer"

namespace llvm {
namespace bolt {

LoongArchMCSymbolizer::~LoongArchMCSymbolizer() {}

bool LoongArchMCSymbolizer::tryAddingSymbolicOperand(
    MCInst &Inst, raw_ostream &CStream, int64_t Value, uint64_t InstAddress,
    bool IsBranch, uint64_t ImmOffset, uint64_t ImmSize, uint64_t InstSize) {
  BinaryContext &BC = Function.getBinaryContext();

  if (BC.MIB->isBranch(Inst) || BC.MIB->isCall(Inst))
    return false;

  const uint64_t InstOffset = InstAddress - Function.getAddress();
  const Relocation *Rel = Function.getRelocationAt(InstOffset);
  if (!Rel)
    return false;

  std::optional<Relocation> AdjustedRel = adjustRelocation(*Rel, Inst);
  if (!AdjustedRel) {
    LLVM_DEBUG(dbgs() << "BOLT-DEBUG: ignoring relocation at 0x"
                      << Twine::utohexstr(InstAddress) << '\n');
    return false;
  }

  const MCSymbol *Symbol = AdjustedRel->Symbol;
  uint64_t Addend = AdjustedRel->Addend;

  const MCExpr *Expr = MCSymbolRefExpr::create(Symbol, *BC.Ctx);
  if (Addend)
    Expr = MCBinaryExpr::createAdd(
        Expr, MCConstantExpr::create(Addend, *BC.Ctx), *BC.Ctx);
  Inst.addOperand(MCOperand::createExpr(
      BC.MIB->getTargetExprFor(Inst, Expr, *BC.Ctx, AdjustedRel->Type)));
  return true;
}

bool LoongArchMCSymbolizer::isRelaxedGOTToDirectPair(
    const Relocation &Rel) const {
  const Relocation *HiRel = nullptr;
  const Relocation *LoRel = nullptr;
  if (Rel.Type == ELF::R_LARCH_GOT_PC_HI20) {
    HiRel = &Rel;
    LoRel = Function.getRelocationAt(Rel.Offset + 4);
  } else if (Rel.Type == ELF::R_LARCH_GOT_PC_LO12 && Rel.Offset >= 4) {
    HiRel = Function.getRelocationAt(Rel.Offset - 4);
    LoRel = &Rel;
  } else {
    return false;
  }

  if (!HiRel || !LoRel || HiRel->Type != ELF::R_LARCH_GOT_PC_HI20 ||
      LoRel->Type != ELF::R_LARCH_GOT_PC_LO12 || !HiRel->Symbol ||
      HiRel->Symbol != LoRel->Symbol || HiRel->Addend != LoRel->Addend)
    return false;

  BinaryContext &BC = Function.getBinaryContext();
  const ErrorOr<uint64_t> SymbolValue = BC.getSymbolValue(*HiRel->Symbol);
  if (!SymbolValue)
    return false;

  // R_LARCH_PCALA_HI20 uses the rounded address page, while
  // R_LARCH_PCALA_LO12 contributes a signed 12-bit offset. If the values
  // extracted from the stale GOT relocations match this decomposition of S+A,
  // the linked instructions materialize S+A directly instead of GOT+G.
  const uint64_t Target = *SymbolValue + HiRel->Addend;
  const uint64_t DirectHi = (Target + 0x800) & ~0xfffULL;
  const uint64_t DirectLo = SignExtend64<12>(Target & 0xfff);
  return HiRel->Value == DirectHi && LoRel->Value == DirectLo;
}

std::optional<Relocation>
LoongArchMCSymbolizer::adjustRelocation(const Relocation &Rel,
                                        const MCInst &Inst) const {
  BinaryContext &BC = Function.getBinaryContext();
  Relocation AdjustedRel = Rel;

  // The linker might perform TLS relocations relaxations, thus changing the
  // instructions. The static relocations might be invalid at this point and we
  // don't have to process these relocations anymore. More information could be
  // found by searching "tlsIeToLe" in lld.
  if (Rel.Type == ELF::R_LARCH_TLS_IE_PC_HI20) {
    switch (Inst.getOpcode()) {
    default:
      return std::nullopt; // May be relaxed, can't adjust anyway
    case LoongArch::PCALAU12I:
      break;
    }
  }
  if (Rel.Type == ELF::R_LARCH_TLS_IE_PC_LO12) {
    switch (Inst.getOpcode()) {
    default:
      return std::nullopt; // May be relaxed, can't adjust anyway
    case LoongArch::LD_D:
    case LoongArch::LD_W:
      break;
    }
  }

  // Some linkers can relax a GOT reference into direct address
  // materialization without updating the relocation types.
  if (isRelaxedGOTToDirectPair(Rel)) {
    switch (Rel.Type) {
    case ELF::R_LARCH_GOT_PC_HI20:
      AdjustedRel.Type = ELF::R_LARCH_PCALA_HI20;
      break;
    case ELF::R_LARCH_GOT_PC_LO12:
      AdjustedRel.Type = ELF::R_LARCH_PCALA_LO12;
      break;
    default:
      llvm_unreachable("reloc misclassified by isRelaxedGOTToDirectPair()");
      break;
    }
    return AdjustedRel;
  }

  if (Relocation::isGOT(Rel.Type)) {
    AdjustedRel.Symbol = BC.registerNameAtAddress("__BOLT_got_zero", 0, 0, 0);
    AdjustedRel.Addend = Rel.Value;
  }

  return AdjustedRel;
}

void LoongArchMCSymbolizer::tryAddingPcLoadReferenceComment(
    raw_ostream &CStream, int64_t Value, uint64_t Address) {}

} // namespace bolt
} // namespace llvm
