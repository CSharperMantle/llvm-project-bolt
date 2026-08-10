//===- bolt/Passes/StackReachingUses.cpp ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the StackReachingUses class.
//
//===----------------------------------------------------------------------===//

#include "bolt/Passes/StackReachingUses.h"
#include "bolt/Passes/FrameAnalysis.h"
#include <map>
#include <utility>

#define DEBUG_TYPE "sru"

namespace llvm {
namespace bolt {

namespace {

/// Compare argument-use metadata by fields rather than pointer identity.
struct ArgAccessesPtrLess {
  bool operator()(const ArgAccesses *const &LHS,
                  const ArgAccesses *const &RHS) const {
    if (LHS->AssumeEverything != RHS->AssumeEverything)
      return LHS->AssumeEverything < RHS->AssumeEverything;
    if (LHS->AssumeEverything)
      return false;
    return LHS->Set < RHS->Set;
  }
};

} // namespace

bool StackReachingUses::isLoadedInDifferentReg(
    const FrameIndexEntry &StoreFIE, const BitVector &Candidates) const {
  assert(Candidates.size() == Classes.size());
  for (int Idx = Candidates.find_first(); Idx != -1;
       Idx = Candidates.find_next(Idx)) {
    if (const std::optional<LoadClassInfo> &LCIY = Classes[Idx].Load) {
      if (StoreFIE.StackOffset + StoreFIE.Size > LCIY->StackOffset &&
          StoreFIE.StackOffset < LCIY->StackOffset + LCIY->Size &&
          StoreFIE.RegOrImm != LCIY->RegOrImm)
        return true;
    }
  }
  return false;
}

bool StackReachingUses::isStoreUsed(const FrameIndexEntry &StoreFIE,
                                    const BitVector &Candidates,
                                    bool IncludeLocalAccesses) const {
  assert(Candidates.size() == Classes.size());
  for (int Idx = Candidates.find_first(); Idx != -1;
       Idx = Candidates.find_next(Idx)) {
    const UseClassInfo &Class = Classes[Idx];
    if (IncludeLocalAccesses && Class.Load &&
        StoreFIE.StackOffset + StoreFIE.Size > Class.Load->StackOffset &&
        StoreFIE.StackOffset < Class.Load->StackOffset + Class.Load->Size)
      return true;

    if (!Class.Args)
      continue;
    if (Class.Args->AssumeEverything)
      return true;

    for (ArgInStackAccess Access : Class.Args->Set)
      if (StoreFIE.StackOffset + StoreFIE.Size > Access.StackOffset &&
          StoreFIE.StackOffset < Access.StackOffset + Access.Size)
        return true;
  }
  return false;
}

void StackReachingUses::preflight() {
  LLVM_DEBUG(dbgs() << "Starting StackReachingUses on \"" << Func.getPrintName()
                    << "\"\n");

  using UseClassKey =
      std::pair<std::optional<LoadClassInfo>, std::optional<unsigned>>;
  std::map<UseClassKey, unsigned> UseClasses;
  std::map<const ArgAccesses *, unsigned, ArgAccessesPtrLess> ArgClasses;

  // Populate the universe of observationally distinct stack uses. Every
  // tracked instruction remains in InstToClass, while equivalent occurrences
  // share one bit and one complete class descriptor.
  for (BinaryBasicBlock &BB : Func) {
    for (MCInst &Inst : BB) {
      std::optional<LoadClassInfo> Load;
      if (ErrorOr<const FrameIndexEntry &> FIE = FA.getFIEFor(Inst)) {
        if (FIE->IsLoad)
          Load = LoadClassInfo{FIE->StackOffset, FIE->RegOrImm, FIE->Size,
                               FIE->IsSimple};
      }

      const ArgAccesses *Args = nullptr;
      std::optional<unsigned> ArgClass;
      ErrorOr<const ArgAccesses &> AA = FA.getArgAccessesFor(Inst);
      if (AA && (!AA->Set.empty() || AA->AssumeEverything)) {
        const unsigned NewArgClass = static_cast<unsigned>(ArgClasses.size());
        const auto ArgIt = ArgClasses.try_emplace(&*AA, NewArgClass).first;
        std::tie(Args, ArgClass) = *ArgIt;
      }

      if (!Load && !Args)
        continue;

      const UseClassKey Key(Load, ArgClass);
      const unsigned NewClass = static_cast<unsigned>(Classes.size());
      const auto [ClassIt, Inserted] = UseClasses.try_emplace(Key, NewClass);
      InstToClass[&Inst] = ClassIt->second;
      if (Inserted)
        Classes.push_back(UseClassInfo{Load, Args});
    }
  }

  assert(Classes.size() == UseClasses.size());

  LLVM_DEBUG(dbgs() << "StackReachingUses classes for \"" << Func.getPrintName()
                    << "\": " << InstToClass.size() << " tracked occurrences, "
                    << Classes.size() << " classes\n");
}

BitVector StackReachingUses::computeNext(const MCInst &Point,
                                         const BitVector &Cur) {
  assert(Cur.size() == Classes.size());
  BitVector Next = Cur;
  // Kill. Decode the current instruction once and avoid scanning live classes
  // unless it is a simple frame store.
  ErrorOr<const FrameIndexEntry &> Store = FA.getFIEFor(Point);
  if (Store && Store->IsSimple && Store->IsStore) {
    for (int Idx = Next.find_first(); Idx != -1; Idx = Next.find_next(Idx)) {
      const std::optional<LoadClassInfo> &Load = Classes[Idx].Load;
      if (!Load || !Load->IsSimple)
        continue;
      if (Store->StackOffset <= Load->StackOffset &&
          Store->StackOffset + Store->Size >= Load->StackOffset + Load->Size) {
        LLVM_DEBUG(dbgs() << "\t\t\tKilling stack-use class " << Idx << "\n");
        Next.reset(Idx);
      }
    }
  }

  // Gen. An instruction with both load and argument-use semantics generates
  // the single class containing both components. Kill intentionally precedes
  // gen for read/modify/write instructions.
  const auto ClassIt = InstToClass.find(&Point);
  if (ClassIt != InstToClass.end())
    Next.set(ClassIt->second);
  return Next;
}

} // namespace bolt
} // namespace llvm
