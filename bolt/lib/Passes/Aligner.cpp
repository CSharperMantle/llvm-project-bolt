//===- bolt/Passes/Aligner.cpp - Pass for optimal code alignment ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the AlignerPass class.
//
//===----------------------------------------------------------------------===//

#include "bolt/Passes/Aligner.h"
#include "bolt/Core/ParallelUtilities.h"
#include "llvm/ADT/SmallPtrSet.h"

#define DEBUG_TYPE "bolt-aligner"

using namespace llvm;

namespace opts {

extern cl::OptionCategory BoltOptCategory;

extern cl::opt<bool> AlignBlocks;
extern cl::opt<bool> PreserveBlocksAlignment;
extern cl::opt<unsigned> AlignFunctions;

static cl::opt<unsigned> AlignBlocksMinSize(
    "align-blocks-min-size",
    cl::desc("minimal size of the basic block that should be aligned"),
    cl::init(0), cl::ZeroOrMore, cl::Hidden, cl::cat(BoltOptCategory));

static cl::opt<unsigned> AlignBlocksThreshold(
    "align-blocks-threshold",
    cl::desc(
        "align only blocks with frequency larger than containing function "
        "execution frequency specified in percent. E.g. 1000 means aligning "
        "blocks that are 10 times more frequently executed than the "
        "containing function."),
    cl::init(800), cl::Hidden, cl::cat(BoltOptCategory));

static cl::opt<unsigned> AlignFunctionsMaxBytes(
    "align-functions-max-bytes",
    cl::desc("maximum number of bytes to use to align functions"), cl::init(32),
    cl::cat(BoltOptCategory));

static cl::opt<unsigned>
    BlockAlignment("block-alignment",
                   cl::desc("boundary to use for alignment of basic blocks"),
                   cl::init(16), cl::ZeroOrMore, cl::cat(BoltOptCategory));

static cl::opt<bool>
    UseCompactAligner("use-compact-aligner",
                      cl::desc("Use compact approach for aligning functions"),
                      cl::init(true), cl::cat(BoltOptCategory));

static cl::opt<bool> AlignHotLoopHeaders(
    "align-hot-loop-headers",
    cl::desc("align hot natural-loop headers based on profile and layout"),
    cl::init(false), cl::cat(BoltOptCategory));

static cl::opt<unsigned>
    HotLoopAlignment("hot-loop-alignment",
                     cl::desc("boundary to use to align hot loop headers"),
                     cl::init(16), cl::ZeroOrMore, cl::cat(BoltOptCategory));

static cl::opt<unsigned> HotLoopAlignmentMaxBytes(
    "hot-loop-alignment-max-bytes",
    cl::desc("maximum number of bytes to use to align hot loop headers"),
    cl::init(16), cl::cat(BoltOptCategory));

} // end namespace opts

