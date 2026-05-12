//===- bolt/Passes/LoongArchRelaxationPass.cpp ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "bolt/Passes/LoongArchRelaxationPass.h"
#include "bolt/Core/ParallelUtilities.h"

using namespace llvm;

namespace llvm {
namespace bolt {

void LoongArchRelaxationPass::runOnFunction(BinaryFunction &BF) {
  BinaryContext &BC = BF.getBinaryContext();
  constexpr unsigned TwoMB = 0x200000;

  for (BinaryBasicBlock &BB : BF) {
    for (auto II = BB.begin(); II != BB.end(); ++II) {
      MCInst &Inst = *II;
      if (!BC.MIB->tryGetLoongArchPCADDIPCRel20SubExpr(Inst))
        continue;

      const MCSymbol *Symbol = BC.MIB->getTargetSymbol(Inst);
      if (!Symbol)
        continue;

      if (BF.getSize() < TwoMB) {
        BinaryFunction *const TargetBF = BC.getFunctionForSymbol(Symbol);
        if (TargetBF == &BF && !BB.isSplit())
          continue;

        if (BinaryBasicBlock *const TargetBB = BF.getBasicBlockForLabel(Symbol))
          if (BB.getFragmentNum() == TargetBB->getFragmentNum())
            continue;
      }

      InstructionListType Replacement;
      {
        auto L = BC.scopeLock();
        Replacement =
            BC.MIB->undoLoongArchPCRel20Relaxation(Inst, BC.Ctx.get());
      }
      II = BB.replaceInstruction(II, Replacement);
    }
  }
}

Error LoongArchRelaxationPass::runOnFunctions(BinaryContext &BC) {
  if (!BC.isLoongArch() || !BC.HasRelocations)
    return Error::success();

  ParallelUtilities::WorkFuncTy WorkFun = [&](BinaryFunction &BF) {
    runOnFunction(BF);
  };

  ParallelUtilities::runOnEachFunction(
      BC, ParallelUtilities::SchedulingPolicy::SP_INST_LINEAR, WorkFun, nullptr,
      "LoongArchRelaxationPass");

  return Error::success();
}

} // namespace bolt
} // namespace llvm
