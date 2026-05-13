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

  const MCExpr *Expr = MCSymbolRefExpr::create(AdjustedRel->Symbol, *BC.Ctx);
  if (AdjustedRel->Addend)
    Expr = MCBinaryExpr::createAdd(
        Expr, MCConstantExpr::create(AdjustedRel->Addend, *BC.Ctx), *BC.Ctx);
  Inst.addOperand(MCOperand::createExpr(
      BC.MIB->getTargetExprFor(Inst, Expr, *BC.Ctx, AdjustedRel->Type)));
  return true;
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
  // Relaxed by lld's LoongArch::tlsIeToLe (R_RELAX_TLS_GD_TO_LE).
  if (Rel.Type == ELF::R_LARCH_TLS_IE_PC_LO12) {
    switch (Inst.getOpcode()) {
    default:
      return std::nullopt; // May be relaxed, can't adjust anyway
    case LoongArch::LD_D:
    case LoongArch::LD_W:
      break;
    }
  }
  // Relaxed by lld's LoongArch::tlsdescToIe / tlsdescToLe.
  if (Rel.Type == ELF::R_LARCH_TLS_DESC_PC_HI20) {
    switch (Inst.getOpcode()) {
    default:
      return std::nullopt; // May be relaxed, can't adjust anyway
    case LoongArch::PCALAU12I:
      break;
    }
  }
  // Relaxed by lld's LoongArch::tlsdescToIe / tlsdescToLe.
  if (Rel.Type == ELF::R_LARCH_TLS_DESC_PC_LO12) {
    switch (Inst.getOpcode()) {
    default:
      return std::nullopt; // May be relaxed, can't adjust anyway
    case LoongArch::ADDI_D:
    case LoongArch::ADDI_W:
      break;
    }
  }
  // Relaxed by lld's LoongArch::tlsdescToIe / tlsdescToLe.
  if (Rel.Type == ELF::R_LARCH_TLS_DESC_PCREL20_S2) {
    switch (Inst.getOpcode()) {
    default:
      return std::nullopt; // May be relaxed, can't adjust anyway
    case LoongArch::PCADDI:
      break;
    }
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