namespace llvm {
namespace bolt {

// Align function to the specified byte-boundary (typically, 64) offsetting
// the function by not more than the corresponding value
static void alignMaxBytes(BinaryFunction &Function) {
  Function.setAlignment(opts::AlignFunctions);
  Function.setMaxAlignmentBytes(opts::AlignFunctionsMaxBytes);
  Function.setMaxColdAlignmentBytes(opts::AlignFunctionsMaxBytes);
}

// Align function to the specified byte-boundary (typically, 64) offsetting
// the function by not more than the minimum over
// -- the size of the function
// -- the specified number of bytes
static void alignCompact(BinaryFunction &Function,
                         const MCCodeEmitter *Emitter) {
  const BinaryContext &BC = Function.getBinaryContext();
  size_t HotSize = 0;
  size_t ColdSize = 0;

  // On AArch64, larger cold code size may lead to more veneers and higher
  // potential overhead for hot code. Minimize the cold code size.
  if (!Function.hasProfile() && BC.isAArch64()) {
    Function.setAlignment(Function.getMinAlignment());
    return;
  }

  for (const BinaryBasicBlock &BB : Function)
    if (BB.isSplit())
      ColdSize += BC.computeCodeSize(BB.begin(), BB.end(), Emitter);
    else
      HotSize += BC.computeCodeSize(BB.begin(), BB.end(), Emitter);

  Function.setAlignment(opts::AlignFunctions);
  if (HotSize > 0)
    Function.setMaxAlignmentBytes(
      std::min(size_t(opts::AlignFunctionsMaxBytes), HotSize));

  // using the same option, max-align-bytes, both for cold and hot parts of the
  // functions, as aligning cold functions typically does not affect performance
  if (ColdSize > 0)
    Function.setMaxColdAlignmentBytes(
      std::min(size_t(opts::AlignFunctionsMaxBytes), ColdSize));
}

static bool applyBlockAlignment(BinaryBasicBlock &BB, uint32_t Alignment,
                                uint32_t MaxBytes) {
  if (Alignment <= BB.getAlignment())
    return false;

  BB.setAlignment(Alignment);
  BB.setAlignmentMaxBytes(MaxBytes);
  return true;
}

void AlignerPass::alignHotLoopHeaders(BinaryFunction &Function,
                                      const MCCodeEmitter *Emitter,
                                      uint64_t HotThreshold) {
  if (!Function.hasValidProfile() || !Function.isSimple() || Function.empty())
    return;

  const BinaryContext &BC = Function.getBinaryContext();

  Function.constructDomTree();
  Function.calculateLoopInfo();

  DenseMap<BinaryBasicBlock *, BinaryBasicBlock *> LayoutPreds;
  BinaryBasicBlock *PrevBB = nullptr;
  for (BinaryBasicBlock *const BB : Function.getLayout().blocks()) {
    if (PrevBB)
      LayoutPreds[BB] = PrevBB;
    PrevBB = BB;
  }

  SmallVector<BinaryLoop *> WorkList;
  for (BinaryLoop *const Loop : Function.getLoopInfo())
    WorkList.emplace_back(Loop);

  SmallPtrSet<BinaryBasicBlock *, 8> SeenHeaders;
  while (!WorkList.empty()) {
    BinaryLoop *const Loop = WorkList.pop_back_val();
    for (BinaryLoop *const SubLoop : *Loop)
      WorkList.emplace_back(SubLoop);

    BinaryBasicBlock *const Header = Loop->getHeader();
    if (!Header || !SeenHeaders.insert(Header).second)
      continue;

    LLVM_DEBUG({ ++NumHotLoopHeaderCandidates; });

    if (Header->isEntryPoint() || Header->isSplit())
      continue;
    // Skip headers without profile.
    if (!Header->hasProfile() || Header->getKnownExecutionCount() == 0)
      continue;
    // Skip headers with cold backedges.
    if (Loop->TotalBackEdgeCount == BinaryBasicBlock::COUNT_NO_PROFILE ||
        Loop->TotalBackEdgeCount < HotThreshold)
      continue;

    const uint64_t HeaderSize =
        BC.computeCodeSize(Header->begin(), Header->end(), Emitter);
    if (HeaderSize == 0) {
      LLVM_DEBUG({
        dbgs() << "BOLT-DEBUG: zero-sized loop header found at \""
               << Function.getPrintName() << "\"+0x"
               << Twine::utohexstr(Header->getOffset()) << ". How curious!";
      });
      continue;
    }

    const unsigned MaxBytes = std::min<unsigned>(
        opts::HotLoopAlignment - 1, opts::HotLoopAlignmentMaxBytes);

    if (BinaryBasicBlock *const LayoutPred = LayoutPreds.lookup(Header)) {
      if (LayoutPred->getFallthrough() == Header) {
        // Here we're having layouts like this:
        //
        // # --- BB LayoutPred ---
        //    ...
        //    instr
        // # (fallthrough to Header)
        // # --- loop header padding ---
        //    nop
        //    nop
        // # --- BB Header ---
        // .Lheader:
        //    instr
        //    ...
        //
        // We don't want to waste too much time executing the paddings on the
        // fallthrough path. Thus, we skip padding this loop if the fallthrough
        // takes up more then 1/5 of |Header|'s total execution count.  This
        // percentage is taken from MachineBlockPlacement::alignBlocks().

        const uint64_t FallthroughCount =
            LayoutPred->getBranchInfo(*Header).Count;
        if (FallthroughCount == BinaryBasicBlock::COUNT_NO_PROFILE)
          continue;

        // Don't pad when we might spend too much time on the padding itself.
        if (FallthroughCount > Header->getKnownExecutionCount() / 5)
          continue;
      }
    }

    LLVM_DEBUG({ ++NumHotLoopHeadersSelected; });

    if (applyBlockAlignment(*Header, opts::HotLoopAlignment, MaxBytes)) {
      LLVM_DEBUG({
        std::unique_lock<llvm::sys::RWMutex> Lock(AlignHistogramMtx);
        AlignHistogram[MaxBytes]++;
        AlignedBlocksCount += Header->getKnownExecutionCount();
      });
    }
  }
}

void AlignerPass::alignBlocks(BinaryFunction &Function,
                              const MCCodeEmitter *Emitter) {
  if (!Function.hasValidProfile() || !Function.isSimple())
    return;

  const BinaryContext &BC = Function.getBinaryContext();

  const uint64_t FuncCount =
      std::max<uint64_t>(1, Function.getKnownExecutionCount());
  BinaryBasicBlock *PrevBB = nullptr;
  for (BinaryBasicBlock *BB : Function.getLayout().blocks()) {
    uint64_t Count = BB->getKnownExecutionCount();

    if (Count <= FuncCount * opts::AlignBlocksThreshold / 100) {
      PrevBB = BB;
      continue;
    }

    uint64_t FTCount = 0;
    if (PrevBB && PrevBB->getFallthrough() == BB)
      FTCount = PrevBB->getBranchInfo(*BB).Count;

    PrevBB = BB;

    if (Count < FTCount * 2)
      continue;

    const uint64_t BlockSize =
        BC.computeCodeSize(BB->begin(), BB->end(), Emitter);
    const uint64_t BytesToUse =
        std::min<uint64_t>(opts::BlockAlignment - 1, BlockSize);

    if (opts::AlignBlocksMinSize && BlockSize < opts::AlignBlocksMinSize)
      continue;

    BB->setAlignment(opts::BlockAlignment);
    BB->setAlignmentMaxBytes(BytesToUse);

    // Update stats.
    LLVM_DEBUG(
      std::unique_lock<llvm::sys::RWMutex> Lock(AlignHistogramMtx);
      AlignHistogram[BytesToUse]++;
      AlignedBlocksCount += BB->getKnownExecutionCount();
    );
  }
}

Error AlignerPass::runOnFunctions(BinaryContext &BC) {
  if (!BC.HasRelocations)
    return Error::success();

  BC.AlignHotLoopHeaders = opts::AlignHotLoopHeaders;
  BC.HotLoopAlignment = opts::HotLoopAlignment;

  const unsigned Alignment =
      opts::AlignHotLoopHeaders
          ? std::max(opts::BlockAlignment, opts::HotLoopAlignment)
          : opts::BlockAlignment;
  AlignHistogram.resize(Alignment);

  // To avoid racing the static variable |Threshold| there.
  const uint64_t HotThreshold = BC.getHotThreshold();

  ParallelUtilities::WorkFuncTy WorkFun = [&](BinaryFunction &BF) {
    // Create a separate MCCodeEmitter to allow lock free execution
    BinaryContext::IndependentCodeEmitter Emitter =
        BC.createIndependentMCCodeEmitter();

    if (opts::UseCompactAligner)
      alignCompact(BF, Emitter.MCE.get());
    else
      alignMaxBytes(BF);

    if (opts::AlignBlocks && !opts::PreserveBlocksAlignment)
      alignBlocks(BF, Emitter.MCE.get());

    if (opts::AlignHotLoopHeaders)
      alignHotLoopHeaders(BF, Emitter.MCE.get(), HotThreshold);
  };

  ParallelUtilities::runOnEachFunction(
      BC, ParallelUtilities::SchedulingPolicy::SP_TRIVIAL, WorkFun,
      ParallelUtilities::PredicateTy(nullptr), "AlignerPass");

  LLVM_DEBUG({
    if (opts::AlignHotLoopHeaders)
      dbgs() << "BOLT-DEBUG: selected " << NumHotLoopHeadersSelected << " of "
             << NumHotLoopHeaderCandidates << " hot loop header candidates\n";
    dbgs() << "BOLT-DEBUG: max bytes per basic block alignment distribution:\n";
    for (unsigned I = 1; I < AlignHistogram.size(); ++I)
      dbgs() << "  " << I << " : " << AlignHistogram[I] << '\n';

    dbgs() << "BOLT-DEBUG: total execution count of aligned blocks: "
           << AlignedBlocksCount << '\n';
  });
  return Error::success();
}

} // end namespace bolt
} // end namespace llvm
