//===- bolt/Passes/CFAOriginAnalysis.h -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef BOLT_PASSES_CFAORIGINANALYSIS_H
#define BOLT_PASSES_CFAORIGINANALYSIS_H

#include "bolt/Passes/DataflowAnalysis.h"

namespace llvm {
namespace bolt {

/// Identify the function entry whose CFA reaches a program point. Distinct
/// entries remain distinct even when their numerical CFA rules are identical.
class CFAOriginState {
public:
  enum class Kind : uint8_t { Empty, Concrete, Superposition };

private:
  Kind StateKind{Kind::Empty};
  const BinaryBasicBlock *Origin{nullptr};

  CFAOriginState(Kind StateKind, const BinaryBasicBlock *Origin)
      : StateKind(StateKind), Origin(Origin) {}

public:
  CFAOriginState() = default;

  static CFAOriginState getEmpty() { return CFAOriginState(); }

  static CFAOriginState getConcrete(const BinaryBasicBlock &Entry) {
    return CFAOriginState(Kind::Concrete, &Entry);
  }

  static CFAOriginState getSuperposition() {
    return CFAOriginState(Kind::Superposition, nullptr);
  }

  bool isEmpty() const { return StateKind == Kind::Empty; }
  bool isConcrete() const { return StateKind == Kind::Concrete; }
  bool isSuperposition() const { return StateKind == Kind::Superposition; }

  const BinaryBasicBlock *getOrigin() const {
    assert(isConcrete() && "CFA origin is not concrete");
    return Origin;
  }

  bool operator==(const CFAOriginState &Other) const {
    return StateKind == Other.StateKind && Origin == Other.Origin;
  }

  bool operator!=(const CFAOriginState &Other) const {
    return !(*this == Other);
  }
};

inline raw_ostream &operator<<(raw_ostream &OS, const CFAOriginState &State) {
  if (State.isEmpty())
    return OS << "empty";
  if (State.isSuperposition())
    return OS << "superposition";
  return OS << State.getOrigin()->getName();
}

/// Propagate CFA origins through the CFG and detect points reached from more
/// than one function entry.
class CFAOriginAnalysis
    : public DataflowAnalysis<CFAOriginAnalysis, CFAOriginState> {
  friend class DataflowAnalysis<CFAOriginAnalysis, CFAOriginState>;

  void preflight() {}

  CFAOriginState getStartingStateAtBB(const BinaryBasicBlock &BB) {
    if (BB.isEntryPoint())
      return CFAOriginState::getConcrete(BB);
    return CFAOriginState::getEmpty();
  }

  CFAOriginState getStartingStateAtPoint(const MCInst &) {
    return CFAOriginState::getEmpty();
  }

  void doConfluence(CFAOriginState &StateOut, const CFAOriginState &StateIn) {
    if (StateIn.isEmpty())
      return;

    if (StateOut.isEmpty()) {
      StateOut = StateIn;
      return;
    }

    if (StateOut.isSuperposition() || StateIn.isSuperposition() ||
        StateOut.getOrigin() != StateIn.getOrigin())
      StateOut = CFAOriginState::getSuperposition();
  }

  CFAOriginState computeNext(const MCInst &, const CFAOriginState &State) {
    return State;
  }

  StringRef getAnnotationName() const { return "CFAOriginAnalysis"; }

public:
  CFAOriginAnalysis(BinaryFunction &BF,
                    MCPlusBuilder::AllocatorIdTy AllocatorId = 0)
      : DataflowAnalysis<CFAOriginAnalysis, CFAOriginState>(BF, AllocatorId) {}

  void run() { DataflowAnalysis<CFAOriginAnalysis, CFAOriginState>::run(); }
};

} // namespace bolt
} // namespace llvm

#endif
