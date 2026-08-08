//===- bolt/Passes/Instrumentation.cpp ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the Instrumentation class.
//
//===----------------------------------------------------------------------===//

#include "bolt/Passes/Instrumentation.h"
#include "bolt/Core/ParallelUtilities.h"
#include "bolt/RuntimeLibs/InstrumentationRuntimeLibrary.h"
#include "bolt/Utils/CommandLineOpts.h"
#include "bolt/Utils/Utils.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/RWMutex.h"
#include <queue>
#include <stack>
#include <unordered_set>

#define DEBUG_TYPE "bolt-instrumentation"

using namespace llvm;

namespace opts {
extern cl::OptionCategory BoltInstrCategory;

cl::opt<std::string> InstrumentationFilename(
    "instrumentation-file",
    cl::desc("file name where instrumented profile will be saved (default: "
             "/tmp/prof.fdata)"),
    cl::init("/tmp/prof.fdata"), cl::Optional, cl::cat(BoltInstrCategory));

cl::opt<std::string> InstrumentationBinpath(
    "instrumentation-binpath",
    cl::desc("path to instrumented binary in case if /proc/self/map_files "
             "is not accessible due to access restriction issues"),
    cl::Optional, cl::cat(BoltInstrCategory));

cl::opt<bool> InstrumentationFileAppendPID(
    "instrumentation-file-append-pid",
    cl::desc("append PID to saved profile file name (default: false)"),
    cl::init(false), cl::Optional, cl::cat(BoltInstrCategory));

cl::opt<bool> ConservativeInstrumentation(
    "conservative-instrumentation",
    cl::desc("disable instrumentation optimizations that sacrifice profile "
             "accuracy (for debugging, default: false)"),
    cl::init(false), cl::Optional, cl::cat(BoltInstrCategory));

cl::opt<uint32_t> InstrumentationMaxSize(
    "instrumentation-max-size",
    cl::desc("Set max memory size of the instrumentation bump allocator "
             "default: 0x6400000)"),
    cl::init(0x6400000), cl::Optional, cl::cat(BoltInstrCategory));

cl::opt<uint32_t> InstrumentationSleepTime(
    "instrumentation-sleep-time",
    cl::desc("interval between profile writes (default: 0 = write only at "
             "program end).  This is useful for service workloads when you "
             "want to dump profile every X minutes or if you are killing the "
             "program and the profile is not being dumped at the end."),
    cl::init(0), cl::Optional, cl::cat(BoltInstrCategory));

cl::opt<bool> InstrumentationNoCountersClear(
    "instrumentation-no-counters-clear",
    cl::desc("Don't clear counters across dumps "
             "(use with instrumentation-sleep-time option)"),
    cl::init(false), cl::Optional, cl::cat(BoltInstrCategory));

cl::opt<bool> InstrumentationWaitForks(
    "instrumentation-wait-forks",
    cl::desc("Wait until all forks of instrumented process will finish "
             "(use with instrumentation-sleep-time option)"),
    cl::init(false), cl::Optional, cl::cat(BoltInstrCategory));

cl::opt<bool>
    InstrumentHotOnly("instrument-hot-only",
                      cl::desc("only insert instrumentation on hot functions "
                               "(needs profile, default: false)"),
                      cl::init(false), cl::Optional,
                      cl::cat(BoltInstrCategory));

cl::opt<bool> InstrumentCalls("instrument-calls",
                              cl::desc("record profile for inter-function "
                                       "control flow activity (default: true)"),
                              cl::init(true), cl::Optional,
                              cl::cat(BoltInstrCategory));

cl::opt<bool> InstrumentLoadProfiles(
    "instrument-load-profiles",
    cl::desc("record load-target addresses for jump-table and vtable loads "
             "to enable jump-table ICP and vtable method-load elimination "
             "without LBR (default: false)"),
    cl::init(false), cl::Optional, cl::cat(BoltInstrCategory));
} // namespace opts

