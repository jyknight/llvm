//===--- CaptureTracking.cpp - Determine whether a pointer is captured ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains routines that help determine which pointers are captured.
// A pointer value is captured if the function makes a copy of any part of the
// pointer that outlives the call.  Not being captured means, more or less, that
// the pointer is only dereferenced and not stored in a global.  Returning part
// of the pointer as the function return value may or may not count as capturing
// the pointer, depending on the context.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/CaptureTracking.h"
#include "llvm/Analysis/OrderedBasicBlock.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

CaptureTracker::~CaptureTracker() {}

bool CaptureTracker::shouldExplore(const Use *U) { return true; }

namespace {
  struct SimpleCaptureTracker : public CaptureTracker {
    explicit SimpleCaptureTracker(bool ReturnCaptures)
      : ReturnCaptures(ReturnCaptures), Captured(false) {}

    void tooManyUses() override { Captured = true; }

    bool captured(const Use *U) override {
      if (isa<ReturnInst>(U->getUser()) && !ReturnCaptures)
        return false;

      Captured = true;
      return true;
    }

    bool ReturnCaptures;

    bool Captured;
  };

  /// Only find pointer captures which happen before the given instruction. Uses
  /// the dominator tree to determine whether one instruction is before another.
  /// Only support the case where the Value is defined in the same basic block
  /// as the given instruction and the use.
  struct CapturesBefore : public CaptureTracker {

    CapturesBefore(bool ReturnCaptures, const Instruction *I, DominatorTree *DT,
                   bool IncludeI, OrderedBasicBlock *IC)
      : OrderedBB(IC), BeforeHere(I), DT(DT),
        ReturnCaptures(ReturnCaptures), IncludeI(IncludeI), Captured(false) {}

    void tooManyUses() override { Captured = true; }

    bool isSafeToPrune(Instruction *I) {
      BasicBlock *BB = I->getParent();
      // We explore this usage only if the usage can reach "BeforeHere".
      // If use is not reachable from entry, there is no need to explore.
      if (BeforeHere != I && !DT->isReachableFromEntry(BB))
        return true;

      // Compute the case where both instructions are inside the same basic
      // block. Since instructions in the same BB as BeforeHere are numbered in
      // 'OrderedBB', avoid using 'dominates' and 'isPotentiallyReachable'
      // which are very expensive for large basic blocks.
      if (BB == BeforeHere->getParent()) {
        // 'I' dominates 'BeforeHere' => not safe to prune.
        //
        // The value defined by an invoke/catchpad dominates an instruction only
        // if it dominates every instruction in UseBB. A PHI is dominated only
        // if the instruction dominates every possible use in the UseBB. Since
        // UseBB == BB, avoid pruning.
        if (isa<InvokeInst>(BeforeHere) || isa<CatchPadInst>(BeforeHere) ||
            isa<PHINode>(I) || I == BeforeHere)
          return false;
        if (!OrderedBB->dominates(BeforeHere, I))
          return false;

        // 'BeforeHere' comes before 'I', it's safe to prune if we also
        // guarantee that 'I' never reaches 'BeforeHere' through a back-edge or
        // by its successors, i.e, prune if:
        //
        //  (1) BB is an entry block or have no sucessors.
        //  (2) There's no path coming back through BB sucessors.
        if (BB == &BB->getParent()->getEntryBlock() ||
            !BB->getTerminator()->getNumSuccessors())
          return true;

        SmallVector<BasicBlock*, 32> Worklist;
        Worklist.append(succ_begin(BB), succ_end(BB));
        if (!isPotentiallyReachableFromMany(Worklist, BB, DT))
          return true;

        return false;
      }

      // If the value is defined in the same basic block as use and BeforeHere,
      // there is no need to explore the use if BeforeHere dominates use.
      // Check whether there is a path from I to BeforeHere.
      if (BeforeHere != I && DT->dominates(BeforeHere, I) &&
          !isPotentiallyReachable(I, BeforeHere, DT))
        return true;

      return false;
    }

    bool shouldExplore(const Use *U) override {
      Instruction *I = cast<Instruction>(U->getUser());

      if (BeforeHere == I && !IncludeI)
        return false;

      if (isSafeToPrune(I))
        return false;

      return true;
    }

    bool captured(const Use *U) override {
      if (isa<ReturnInst>(U->getUser()) && !ReturnCaptures)
        return false;

      if (!shouldExplore(U))
        return false;

      Captured = true;
      return true;
    }

    OrderedBasicBlock *OrderedBB;
    const Instruction *BeforeHere;
    DominatorTree *DT;

    bool ReturnCaptures;
    bool IncludeI;

    bool Captured;
  };
}

/// PointerMayBeCaptured - Return true if this pointer value may be captured
/// by the enclosing function (which is required to exist).  This routine can
/// be expensive, so consider caching the results.  The boolean ReturnCaptures
/// specifies whether returning the value (or part of it) from the function
/// counts as capturing it or not.  The boolean StoreCaptures specified whether
/// storing the value (or part of it) into memory anywhere automatically
/// counts as capturing it or not.
bool llvm::PointerMayBeCaptured(const Value *V,
                                bool ReturnCaptures, bool StoreCaptures) {
  assert(!isa<GlobalValue>(V) &&
         "It doesn't make sense to ask whether a global is captured.");

  // TODO: If StoreCaptures is not true, we could do Fancy analysis
  // to determine whether this store is not actually an escape point.
  // In that case, BasicAliasAnalysis should be updated as well to
  // take advantage of this.
  (void)StoreCaptures;

  SimpleCaptureTracker SCT(ReturnCaptures);
  PointerMayBeCaptured(V, &SCT);
  return SCT.Captured;
}

