//===- bolt/Passes/Aligner.h - Pass for optimal code alignment --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the Aligner class, which provides
// alignment for code, e.g. basic block and functions, with the goal to achieve
// the optimal performance.
//
//===----------------------------------------------------------------------===//

#ifndef BOLT_PASSES_ALIGNER_H
#define BOLT_PASSES_ALIGNER_H

#include "bolt/Passes/BinaryPasses.h"
#include "llvm/Support/RWMutex.h"

namespace llvm {
namespace bolt {

class AlignerPass : public BinaryFunctionPass {
private:
  /// Stats for usage of max bytes for basic block alignment.
  std::vector<uint32_t> AlignHistogram;
  llvm::sys::RWMutex AlignHistogramMtx;

  /// Stats: execution count of blocks that were aligned.
  std::atomic<uint64_t> AlignedBlocksCount{0};

  /// Stats: number of unique natural-loop header candidates.
  std::atomic<uint64_t> NumHotLoopHeaderCandidates{0};

  /// Stats: number of hot-loop headers that pass every selection gate.
  std::atomic<uint64_t> NumHotLoopHeadersSelected{0};

  /// Assign alignment to hot natural-loop headers based on profile and final
  /// block layout.
  void alignHotLoopHeaders(BinaryFunction &Function,
                           const MCCodeEmitter *Emitter,
                           uint64_t HotThreshold);

  /// Assign alignment to basic blocks based on profile.
  void alignBlocks(BinaryFunction &Function, const MCCodeEmitter *Emitter);

public:
  explicit AlignerPass() : BinaryFunctionPass(false) {}

  const char *getName() const override { return "aligner"; }

  /// Pass entry point
  Error runOnFunctions(BinaryContext &BC) override;
};

} // namespace bolt
} // namespace llvm

#endif
