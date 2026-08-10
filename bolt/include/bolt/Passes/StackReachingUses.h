//===- bolt/Passes/StackReachingUses.h --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef BOLT_PASSES_STACKREACHINGUSES_H
#define BOLT_PASSES_STACKREACHINGUSES_H

#include "bolt/Passes/DataflowAnalysis.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/CommandLine.h"
#include <cstdint>
#include <optional>
#include <tuple>

namespace opts {
extern llvm::cl::opt<bool> TimeOpts;
}

namespace llvm {
namespace bolt {

class FrameAnalysis;
struct ArgAccesses;
struct FrameIndexEntry;

class StackReachingUses : public DataflowAnalysis<StackReachingUses, BitVector,
                                                  /*Backward=*/true> {
  using Parent = DataflowAnalysis<StackReachingUses, BitVector, true>;
  friend Parent;

public:
  StackReachingUses(const FrameAnalysis &FA, BinaryFunction &BF,
                    MCPlusBuilder::AllocatorIdTy AllocId = 0)
      : Parent(BF, AllocId), FA(FA) {}
  virtual ~StackReachingUses() {}

  /// Return true if the stack position written by the store in \p StoreFIE was
  /// later consumed by a load to a different register (not the same one used in
  /// the store). Useful for identifying loads/stores of callee-saved regs.
  bool isLoadedInDifferentReg(const FrameIndexEntry &StoreFIE,
                              const BitVector &Candidates) const;

  /// Answer whether the stack position written by the store represented in
  /// \p StoreFIE is loaded from or consumed in any way. The classes for all
  /// relevant uses reaching this store should be set in \p Candidates.
  /// If \p IncludeLocalAccesses is false, only consider whether a callee
  /// consumes this stack position.
  bool isStoreUsed(const FrameIndexEntry &StoreFIE, const BitVector &Candidates,
                   bool IncludeLocalAccesses = true) const;

  void run() { Parent::run(); }

protected:
  /// Interesting fields from FrameIndexEntry that determine the behavior of a
  /// tracked frame load.
  struct LoadClassInfo {
    int64_t StackOffset;
    int32_t RegOrImm;
    uint8_t Size;
    bool IsSimple;

    auto tie() const { return std::tie(StackOffset, RegOrImm, Size, IsSimple); }
    bool operator<(const LoadClassInfo &Other) const {
      return tie() < Other.tie();
    }
  };

  struct UseClassInfo {
    std::optional<LoadClassInfo> Load;
    /// Canonical FrameAnalysis-owned argument-use metadata, if any.
    const ArgAccesses *Args{nullptr};
  };

  // Reference to the result of stack frame analysis
  const FrameAnalysis &FA;

  /// Complete semantics for each bit in the dataflow state.
  SmallVector<UseClassInfo, 0> Classes;

  /// Map every tracked instruction occurrence to its use-class bit.
  DenseMap<const MCInst *, unsigned> InstToClass;

  void preflight();

  BitVector getStartingStateAtBB(const BinaryBasicBlock &BB) {
    return BitVector(Classes.size(), false);
  }

  BitVector getStartingStateAtPoint(const MCInst &Point) {
    return BitVector(Classes.size(), false);
  }

  void doConfluence(BitVector &StateOut, const BitVector &StateIn) {
    StateOut |= StateIn;
  }

  BitVector computeNext(const MCInst &Point, const BitVector &Cur);

  StringRef getAnnotationName() const { return StringRef("StackReachingUses"); }
};

} // end namespace bolt
} // end namespace llvm

#endif