/// PointerMayBeCapturedBefore - Return true if this pointer value may be
/// captured by the enclosing function (which is required to exist). If a
/// DominatorTree is provided, only captures which happen before the given
/// instruction are considered. This routine can be expensive, so consider
/// caching the results.  The boolean ReturnCaptures specifies whether
/// returning the value (or part of it) from the function counts as capturing
/// it or not.  The boolean StoreCaptures specified whether storing the value
/// (or part of it) into memory anywhere automatically counts as capturing it
/// or not. A ordered basic block \p OBB can be used in order to speed up
/// queries about relative order among instructions in the same basic block.
bool llvm::PointerMayBeCapturedBefore(const Value *V, bool ReturnCaptures,
                                      bool StoreCaptures, const Instruction *I,
                                      DominatorTree *DT, bool IncludeI,
                                      OrderedBasicBlock *OBB) {
  assert(!isa<GlobalValue>(V) &&
         "It doesn't make sense to ask whether a global is captured.");
  bool UseNewOBB = OBB == nullptr;

  if (!DT)
    return PointerMayBeCaptured(V, ReturnCaptures, StoreCaptures);
  if (UseNewOBB)
    OBB = new OrderedBasicBlock(I->getParent());

  // TODO: See comment in PointerMayBeCaptured regarding what could be done
  // with StoreCaptures.

  CapturesBefore CB(ReturnCaptures, I, DT, IncludeI, OBB);
  PointerMayBeCaptured(V, &CB);

  if (UseNewOBB)
    delete OBB;
  return CB.Captured;
}

/// TODO: Write a new FunctionPass AliasAnalysis so that it can keep
/// a cache. Then we can move the code from BasicAliasAnalysis into
/// that path, and remove this threshold.
static int const Threshold = 20;

void llvm::PointerMayBeCaptured(const Value *V, CaptureTracker *Tracker) {
  assert(V->getType()->isPointerTy() && "Capture is for pointers only!");
  SmallVector<const Use *, Threshold> Worklist;
  SmallSet<const Use *, Threshold> Visited;
  int Count = 0;

  for (const Use &U : V->uses()) {
    // If there are lots of uses, conservatively say that the value
    // is captured to avoid taking too much compile time.
    if (Count++ >= Threshold)
      return Tracker->tooManyUses();

    if (!Tracker->shouldExplore(&U)) continue;
    Visited.insert(&U);
    Worklist.push_back(&U);
  }

  while (!Worklist.empty()) {
    const Use *U = Worklist.pop_back_val();
    Instruction *I = cast<Instruction>(U->getUser());
    V = U->get();

    switch (I->getOpcode()) {
    case Instruction::Call:
    case Instruction::Invoke: {
      CallSite CS(I);
      // Not captured if the callee is readonly, doesn't return a copy through
      // its return value and doesn't unwind (a readonly function can leak bits
      // by throwing an exception or not depending on the input value).
      if (CS.onlyReadsMemory() && CS.doesNotThrow() && I->getType()->isVoidTy())
        break;

      // Not captured if only passed via 'nocapture' arguments.  Note that
      // calling a function pointer does not in itself cause the pointer to
      // be captured.  This is a subtle point considering that (for example)
      // the callee might return its own address.  It is analogous to saying
      // that loading a value from a pointer does not cause the pointer to be
      // captured, even though the loaded value might be the pointer itself
      // (think of self-referential objects).
      CallSite::arg_iterator B = CS.arg_begin(), E = CS.arg_end();
      for (CallSite::arg_iterator A = B; A != E; ++A)
        if (A->get() == V && !CS.doesNotCapture(A - B))
          // The parameter is not marked 'nocapture' - captured.
          if (Tracker->captured(U))
            return;
      break;
    }
    case Instruction::Load:
      // Loading from a pointer does not cause it to be captured.
      break;
    case Instruction::VAArg:
      // "va-arg" from a pointer does not cause it to be captured.
      break;
    case Instruction::Store:
      if (V == I->getOperand(0))
        // Stored the pointer - conservatively assume it may be captured.
        if (Tracker->captured(U))
          return;
      // Storing to the pointee does not cause the pointer to be captured.
      break;
    case Instruction::BitCast:
    case Instruction::GetElementPtr:
    case Instruction::PHI:
    case Instruction::Select:
    case Instruction::AddrSpaceCast:
      // The original value is not captured via this if the new value isn't.
      Count = 0;
      for (Use &UU : I->uses()) {
        // If there are lots of uses, conservatively say that the value
        // is captured to avoid taking too much compile time.
        if (Count++ >= Threshold)
          return Tracker->tooManyUses();

        if (Visited.insert(&UU).second)
          if (Tracker->shouldExplore(&UU))
            Worklist.push_back(&UU);
      }
      break;
    case Instruction::ICmp:
      // Don't count comparisons of a no-alias return value against null as
      // captures. This allows us to ignore comparisons of malloc results
      // with null, for example.
      if (ConstantPointerNull *CPN =
          dyn_cast<ConstantPointerNull>(I->getOperand(1)))
        if (CPN->getType()->getAddressSpace() == 0)
          if (isNoAliasCall(V->stripPointerCasts()))
            break;
      // Otherwise, be conservative. There are crazy ways to capture pointers
      // using comparisons.
      if (Tracker->captured(U))
        return;
      break;
    default:
      // Something else - be conservative and say it is captured.
      if (Tracker->captured(U))
        return;
      break;
    }
  }

  // All uses examined.
}
