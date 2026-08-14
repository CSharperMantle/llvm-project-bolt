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
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/CommandLine.h"
#include <cstdint>
#include <limits>
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

/// Compute the register effects reaching each program point without preserving
/// instruction identity.
/// If \p Def is true, this computes a forward dataflow equation to
/// propagate reaching definitions.
/// If false, this computes a backward dataflow equation propagating
/// uses to their definitions.
template <bool Def = false>
class RegReachingDefOrUse
    : public DataflowAnalysis<RegReachingDefOrUse<Def>, BitVector, !Def> {
  using Parent = DataflowAnalysis<RegReachingDefOrUse<Def>, BitVector, !Def>;
  friend Parent;
  friend class DataflowInfoManager;

  using RegSetKey = SmallVector<unsigned, 4>;

  struct InstTransferInfo {
    unsigned ClobberSet;
    std::optional<unsigned> GenClass;
  };

public:
  RegReachingDefOrUse(const RegAnalysis &RA, BinaryFunction &BF,
                      MCPlusBuilder::AllocatorIdTy AllocId = 0)
      : Parent(BF, AllocId), RA(RA) {}
  virtual ~RegReachingDefOrUse() {}

  /// Return true if any class in \p Candidates affects \p Reg.
  bool isReachedBy(MCPhysReg Reg, const BitVector &Candidates) const {
    assert(Candidates.size() == Classes.size());
    for (int Idx = Candidates.find_first(); Idx != -1;
         Idx = Candidates.find_next(Idx)) {
      if (Classes[Idx][Reg])
        return true;
    }
    return false;
  }

  void run();

protected:
  uint64_t getNumTrackedOccurrences() const { return NumTrackedOccurrences; }
  size_t getNumClasses() const { return Classes.size(); }

  const RegAnalysis &RA;

  /// Complete register semantics for each bit in the dataflow state.
  SmallVector<BitVector, 0> Classes;

  /// One lookup supplies both kill and optional gen behavior for a point.
  DenseMap<const MCInst *, InstTransferInfo> InstToTransfer;

  /// Exact class bits killed by each distinct complete clobber set.
  SmallVector<BitVector, 0> KillSets;

  uint64_t NumTrackedOccurrences{0};

  static RegSetKey makeRegSetKey(const BitVector &Regs) {
    RegSetKey Key;
    for (int Reg = Regs.find_first(); Reg != -1; Reg = Regs.find_next(Reg))
      Key.emplace_back(static_cast<unsigned>(Reg));
    return Key;
  }

  static unsigned internRegSet(const BitVector &Regs,
                               DenseMap<RegSetKey, unsigned> &Ids,
                               SmallVectorImpl<BitVector> &Sets) {
    assert(Sets.size() < std::numeric_limits<unsigned>::max());
    const unsigned NewId = static_cast<unsigned>(Sets.size());
    const auto [It, Inserted] = Ids.try_emplace(makeRegSetKey(Regs), NewId);
    if (Inserted)
      Sets.emplace_back(Regs);
    return It->second;
  }

  void preflight() {
    const unsigned NumRegs = this->BC.MRI->getNumRegs();

    DenseMap<RegSetKey, unsigned> ClassIds;
    DenseMap<RegSetKey, unsigned> ClobberIds;
    SmallVector<BitVector> ClobberSets;

    for (BinaryBasicBlock &BB : this->Func) {
      for (MCInst &Inst : BB) {
        BitVector Clobbers(NumRegs);
        RA.getInstClobberList(Inst, Clobbers);
        const unsigned ClobberSet =
            internRegSet(Clobbers, ClobberIds, ClobberSets);

        const auto [TransferIt, Inserted] = InstToTransfer.try_emplace(
            &Inst, InstTransferInfo{ClobberSet, std::nullopt});
        assert(Inserted && "duplicate instruction in register dataflow");

        if (this->BC.MIB->isCFI(Inst))
          continue;

        const BitVector *Generated = &Clobbers;
        BitVector Touched;
        if constexpr (!Def) {
          Touched.resize(NumRegs);
          this->BC.MIB->getTouchedRegs(Inst, Touched);
          Generated = &Touched;
        }

        // Empty effects cannot satisfy an isReachedBy() query and therefore do
        // not need a class bit.
        if (Generated->none())
          continue;

        TransferIt->second.GenClass =
            internRegSet(*Generated, ClassIds, Classes);
        ++NumTrackedOccurrences;
      }
    }

    KillSets.assign(ClobberSets.size(), BitVector(Classes.size()));
    BitVector Scratch(NumRegs);
    for (const auto [ClobberIndex, Clobber] : llvm::enumerate(ClobberSets)) {
      BitVector &KillSet = KillSets[ClobberIndex];
      for (const auto [ClassIndex, Class] : llvm::enumerate(Classes)) {
        Scratch = Class;
        Scratch.reset(Clobber);
        if (Scratch.none())
          KillSet.set(ClassIndex);
      }
    }
  }

  BitVector getStartingStateAtBB(const BinaryBasicBlock &BB) {
    return BitVector(Classes.size(), false);
  }

  BitVector getStartingStateAtPoint(const MCInst &Point) {
    return BitVector(Classes.size(), false);
  }

  void doConfluence(BitVector &StateOut, const BitVector &StateIn) {
    StateOut |= StateIn;
  }

  BitVector computeNext(const MCInst &Point, const BitVector &Cur) {
    assert(Cur.size() == Classes.size());
    BitVector Next = Cur;

    const auto TransferIt = InstToTransfer.find(&Point);
    assert(TransferIt != InstToTransfer.end() &&
           "missing register dataflow transfer");
    const InstTransferInfo &Transfer = TransferIt->second;
    assert(Transfer.ClobberSet < KillSets.size());
    Next.reset(KillSets[Transfer.ClobberSet]);

    // Preserve kill-before-gen ordering for RMW instructions.
    if (Transfer.GenClass)
      Next.set(*Transfer.GenClass);
    return Next;
  }

  StringRef getAnnotationName() const {
    if constexpr (Def)
      return StringRef("RegReachingDefs");
    return StringRef("RegReachingUses");
  }
};

} // end namespace bolt
} // end namespace llvm

#endif
