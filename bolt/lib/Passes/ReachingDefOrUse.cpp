//===- bolt/Passes/ReachingDefOrUse.cpp -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file supplies various implementation of {,Reg}ReachingDefOrUse
// class methods.
//
//===----------------------------------------------------------------------===//

#include "bolt/Passes/ReachingDefOrUse.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "reaching-def-or-use"

using namespace llvm;

namespace llvm {
namespace bolt {

template <bool Def> void RegReachingDefOrUse<Def>::run() {
  Parent::run();
  LLVM_DEBUG({
    if constexpr (Def) {
      dbgs() << "RegReachingDefs classes for \"" << this->Func.getPrintName()
             << "\": " << getNumTrackedOccurrences() << " tracked occurrences, "
             << getNumClasses() << " classes\n";
    } else {
      dbgs() << "RegReachingUses classes for \"" << this->Func.getPrintName()
             << "\": " << getNumTrackedOccurrences() << " tracked occurrences, "
             << getNumClasses() << " classes\n";
    }
  });
}

template void RegReachingDefOrUse<true>::run();
template void RegReachingDefOrUse<false>::run();

} // end namespace bolt
} // end namespace llvm
