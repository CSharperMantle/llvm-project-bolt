//===- bolt/Passes/ReachingDefOrUse.h ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef BOLT_PASSES_REACHINGDEFORUSE_H
#define BOLT_PASSES_REACHINGDEFORUSE_H

#include "bolt/Passes/DataflowAnalysis.h"
#include "bolt/Passes/RegAnalysis.h"
#include "llvm/Support/CommandLine.h"
#include <optional>

namespace opts {
extern llvm::cl::opt<bool> TimeOpts;
}

namespace llvm {
namespace bolt {

/// If \p Def is true, this computes a forward dataflow equation to
/// propagate reaching definitions.
/// If false, this computes a backward dataflow equation propagating
/// uses to their definitions.
template <bool Def = false>
class ReachingDefOrUse
    : public InstrsDataflowAnalysis<ReachingDefOrUse<Def>, !Def> {
  friend class DataflowAnalysis<ReachingDefOrUse<Def>, BitVector, !Def>;

public:
  ReachingDefOrUse(const RegAnalysis &RA, BinaryFunction &BF,
                   std::optional<MCPhysReg> TrackingReg = std::nullopt,
                   MCPlusBuilder::AllocatorIdTy AllocId = 0)
      : InstrsDataflowAnalysis<ReachingDefOrUse<Def>, !Def>(BF, AllocId),
        RA(RA), TrackingReg(TrackingReg) {}
  virtual ~ReachingDefOrUse() {}

  bool isReachedBy(MCPhysReg Reg, ExprIterator Candidates) {
    for (auto I = Candidates; I != this->expr_end(); ++I) {
      const int Idx = I.getBitVectorIndex();
      const BitVector &BV = Def ? ClobberSets[Idx] : TouchedSets[Idx];
      if (BV[Reg])
        return true;
    }
    return false;
  }

  bool doesAReachesB(const MCInst &A, const MCInst &B) {
    return (*this->getStateAt(B))[this->ExprToIdx[&A]];
  }

  void run() { InstrsDataflowAnalysis<ReachingDefOrUse<Def>, !Def>::run(); }

protected:
  /// Reference to the result of reg analysis
  const RegAnalysis &RA;

  /// If set, limit the dataflow to only track instructions affecting this
  /// register. Otherwise the analysis can be too permissive.
  std::optional<MCPhysReg> TrackingReg;

  /// Per-instruction cache of RA.getInstClobberList() indexed by ExprToIdx.
  SmallVector<BitVector, 0> ClobberSets;
  /// Per-instruction cache of MIB->getTouchedRegs() indexed by ExprToIdx.
  SmallVector<BitVector, 0> TouchedSets;
  /// Per-instruction cache of RA.getInstUsedRegsList() indexed by ExprToIdx.
  SmallVector<BitVector, 0> UsedSets;

  /// Scratch buffers reused across doesXKillsY() calls to avoid per-call
  /// BitVector allocation.
  BitVector ScratchX;
  BitVector ScratchY;

  void preflight() {
    // Populate our universe of tracked expressions with all instructions
    // except pseudos
    for (BinaryBasicBlock &BB : this->Func) {
      for (MCInst &Inst : BB) {
        this->Expressions.push_back(&Inst);
        this->ExprToIdx[&Inst] = this->NumInstrs++;
      }
    }
    // Precompute per-instruction register sets.
    const unsigned NumRegs = this->BC.MRI->getNumRegs();
    ClobberSets.assign(this->NumInstrs, BitVector(NumRegs, false));
    TouchedSets.assign(this->NumInstrs, BitVector(NumRegs, false));
    UsedSets.assign(this->NumInstrs, BitVector(NumRegs, false));
    ScratchX.resize(NumRegs, false);
    ScratchY.resize(NumRegs, false);
    for (const BinaryBasicBlock &BB : this->Func) {
      for (const MCInst &Inst : BB) {
        const uint64_t Idx = this->ExprToIdx[&Inst];
        RA.getInstClobberList(Inst, ClobberSets[Idx]);
        this->BC.MIB->getTouchedRegs(Inst, TouchedSets[Idx]);
        RA.getInstUsedRegsList(Inst, UsedSets[Idx], false);
      }
    }
  }

  BitVector getStartingStateAtBB(const BinaryBasicBlock &BB) {
    return BitVector(this->NumInstrs, false);
  }

  BitVector getStartingStateAtPoint(const MCInst &Point) {
    return BitVector(this->NumInstrs, false);
  }

  void doConfluence(BitVector &StateOut, const BitVector &StateIn) {
    StateOut |= StateIn;
  }

  /// Define the function computing the kill set -- whether expression Y, a
  /// tracked expression, will be considered to be dead after executing X.
  bool doesXKillsY(uint64_t XIdx, uint64_t YIdx) {

    // getClobberedRegs for X and Y. If they intersect, return true
    const BitVector &XClobbers = ClobberSets[XIdx];
    // In defs, write after write -> kills first write
    // In uses, write after access (read or write) -> kills access
    const BitVector &YClobbers = Def ? ClobberSets[YIdx] : TouchedSets[YIdx];

    ScratchX.reset();
    ScratchX |= XClobbers;
    ScratchY.reset();
    ScratchY |= YClobbers;
    // Limit the analysis, if requested
    if (TrackingReg) {
      const BitVector &Filter = this->BC.MIB->getAliases(*TrackingReg);
      ScratchX &= Filter;
      ScratchY &= Filter;
    }
    // X kills Y if it clobbers Y completely -- this is a conservative approach.
    // In practice, we may produce use-def links that may not exist.
    ScratchX &= ScratchY;
    return ScratchX == ScratchY;
  }

  BitVector computeNext(const MCInst &Point, const BitVector &Cur) {
    BitVector Next = Cur;
    const uint64_t XIdx = this->ExprToIdx[&Point];
    // Kill
    for (auto I = this->expr_begin(Next), E = this->expr_end(); I != E; ++I) {
      assert(*I != nullptr && "Lost pointers");
      const uint64_t YIdx = I.getBitVectorIndex();
      if (doesXKillsY(XIdx, YIdx)) {
        Next.reset(YIdx);
      }
    }
    // Gen
    if (!this->BC.MIB->isCFI(Point)) {
      if (TrackingReg == std::nullopt) {
        // Track all instructions
        Next.set(XIdx);
      } else {
        // Track only instructions relevant to TrackingReg.
        const BitVector &Regs = Def ? ClobberSets[XIdx] : UsedSets[XIdx];
        if (Regs.anyCommon(this->BC.MIB->getAliases(*TrackingReg)))
          Next.set(XIdx);
      }
    }
    return Next;
  }

  StringRef getAnnotationName() const {
    if (Def)
      return StringRef("ReachingDefs");
    return StringRef("ReachingUses");
  }
};

} // end namespace bolt
} // end namespace llvm

#endif