namespace llvm {
namespace bolt {

static bool hasAArch64ExclusiveMemop(
    BinaryFunction &Function,
    std::unordered_set<const BinaryBasicBlock *> &BBToSkip) {
  // FIXME ARMv8-a architecture reference manual says that software must avoid
  // having any explicit memory accesses between exclusive load and associated
  // store instruction. So for now skip instrumentation for basic blocks that
  // have these instructions, since it might lead to runtime deadlock.
  BinaryContext &BC = Function.getBinaryContext();
  std::queue<std::pair<BinaryBasicBlock *, bool>> BBQueue; // {BB, isLoad}
  std::unordered_set<BinaryBasicBlock *> Visited;

  if (Function.getLayout().block_begin() == Function.getLayout().block_end())
    return 0;

  BinaryBasicBlock *BBfirst = *Function.getLayout().block_begin();
  BBQueue.push({BBfirst, false});

  while (!BBQueue.empty()) {
    BinaryBasicBlock *BB = BBQueue.front().first;
    bool IsLoad = BBQueue.front().second;
    BBQueue.pop();
    if (!Visited.insert(BB).second)
      continue;

    for (const MCInst &Inst : *BB) {
      // Two loads one after another - skip whole function
      if (BC.MIB->isAArch64ExclusiveLoad(Inst) && IsLoad) {
        if (opts::Verbosity >= 2) {
          outs() << "BOLT-INSTRUMENTER: function " << Function.getPrintName()
                 << " has two exclusive loads. Ignoring the function.\n";
        }
        return true;
      }

      if (BC.MIB->isAArch64ExclusiveLoad(Inst))
        IsLoad = true;

      if (IsLoad && BBToSkip.insert(BB).second) {
        if (opts::Verbosity >= 2) {
          outs() << "BOLT-INSTRUMENTER: skip BB " << BB->getName()
                 << " due to exclusive instruction in function "
                 << Function.getPrintName() << "\n";
        }
      }

      if (!IsLoad && BC.MIB->isAArch64ExclusiveStore(Inst)) {
        if (opts::Verbosity >= 2) {
          outs() << "BOLT-INSTRUMENTER: function " << Function.getPrintName()
                 << " has exclusive store without corresponding load. Ignoring "
                    "the function.\n";
        }
        return true;
      }

      if (IsLoad && (BC.MIB->isAArch64ExclusiveStore(Inst) ||
                     BC.MIB->isAArch64ExclusiveClear(Inst)))
        IsLoad = false;
    }

    if (IsLoad && BB->succ_size() == 0) {
      if (opts::Verbosity >= 2) {
        outs()
            << "BOLT-INSTRUMENTER: function " << Function.getPrintName()
            << " has exclusive load in trailing BB. Ignoring the function.\n";
      }
      return true;
    }

    for (BinaryBasicBlock *BBS : BB->successors())
      BBQueue.push({BBS, IsLoad});
  }

  if (BBToSkip.size() == Visited.size()) {
    if (opts::Verbosity >= 2) {
      outs() << "BOLT-INSTRUMENTER: all BBs are marked with true. Ignoring the "
                "function "
             << Function.getPrintName() << "\n";
    }
    return true;
  }

  return false;
}

uint32_t Instrumentation::getFunctionNameIndex(const BinaryFunction &Function) {
  auto Iter = FuncToStringIdx.find(&Function);
  if (Iter != FuncToStringIdx.end())
    return Iter->second;
  size_t Idx = Summary->StringTable.size();
  FuncToStringIdx.emplace(std::make_pair(&Function, Idx));
  Summary->StringTable.append(getEscapedName(Function.getOneName()));
  Summary->StringTable.append(1, '\0');
  return Idx;
}

bool Instrumentation::createCallDescription(FunctionDescription &FuncDesc,
                                            const BinaryFunction &FromFunction,
                                            uint32_t From, uint32_t FromNodeID,
                                            const BinaryFunction &ToFunction,
                                            uint32_t To, bool IsInvoke) {
  CallDescription CD;
  // Ordinarily, we don't augment direct calls with an explicit counter, except
  // when forced to do so or when we know this callee could be throwing
  // exceptions, in which case there is no other way to accurately record its
  // frequency.
  bool ForceInstrumentation = opts::ConservativeInstrumentation || IsInvoke;
  CD.FromLoc.FuncString = getFunctionNameIndex(FromFunction);
  CD.FromLoc.Offset = From;
  CD.FromNode = FromNodeID;
  CD.Target = &ToFunction;
  CD.ToLoc.FuncString = getFunctionNameIndex(ToFunction);
  CD.ToLoc.Offset = To;
  CD.Counter = ForceInstrumentation ? Summary->Counters.size() : 0xffffffff;
  if (ForceInstrumentation)
    ++DirectCallCounters;
  FuncDesc.Calls.emplace_back(CD);
  return ForceInstrumentation;
}

void Instrumentation::createIndCallDescription(
    const BinaryFunction &FromFunction, uint32_t From) {
  IndCallDescription ICD;
  ICD.FromLoc.FuncString = getFunctionNameIndex(FromFunction);
  ICD.FromLoc.Offset = From;
  Summary->IndCallDescriptions.emplace_back(ICD);
}

void Instrumentation::createIndCallTargetDescription(
    const BinaryFunction &ToFunction, uint32_t To) {
  IndCallTargetDescription ICD;
  ICD.ToLoc.FuncString = getFunctionNameIndex(ToFunction);
  ICD.ToLoc.Offset = To;
  ICD.Target = &ToFunction;
  Summary->IndCallTargetDescriptions.emplace_back(ICD);
}

void Instrumentation::createLoadDescription(const BinaryFunction &FromFunction,
                                            uint32_t From) {
  LoadDescription LD;
  LD.FromLoc.FuncString = getFunctionNameIndex(FromFunction);
  LD.FromLoc.Offset = From;
  Summary->LoadDescriptions.emplace_back(LD);
}

bool Instrumentation::createEdgeDescription(FunctionDescription &FuncDesc,
                                            const BinaryFunction &FromFunction,
                                            uint32_t From, uint32_t FromNodeID,
                                            const BinaryFunction &ToFunction,
                                            uint32_t To, uint32_t ToNodeID,
                                            bool Instrumented) {
  EdgeDescription ED;
  auto Result = FuncDesc.EdgesSet.insert(std::make_pair(FromNodeID, ToNodeID));
  // Avoid creating duplicated edge descriptions. This happens in CFGs where a
  // block jumps to its fall-through.
  if (Result.second == false)
    return false;
  ED.FromLoc.FuncString = getFunctionNameIndex(FromFunction);
  ED.FromLoc.Offset = From;
  ED.FromNode = FromNodeID;
  ED.ToLoc.FuncString = getFunctionNameIndex(ToFunction);
  ED.ToLoc.Offset = To;
  ED.ToNode = ToNodeID;
  ED.Counter = Instrumented ? Summary->Counters.size() : 0xffffffff;
  if (Instrumented)
    ++BranchCounters;
  FuncDesc.Edges.emplace_back(ED);
  return Instrumented;
}

void Instrumentation::createLeafNodeDescription(FunctionDescription &FuncDesc,
                                                uint32_t Node) {
  InstrumentedNode IN;
  IN.Node = Node;
  IN.Counter = Summary->Counters.size();
  ++LeafNodeCounters;
  FuncDesc.LeafNodes.emplace_back(IN);
}

InstructionListType
Instrumentation::createInstrumentationSnippet(BinaryContext &BC, bool IsLeaf) {
  auto L = BC.scopeLock();
  MCSymbol *Label = BC.Ctx->createNamedTempSymbol("InstrEntry");
  Summary->Counters.emplace_back(Label);
  return BC.MIB->createInstrIncMemory(Label, BC.Ctx.get(), IsLeaf,
                                      BC.AsmInfo->getCodePointerSize());
}

// Helper instruction sequence insertion function
static BinaryBasicBlock::iterator
insertInstructions(InstructionListType &Instrs, BinaryBasicBlock &BB,
                   BinaryBasicBlock::iterator Iter) {
  for (MCInst &NewInst : Instrs) {
    Iter = BB.insertInstruction(Iter, NewInst);
    ++Iter;
  }
  return Iter;
}

void Instrumentation::instrumentLeafNode(BinaryBasicBlock &BB,
                                         BinaryBasicBlock::iterator Iter,
                                         bool IsLeaf,
                                         FunctionDescription &FuncDesc,
                                         uint32_t Node) {
  createLeafNodeDescription(FuncDesc, Node);
  InstructionListType CounterInstrs = createInstrumentationSnippet(
      BB.getFunction()->getBinaryContext(), IsLeaf);
  insertInstructions(CounterInstrs, BB, Iter);
}

void Instrumentation::instrumentIndirectTarget(BinaryBasicBlock &BB,
                                               BinaryBasicBlock::iterator &Iter,
                                               BinaryFunction &FromFunction,
                                               uint32_t From) {
  auto L = FromFunction.getBinaryContext().scopeLock();
  const size_t IndCallSiteID = Summary->IndCallDescriptions.size();
  createIndCallDescription(FromFunction, From);

  BinaryContext &BC = FromFunction.getBinaryContext();
  bool IsTailCall = BC.MIB->isTailCall(*Iter);
  InstructionListType CounterInstrs = BC.MIB->createInstrumentedIndirectCall(
      std::move(*Iter),
      IsTailCall ? IndTailCallHandlerExitBBFunction->getSymbol()
                 : IndCallHandlerExitBBFunction->getSymbol(),
      IndCallSiteID, &*BC.Ctx);

  Iter = BB.eraseInstruction(Iter);
  Iter = insertInstructions(CounterInstrs, BB, Iter);
  --Iter;
}

void Instrumentation::instrumentLoadTarget(BinaryBasicBlock &BB,
                                           BinaryBasicBlock::iterator &Iter,
                                           BinaryFunction &FromFunction,
                                           uint32_t LoadOffset) {
  MCInst LoadInst = *Iter;

  BinaryContext &BC = FromFunction.getBinaryContext();
  auto L = BC.scopeLock();
  const size_t LoadSiteID = Summary->LoadDescriptions.size();
  createLoadDescription(FromFunction, LoadOffset);

  InstructionListType Snippet = BC.MIB->createInstrumentedLoad(
      std::move(*Iter), LoadHandlerFunction->getSymbol(), LoadSiteID, BC);
  Iter = BB.eraseInstruction(Iter);
  Iter = insertInstructions(Snippet, BB, Iter);
  Iter = BB.insertInstruction(Iter, std::move(LoadInst));
}

bool Instrumentation::instrumentOneTarget(
    SplitWorklistTy &SplitWorklist, SplitInstrsTy &SplitInstrs,
    BinaryBasicBlock::iterator &Iter, BinaryFunction &FromFunction,
    BinaryBasicBlock &FromBB, uint32_t From, BinaryFunction &ToFunc,
    BinaryBasicBlock *TargetBB, uint32_t ToOffset, bool IsLeaf, bool IsInvoke,
    FunctionDescription *FuncDesc, uint32_t FromNodeID, uint32_t ToNodeID) {
  BinaryContext &BC = FromFunction.getBinaryContext();
  {
    auto L = BC.scopeLock();
    bool Created = true;
    if (!TargetBB)
      Created = createCallDescription(*FuncDesc, FromFunction, From, FromNodeID,
                                      ToFunc, ToOffset, IsInvoke);
    else
      Created = createEdgeDescription(*FuncDesc, FromFunction, From, FromNodeID,
                                      ToFunc, ToOffset, ToNodeID,
                                      /*Instrumented=*/true);
    if (!Created)
      return false;
  }

  InstructionListType CounterInstrs = createInstrumentationSnippet(BC, IsLeaf);

  const MCInst &Inst = *Iter;
  if (BC.MIB->isCall(Inst)) {
    // This code handles both
    // - (regular) inter-function calls (cross-function control transfer),
    // - (rare) intra-function calls (function-local control transfer)
    Iter = insertInstructions(CounterInstrs, FromBB, Iter);
    return true;
  }

  if (!TargetBB || !FuncDesc)
    return false;

  // Indirect branch, conditional branches or fall-throughs
  // Regular cond branch, put counter at start of target block
  //
  // N.B.: (FromBB != TargetBBs) checks below handle conditional jumps where
  // we can't put the instrumentation counter in this block because not all
  // paths that reach it at this point will be taken and going to the target.
  if (TargetBB->pred_size() == 1 && &FromBB != TargetBB &&
      !TargetBB->isEntryPoint()) {
    insertInstructions(CounterInstrs, *TargetBB, TargetBB->begin());
    return true;
  }
  if (FromBB.succ_size() == 1 && &FromBB != TargetBB) {
    Iter = insertInstructions(CounterInstrs, FromBB, Iter);
    return true;
  }
  // Critical edge, create BB and put counter there
  SplitWorklist.emplace_back(&FromBB, TargetBB);
  SplitInstrs.emplace_back(std::move(CounterInstrs));
  return true;
}

/// Check if a load's displacement expression references a data-section
/// symbol (.data, .bss, .got, .rodata).  Pattern from SimplifyRODataLoads.
static bool hasDataSectionTarget(MCInst &Inst, BinaryContext &BC) {
  const MCOperand *DispOp = BC.MIB->getMemOperandDisp(Inst);
  if (DispOp == Inst.end() || !DispOp->isExpr())
    return false;
  const auto [Sym, SymOffset] = BC.MIB->getTargetSymbolInfo(DispOp->getExpr());
  if (!Sym)
    return false;
  BinaryData *BD = BC.getBinaryDataByName(Sym->getName());
  if (!BD)
    return false;
  const BinarySection &Section = BD->getSection();
  return Section.isAllocatable() && !Section.isTLS() &&
         (Section.isWritable() || Section.getELFType() == ELF::SHT_PROGBITS);
}

void Instrumentation::instrumentFunction(BinaryFunction &Function,
                                         MCPlusBuilder::AllocatorIdTy AllocId) {
  if (Function.hasUnknownControlFlow())
    return;

  BinaryContext &BC = Function.getBinaryContext();
  if (BC.isMachO() && Function.hasName("___GLOBAL_init_65535/1"))
    return;

  std::unordered_set<const BinaryBasicBlock *> BBToSkip;
  if (BC.isAArch64() && hasAArch64ExclusiveMemop(Function, BBToSkip))
    return;

  SplitWorklistTy SplitWorklist;
  SplitInstrsTy SplitInstrs;

  FunctionDescription *FuncDesc = nullptr;
  {
    std::unique_lock<llvm::sys::RWMutex> L(FDMutex);
    Summary->FunctionDescriptions.emplace_back();
    FuncDesc = &Summary->FunctionDescriptions.back();
  }

  FuncDesc->Function = &Function;
  Function.disambiguateJumpTables(AllocId);
  Function.deleteConservativeEdges();

  if (opts::InstrumentLoadProfiles) {
    const unsigned VtableLoadTag = getVtableLoadAnnotationIndex(BC);
    for (BinaryBasicBlock &BB : Function) {
      if (BBToSkip.count(&BB))
        continue;
      for (const MCInst &Inst : BB) {
        if (Function.getJumpTable(Inst) || !BC.MIB->isIndirectCall(Inst))
          continue;
        ++VtableCallsitesExamined;

        std::vector<MCInst *> MethodFetchInsns;
        unsigned VtableRegNum, BaseRegNum;
        uint64_t MethodOffset;
        ArrayRef<MCInst> Insns(&BB.front(), &Inst + 1);
        if (!BC.MIB->analyzeVirtualMethodCall(Insns.begin(), Insns.end(),
                                              MethodFetchInsns, VtableRegNum,
                                              BaseRegNum, MethodOffset))
          continue;
        ++VtableCallsitesRecognized;

        if (MethodFetchInsns.empty())
          continue;

        MCInst *MethodFetchInst = MethodFetchInsns.back();
        auto MethodFetchIter = BB.findInstruction(MethodFetchInst);
        if (MethodFetchIter == BB.end() ||
            !BC.MII->get(MethodFetchInst->getOpcode()).mayLoad() ||
            Function.getJumpTable(*MethodFetchInst))
          continue;

        if (BC.MIB->hasAnnotation(*MethodFetchInst, VtableLoadTag)) {
          ++DuplicateVtableLoads;
          continue;
        }

        const uint32_t LoadOffset =
            BB.getInputOffset() +
            BC.computeCodeSize(BB.begin(), MethodFetchIter);
        BC.MIB->addAnnotation(*MethodFetchInst, VtableLoadTag, LoadOffset,
                              AllocId);
        ++VtableLoadsMarked;
      }
    }
  }

  std::unordered_map<const BinaryBasicBlock *, uint32_t> BBToID;
  uint32_t Id = 0;
  for (auto BBI = Function.begin(); BBI != Function.end(); ++BBI) {
    BBToID[&*BBI] = Id++;
  }
  std::unordered_set<const BinaryBasicBlock *> VisitedSet;
  // DFS to establish edges we will use for a spanning tree. Edges in the
  // spanning tree can be instrumentation-free since their count can be
  // inferred by solving flow equations on a bottom-up traversal of the tree.
  // Exit basic blocks are always instrumented so we start the traversal with
  // a minimum number of defined variables to make the equation solvable.
  std::stack<std::pair<const BinaryBasicBlock *, BinaryBasicBlock *>> Stack;
  std::unordered_map<const BinaryBasicBlock *,
                     std::set<const BinaryBasicBlock *>>
      STOutSet;
  for (auto BBI = Function.getLayout().block_rbegin();
       BBI != Function.getLayout().block_rend(); ++BBI) {
    if ((*BBI)->isEntryPoint() || (*BBI)->isLandingPad()) {
      Stack.push(std::make_pair(nullptr, *BBI));
      if (opts::InstrumentCalls && (*BBI)->isEntryPoint()) {
        EntryNode E;
        E.Node = BBToID[&**BBI];
        E.Address = (*BBI)->getInputOffset();
        FuncDesc->EntryNodes.emplace_back(E);
        createIndCallTargetDescription(Function, (*BBI)->getInputOffset());
      }
    }
  }

  // Modified version of BinaryFunction::dfs() to build a spanning tree
  if (!opts::ConservativeInstrumentation) {
    while (!Stack.empty()) {
      BinaryBasicBlock *BB;
      const BinaryBasicBlock *Pred;
      std::tie(Pred, BB) = Stack.top();
      Stack.pop();
      if (llvm::is_contained(VisitedSet, BB))
        continue;

      VisitedSet.insert(BB);
      if (Pred)
        STOutSet[Pred].insert(BB);

      for (BinaryBasicBlock *SuccBB : BB->successors())
        Stack.push(std::make_pair(BB, SuccBB));
    }
  }

  // Determine whether this is a leaf function, which needs special
  // instructions to protect the red zone
  bool IsLeafFunction = true;
  DenseSet<const BinaryBasicBlock *> InvokeBlocks;
  for (const BinaryBasicBlock &BB : Function) {
    for (const MCInst &Inst : BB) {
      if (BC.MIB->isCall(Inst)) {
        if (BC.MIB->isInvoke(Inst))
          InvokeBlocks.insert(&BB);
        if (!BC.MIB->isTailCall(Inst))
          IsLeafFunction = false;
      }
    }
  }

  for (auto BBI = Function.begin(), BBE = Function.end(); BBI != BBE; ++BBI) {
    BinaryBasicBlock &BB = *BBI;

    // Skip BBs with exclusive load/stores
    if (BBToSkip.find(&BB) != BBToSkip.end())
      continue;

    bool HasUnconditionalBranch = false;
    bool HasJumpTable = false;
    bool IsInvokeBlock = InvokeBlocks.count(&BB) > 0;

    for (auto I = BB.begin(); I != BB.end(); ++I) {
      const MCInst &Inst = *I;
      if (!BC.MIB->getOffset(Inst))
        continue;

      const bool IsJumpTable = Function.getJumpTable(Inst);
      if (IsJumpTable)
        HasJumpTable = true;
      else if (BC.MIB->isUnconditionalBranch(Inst))
        HasUnconditionalBranch = true;
      else if (!(BC.MIB->isCall(Inst) || BC.MIB->isConditionalBranch(Inst)))
        continue;

      const uint32_t FromOffset = *BC.MIB->getOffset(Inst);
      const MCSymbol *Target = BC.MIB->getTargetSymbol(Inst);
      BinaryBasicBlock *TargetBB = Function.getBasicBlockForLabel(Target);
      uint32_t ToOffset = TargetBB ? TargetBB->getInputOffset() : 0;
      BinaryFunction *TargetFunc =
          TargetBB ? &Function : BC.getFunctionForSymbol(Target);
      if (TargetFunc && BC.MIB->isCall(Inst)) {
        if (opts::InstrumentCalls) {
          const BinaryBasicBlock *ForeignBB =
              TargetFunc->getBasicBlockForLabel(Target);
          if (ForeignBB)
            ToOffset = ForeignBB->getInputOffset();
          instrumentOneTarget(SplitWorklist, SplitInstrs, I, Function, BB,
                              FromOffset, *TargetFunc, TargetBB, ToOffset,
                              IsLeafFunction, IsInvokeBlock, FuncDesc,
                              BBToID[&BB]);
        }
        continue;
      }
      if (TargetFunc) {
        // Do not instrument edges in the spanning tree
        if (llvm::is_contained(STOutSet[&BB], TargetBB)) {
          auto L = BC.scopeLock();
          createEdgeDescription(*FuncDesc, Function, FromOffset, BBToID[&BB],
                                Function, ToOffset, BBToID[TargetBB],
                                /*Instrumented=*/false);
          continue;
        }
        instrumentOneTarget(SplitWorklist, SplitInstrs, I, Function, BB,
                            FromOffset, *TargetFunc, TargetBB, ToOffset,
                            IsLeafFunction, IsInvokeBlock, FuncDesc,
                            BBToID[&BB], BBToID[TargetBB]);
        continue;
      }

      if (IsJumpTable) {
        for (BinaryBasicBlock *&Succ : BB.successors()) {
          // Do not instrument edges in the spanning tree
          if (llvm::is_contained(STOutSet[&BB], &*Succ)) {
            auto L = BC.scopeLock();
            createEdgeDescription(*FuncDesc, Function, FromOffset, BBToID[&BB],
                                  Function, Succ->getInputOffset(),
                                  BBToID[&*Succ], /*Instrumented=*/false);
            continue;
          }
          instrumentOneTarget(
              SplitWorklist, SplitInstrs, I, Function, BB, FromOffset, Function,
              &*Succ, Succ->getInputOffset(), IsLeafFunction, IsInvokeBlock,
              FuncDesc, BBToID[&BB], BBToID[&*Succ]);
        }

        if (opts::InstrumentLoadProfiles) {
          MCInst *MemLocInstr = nullptr;
          MCInst *PCRelBaseOut = nullptr;
          MCInst *FixedEntryLoadInst = nullptr;
          unsigned BaseReg = 0, IndexReg = 0;
          int64_t DispValue = 0;
          const MCExpr *DispExpr = nullptr;
          const IndirectBranchType JTType = BC.MIB->analyzeIndirectBranch(
              *I, BB.begin(), I, BC.AsmInfo->getCodePointerSize(), MemLocInstr,
              BaseReg, IndexReg, DispValue, DispExpr, PCRelBaseOut,
              FixedEntryLoadInst, &Function);
          (void)JTType;
          if (MemLocInstr) {
            // getOffset() is only meaningful for call/branch/return/prefix
            // instructions. Use findInstruction() + computeCodeSize() instead.
            auto LoadIter = BB.findInstruction(MemLocInstr);
            assert(LoadIter != BB.end() && "MemLocInstr not found in BB");
            const uint32_t LoadOffset =
                BB.getInputOffset() + BC.computeCodeSize(BB.begin(), LoadIter);
            instrumentLoadTarget(BB, LoadIter, Function, LoadOffset);
            // Mark this BB as done.
            // FIXME: Reason about the validity of this carefully.
            I = std::prev(BB.end());
            continue;
          }
        }
        continue;
      }

      // Handle indirect calls -- could be direct calls with unknown targets
      // or secondary entry points of known functions, so check it is indirect
      // to be sure.
      if (opts::InstrumentCalls && BC.MIB->isIndirectCall(*I))
        instrumentIndirectTarget(BB, I, Function, FromOffset);

    } // End of instructions loop

    // Instrument fallthroughs (when the direct jump instruction is missing)
    if (!HasUnconditionalBranch && !HasJumpTable && BB.succ_size() > 0 &&
        BB.size() > 0) {
      BinaryBasicBlock *FTBB = BB.getFallthrough();
      assert(FTBB && "expected valid fall-through basic block");
      auto I = BB.begin();
      auto LastInstr = BB.end();
      --LastInstr;
      while (LastInstr != I && BC.MIB->isPseudo(*LastInstr))
        --LastInstr;
      uint32_t FromOffset = 0;
      // The last instruction in the BB should have an annotation, except
      // if it was branching to the end of the function as a result of
      // __builtin_unreachable(), in which case it was deleted by fixBranches.
      // Ignore this case. FIXME: force fixBranches() to preserve the offset.
      if (!BC.MIB->getOffset(*LastInstr))
        continue;
      FromOffset = *BC.MIB->getOffset(*LastInstr);

      // Do not instrument edges in the spanning tree
      if (llvm::is_contained(STOutSet[&BB], FTBB)) {
        auto L = BC.scopeLock();
        createEdgeDescription(*FuncDesc, Function, FromOffset, BBToID[&BB],
                              Function, FTBB->getInputOffset(), BBToID[FTBB],
                              /*Instrumented=*/false);
        continue;
      }
      instrumentOneTarget(SplitWorklist, SplitInstrs, I, Function, BB,
                          FromOffset, Function, FTBB, FTBB->getInputOffset(),
                          IsLeafFunction, IsInvokeBlock, FuncDesc, BBToID[&BB],
                          BBToID[FTBB]);
    }
  } // End of BBs loop

  // Instrument selected vtable loads and data-section loads for ReorderData.
  if (opts::InstrumentLoadProfiles) {
    const unsigned VtableLoadTag = getVtableLoadAnnotationIndex(BC);

    for (auto BBI = Function.begin(); BBI != Function.end(); ++BBI) {
      BinaryBasicBlock &BB = *BBI;
      if (BBToSkip.count(&BB))
        continue;
      for (auto I = BB.begin(); I != BB.end(); ++I) {
        MCInst &Inst = *I;
        if (!BC.MII->get(Inst.getOpcode()).mayLoad())
          continue;
        if (Function.getJumpTable(Inst))
          continue;
        bool IsLoad, IsStore, IsStoreFromReg, IsSimple, IsIndexed;
        MCPhysReg Reg;
        int32_t SrcImm;
        uint16_t StackPtrReg;
        int64_t StackOffset;
        uint8_t Size;
        if (BC.MIB->isStackAccess(Inst, IsLoad, IsStore, IsStoreFromReg, Reg,
                                  SrcImm, StackPtrReg, StackOffset, Size,
                                  IsSimple, IsIndexed))
          continue;
        if (BC.MIB->hasAnnotation(Inst, VtableLoadTag)) {
          const uint32_t LoadOffset =
              BC.MIB->getAnnotationAs<uint32_t>(Inst, VtableLoadTag);
          BC.MIB->removeAnnotation(Inst, VtableLoadTag);
          instrumentLoadTarget(BB, I, Function, LoadOffset);
          ++VtableLoadsInstrumented;
          continue;
        }
        if (!hasDataSectionTarget(Inst, BC))
          continue;
        const uint32_t LoadOffset =
            BB.getInputOffset() + BC.computeCodeSize(BB.begin(), I);
        instrumentLoadTarget(BB, I, Function, LoadOffset);
      }
    }
  }

  // Instrument spanning tree leaves
  if (!opts::ConservativeInstrumentation) {
    for (auto BBI = Function.begin(), BBE = Function.end(); BBI != BBE; ++BBI) {
      BinaryBasicBlock &BB = *BBI;
      if (STOutSet[&BB].size() == 0)
        instrumentLeafNode(BB, BB.begin(), IsLeafFunction, *FuncDesc,
                           BBToID[&BB]);
    }
  }

  // Consume list of critical edges: split them and add instrumentation to the
  // newly created BBs
  auto Iter = SplitInstrs.begin();
  for (std::pair<BinaryBasicBlock *, BinaryBasicBlock *> &BBPair :
       SplitWorklist) {
    BinaryBasicBlock *NewBB = Function.splitEdge(BBPair.first, BBPair.second);
    NewBB->addInstructions(Iter->begin(), Iter->end());
    ++Iter;
  }

  // Unused now
  FuncDesc->EdgesSet.clear();
}

Error Instrumentation::runOnFunctions(BinaryContext &BC) {
  if (BC.usesBTI())
    return createFatalBOLTError(
        "BOLT-ERROR: instrumenting binaries using BTI is not supported.\n");
  /* BTI TODO:
   Instrumentation functions add indirect branches into the .text.injected
   section, see:
   - __bolt_instr_ind_call_handler
   - __bolt_instr_ind_tail_call_handler
   - __bolt_instr_ind_tailcall_handler_func
   - __bolt_start_trampoline
   - __bolt_fini_trampoline
   We cannot add BTIs to their targets when they are created, because the
   instrumentation snippets get added later to these targets, and the added BTI
   instruction will not be the first (rendering it useless).
   */

  const unsigned Flags = BinarySection::getFlags(/*IsReadOnly=*/false,
                                                 /*IsText=*/false,
                                                 /*IsAllocatable=*/true);
  BC.registerOrUpdateSection(".bolt.instr.counters", ELF::SHT_PROGBITS, Flags,
                             nullptr, 0, BC.RegularPageSize);

  BC.registerOrUpdateNoteSection(".bolt.instr.tables", nullptr, 0,
                                 /*Alignment=*/1,
                                 /*IsReadOnly=*/true, ELF::SHT_NOTE);

  Summary->IndCallCounterFuncPtr =
      BC.Ctx->getOrCreateSymbol("__bolt_ind_call_counter_func_pointer");
  Summary->IndTailCallCounterFuncPtr =
      BC.Ctx->getOrCreateSymbol("__bolt_ind_tailcall_counter_func_pointer");
  Summary->LoadCounterFuncPtr =
      BC.Ctx->getOrCreateSymbol("__bolt_load_counter_func_pointer");

  createAuxiliaryFunctions(BC);

  ParallelUtilities::PredicateTy SkipPredicate = [&](const BinaryFunction &BF) {
    return (!BF.isSimple() || BF.isIgnored() ||
            (opts::InstrumentHotOnly && !BF.getKnownExecutionCount()));
  };

  ParallelUtilities::WorkFuncWithAllocTy WorkFun =
      [&](BinaryFunction &BF, MCPlusBuilder::AllocatorIdTy AllocatorId) {
        instrumentFunction(BF, AllocatorId);
      };

  ParallelUtilities::runOnEachFunctionWithUniqueAllocId(
      BC, ParallelUtilities::SchedulingPolicy::SP_INST_QUADRATIC, WorkFun,
      SkipPredicate, "instrumentation", /* ForceSequential=*/true);

  if (BC.isMachO()) {
    if (BC.StartFunctionAddress) {
      BinaryFunction *Main =
          BC.getBinaryFunctionAtAddress(*BC.StartFunctionAddress);
      assert(Main && "Entry point function not found");
      BinaryBasicBlock &BB = Main->front();

      ErrorOr<BinarySection &> SetupSection =
          BC.getUniqueSectionByName("I__setup");
      if (!SetupSection)
        return createFatalBOLTError("Cannot find I__setup section\n");

      MCSymbol *Target = BC.registerNameAtAddress(
          "__bolt_instr_setup", SetupSection->getAddress(), 0, 0);
      MCInst NewInst;
      BC.MIB->createCall(NewInst, Target, BC.Ctx.get());
      BB.insertInstruction(BB.begin(), std::move(NewInst));
    } else {
      BC.errs() << "BOLT-WARNING: Entry point not found\n";
    }

    if (BinaryData *BD = BC.getBinaryDataByName("___GLOBAL_init_65535/1")) {
      BinaryFunction *Ctor = BC.getBinaryFunctionAtAddress(BD->getAddress());
      assert(Ctor && "___GLOBAL_init_65535 function not found");
      BinaryBasicBlock &BB = Ctor->front();
      ErrorOr<BinarySection &> FiniSection =
          BC.getUniqueSectionByName("I__fini");
      if (!FiniSection)
        return createFatalBOLTError("Cannot find I__fini section");

      MCSymbol *Target = BC.registerNameAtAddress(
          "__bolt_instr_fini", FiniSection->getAddress(), 0, 0);
      auto IsLEA = [&BC](const MCInst &Inst) { return BC.MIB->isLEA64r(Inst); };
      const auto LEA = std::find_if(
          std::next(llvm::find_if(reverse(BB), IsLEA)), BB.rend(), IsLEA);
      LEA->getOperand(4).setExpr(MCSymbolRefExpr::create(Target, *BC.Ctx));
    } else {
      BC.errs() << "BOLT-WARNING: ___GLOBAL_init_65535 not found\n";
    }
  }

  setupRuntimeLibrary(BC);
  return Error::success();
}

void Instrumentation::createAuxiliaryFunctions(BinaryContext &BC) {
  auto createSimpleFunction =
      [&](StringRef Title, InstructionListType Instrs) -> BinaryFunction * {
    BinaryFunction *Func = BC.createInjectedBinaryFunction(std::string(Title));

    std::vector<std::unique_ptr<BinaryBasicBlock>> BBs;
    BBs.emplace_back(Func->createBasicBlock());
    BBs.back()->addInstructions(Instrs.begin(), Instrs.end());
    BBs.back()->setCFIState(0);
    Func->insertBasicBlocks(nullptr, std::move(BBs),
                            /*UpdateLayout=*/true,
                            /*UpdateCFIState=*/false);
    Func->updateState(BinaryFunction::State::CFG_Finalized);
    return Func;
  };

  // Here we are creating a set of functions to handle BB entry/exit.
  // IndCallHandlerExitBB contains instructions to finish handling traffic to an
  // indirect call. We pass it to createInstrumentedIndCallHandlerEntryBB(),
  // which will check if a pointer to runtime library traffic accounting
  // function was initialized (it is done during initialization of runtime
  // library). If it is so - calls it. Then this routine returns to normal
  // execution by jumping to exit BB.
  BinaryFunction *IndCallHandlerExitBB =
      createSimpleFunction("__bolt_instr_ind_call_handler",
                           BC.MIB->createInstrumentedIndCallHandlerExitBB());

  IndCallHandlerExitBBFunction =
      createSimpleFunction("__bolt_instr_ind_call_handler_func",
                           BC.MIB->createInstrumentedIndCallHandlerEntryBB(
                               Summary->IndCallCounterFuncPtr,
                               IndCallHandlerExitBB->getSymbol(), &*BC.Ctx));

  BinaryFunction *IndTailCallHandlerExitBB = createSimpleFunction(
      "__bolt_instr_ind_tail_call_handler",
      BC.MIB->createInstrumentedIndTailCallHandlerExitBB());

  IndTailCallHandlerExitBBFunction = createSimpleFunction(
      "__bolt_instr_ind_tailcall_handler_func",
      BC.MIB->createInstrumentedIndCallHandlerEntryBB(
          Summary->IndTailCallCounterFuncPtr,
          IndTailCallHandlerExitBB->getSymbol(), &*BC.Ctx));

  if (opts::InstrumentLoadProfiles)
    LoadHandlerFunction =
        createSimpleFunction("__bolt_instr_load_handler_func",
                             BC.MIB->createInstrumentedLoadHandlerBody(
                                 Summary->LoadCounterFuncPtr, &*BC.Ctx));

  // These helpers are not needed on ELFs.  Don't define them to prevent them
  // from referencing non-existent symbols.
  //
  // Cf. "#if defined(__APPLE__)" in bolt/runtime/instr.cpp.
  if (BC.isMachO()) {
    createSimpleFunction("__bolt_num_counters_getter",
                         BC.MIB->createNumCountersGetter(BC.Ctx.get()));
    createSimpleFunction("__bolt_instr_locations_getter",
                         BC.MIB->createInstrLocationsGetter(BC.Ctx.get()));
    createSimpleFunction("__bolt_instr_tables_getter",
                         BC.MIB->createInstrTablesGetter(BC.Ctx.get()));
    createSimpleFunction("__bolt_instr_num_funcs_getter",
                         BC.MIB->createInstrNumFuncsGetter(BC.Ctx.get()));
  }

  if (BC.isELF()) {
    if (BC.StartFunctionAddress) {
      BinaryFunction *Start =
          BC.getBinaryFunctionAtAddress(*BC.StartFunctionAddress);
      assert(Start && "Entry point function not found");
      const MCSymbol *StartSym = Start->getSymbol();
      createSimpleFunction(
          "__bolt_start_trampoline",
          BC.MIB->createSymbolTrampoline(StartSym, BC.Ctx.get()));
    }
    if (BC.FiniFunctionAddress) {
      BinaryFunction *Fini =
          BC.getBinaryFunctionAtAddress(*BC.FiniFunctionAddress);
      assert(Fini && "Finalization function not found");
      const MCSymbol *FiniSym = Fini->getSymbol();
      createSimpleFunction(
          "__bolt_fini_trampoline",
          BC.MIB->createSymbolTrampoline(FiniSym, BC.Ctx.get()));
    } else {
      // Create dummy return function for trampoline to avoid issues
      // with unknown symbol in runtime library. E.g. for static PIE
      // executable
      createSimpleFunction("__bolt_fini_trampoline",
                           BC.MIB->createReturnInstructionList(BC.Ctx.get()));
    }
    if (BC.isAArch64())
      BC.MIB->createInstrCounterIncrFunc(BC);
  }
}

void Instrumentation::setupRuntimeLibrary(BinaryContext &BC) {
  uint32_t FuncDescSize = Summary->getFDSize();

  BC.outs() << "BOLT-INSTRUMENTER: Number of indirect call site descriptors: "
            << Summary->IndCallDescriptions.size() << "\n";
  BC.outs() << "BOLT-INSTRUMENTER: Number of indirect call target descriptors: "
            << Summary->IndCallTargetDescriptions.size() << "\n";
  BC.outs() << "BOLT-INSTRUMENTER: Number of function descriptors: "
            << Summary->FunctionDescriptions.size() << "\n";
  BC.outs() << "BOLT-INSTRUMENTER: Number of load site descriptors: "
            << Summary->LoadDescriptions.size() << "\n";
  BC.outs() << "BOLT-INSTRUMENTER: Number of indirect callsites examined for "
               "vtable form: "
            << VtableCallsitesExamined << "\n";
  BC.outs() << "BOLT-INSTRUMENTER: Number of virtual callsites recognized: "
            << VtableCallsitesRecognized << "\n";
  BC.outs() << "BOLT-INSTRUMENTER: Number of vtable loads marked: "
            << VtableLoadsMarked << "\n";
  BC.outs() << "BOLT-INSTRUMENTER: Number of duplicate vtable load marks: "
            << DuplicateVtableLoads << "\n";
  BC.outs() << "BOLT-INSTRUMENTER: Number of vtable loads instrumented: "
            << VtableLoadsInstrumented << "\n";
  BC.outs() << "BOLT-INSTRUMENTER: Number of branch counters: "
            << BranchCounters << "\n";
  BC.outs() << "BOLT-INSTRUMENTER: Number of ST leaf node counters: "
            << LeafNodeCounters << "\n";
  BC.outs() << "BOLT-INSTRUMENTER: Number of direct call counters: "
            << DirectCallCounters << "\n";
  BC.outs() << "BOLT-INSTRUMENTER: Total number of counters: "
            << Summary->Counters.size() << "\n";
  BC.outs() << "BOLT-INSTRUMENTER: Total size of counters: "
            << (Summary->Counters.size() * 8)
            << " bytes (static alloc memory)\n";
  BC.outs() << "BOLT-INSTRUMENTER: Total size of string table emitted: "
            << Summary->StringTable.size() << " bytes in file\n";
  BC.outs() << "BOLT-INSTRUMENTER: Total size of descriptors: "
            << (FuncDescSize +
                Summary->IndCallDescriptions.size() *
                    sizeof(IndCallDescription) +
                Summary->IndCallTargetDescriptions.size() *
                    sizeof(IndCallTargetDescription) +
                Summary->LoadDescriptions.size() * sizeof(LoadDescription))
            << " bytes in file\n";
  BC.outs() << "BOLT-INSTRUMENTER: Profile will be saved to file "
            << opts::InstrumentationFilename << "\n";

  InstrumentationRuntimeLibrary *RtLibrary =
      static_cast<InstrumentationRuntimeLibrary *>(BC.getRuntimeLibrary());
  assert(RtLibrary && "instrumentation runtime library object must be set");
  RtLibrary->setSummary(std::move(Summary));
}

unsigned Instrumentation::getVtableLoadAnnotationIndex(BinaryContext &BC) {
  if (VtableLoadAnnotationIndex)
    return *VtableLoadAnnotationIndex;
  VtableLoadAnnotationIndex =
      BC.MIB->getOrCreateAnnotationIndex("InstrumentVtableLoad");
  return *VtableLoadAnnotationIndex;
}
} // namespace bolt
} // namespace llvm
