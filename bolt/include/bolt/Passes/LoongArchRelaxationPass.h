//===- bolt/Passes/LoongArchRelaxationPass.h --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the LoongArchRelaxationPass class, which expands
// linker-relaxed PCADDI instructions back to PCALAU12I + ADDI.D when BOLT may
// move their targets out of PCADDI range.
//
//===----------------------------------------------------------------------===//

#ifndef BOLT_PASSES_LOONGARCHRELAXATIONPASS_H
#define BOLT_PASSES_LOONGARCHRELAXATIONPASS_H

#include "bolt/Passes/BinaryPasses.h"

namespace llvm {
namespace bolt {

class LoongArchRelaxationPass : public BinaryFunctionPass {
  void runOnFunction(BinaryFunction &Function);

public:
  explicit LoongArchRelaxationPass(const cl::opt<bool> &PrintPass)
      : BinaryFunctionPass(PrintPass) {}

  const char *getName() const override { return "loongarch-relaxation"; }

  Error runOnFunctions(BinaryContext &BC) override;
};

} // namespace bolt
} // namespace llvm

#endif // BOLT_PASSES_LOONGARCHRELAXATIONPASS_H
