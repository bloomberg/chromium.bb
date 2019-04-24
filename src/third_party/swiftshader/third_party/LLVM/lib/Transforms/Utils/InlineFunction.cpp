//===- InlineFunction.cpp - Code to perform function inlining -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements inlining of a function into a call site, resolving
// parameters and the return value as appropriate.
//
// The code in this file for handling inlines through invoke
// instructions preserves semantics only under some assumptions about
// the behavior of unwinders which correspond to gcc-style libUnwind
// exception personality functions.  Eventually the IR will be
// improved to make this unnecessary, but until then, this code is
// marked [LIBUNWIND].
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Module.h"
#include "llvm/Instructions.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/Intrinsics.h"
#include "llvm/Attributes.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/DebugInfo.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/IRBuilder.h"
using namespace llvm;

bool llvm::InlineFunction(CallInst *CI, InlineFunctionInfo &IFI) {
  return InlineFunction(CallSite(CI), IFI);
}
bool llvm::InlineFunction(InvokeInst *II, InlineFunctionInfo &IFI) {
  return InlineFunction(CallSite(II), IFI);
}

// FIXME: New EH - Remove the functions marked [LIBUNWIND] when new EH is
// turned on.

/// [LIBUNWIND] Look for an llvm.eh.exception call in the given block.
static EHExceptionInst *findExceptionInBlock(BasicBlock *bb) {
  for (BasicBlock::iterator i = bb->begin(), e = bb->end(); i != e; i++) {
    EHExceptionInst *exn = dyn_cast<EHExceptionInst>(i);
    if (exn) return exn;
  }

  return 0;
}

/// [LIBUNWIND] Look for the 'best' llvm.eh.selector instruction for
/// the given llvm.eh.exception call.
static EHSelectorInst *findSelectorForException(EHExceptionInst *exn) {
  BasicBlock *exnBlock = exn->getParent();

  EHSelectorInst *outOfBlockSelector = 0;
  for (Instruction::use_iterator
         ui = exn->use_begin(), ue = exn->use_end(); ui != ue; ++ui) {
    EHSelectorInst *sel = dyn_cast<EHSelectorInst>(*ui);
    if (!sel) continue;

    // Immediately accept an eh.selector in the same block as the
    // excepton call.
    if (sel->getParent() == exnBlock) return sel;

    // Otherwise, use the first selector we see.
    if (!outOfBlockSelector) outOfBlockSelector = sel;
  }

  return outOfBlockSelector;
}

/// [LIBUNWIND] Find the (possibly absent) call to @llvm.eh.selector
/// in the given landing pad.  In principle, llvm.eh.exception is
/// required to be in the landing pad; in practice, SplitCriticalEdge
/// can break that invariant, and then inlining can break it further.
/// There's a real need for a reliable solution here, but until that
/// happens, we have some fragile workarounds here.
static EHSelectorInst *findSelectorForLandingPad(BasicBlock *lpad) {
  // Look for an exception call in the actual landing pad.
  EHExceptionInst *exn = findExceptionInBlock(lpad);
  if (exn) return findSelectorForException(exn);

  // Okay, if that failed, look for one in an obvious successor.  If
  // we find one, we'll fix the IR by moving things back to the
  // landing pad.

  bool dominates = true; // does the lpad dominate the exn call
  BasicBlock *nonDominated = 0; // if not, the first non-dominated block
  BasicBlock *lastDominated = 0; // and the block which branched to it

  BasicBlock *exnBlock = lpad;

  // We need to protect against lpads that lead into infinite loops.
  SmallPtrSet<BasicBlock*,4> visited;
  visited.insert(exnBlock);

  do {
    // We're not going to apply this hack to anything more complicated
    // than a series of unconditional branches, so if the block
    // doesn't terminate in an unconditional branch, just fail.  More
    // complicated cases can arise when, say, sinking a call into a
    // split unwind edge and then inlining it; but that can do almost
    // *anything* to the CFG, including leaving the selector
    // completely unreachable.  The only way to fix that properly is
    // to (1) prohibit transforms which move the exception or selector
    // values away from the landing pad, e.g. by producing them with
    // instructions that are pinned to an edge like a phi, or
    // producing them with not-really-instructions, and (2) making
    // transforms which split edges deal with that.
    BranchInst *branch = dyn_cast<BranchInst>(&exnBlock->back());
    if (!branch || branch->isConditional()) return 0;

    BasicBlock *successor = branch->getSuccessor(0);

    // Fail if we found an infinite loop.
    if (!visited.insert(successor)) return 0;

    // If the successor isn't dominated by exnBlock:
    if (!successor->getSinglePredecessor()) {
      // We don't want to have to deal with threading the exception
      // through multiple levels of phi, so give up if we've already
      // followed a non-dominating edge.
      if (!dominates) return 0;

      // Otherwise, remember this as a non-dominating edge.
      dominates = false;
      nonDominated = successor;
      lastDominated = exnBlock;
    }

    exnBlock = successor;

    // Can we stop here?
    exn = findExceptionInBlock(exnBlock);
  } while (!exn);

  // Look for a selector call for the exception we found.
  EHSelectorInst *selector = findSelectorForException(exn);
  if (!selector) return 0;

  // The easy case is when the landing pad still dominates the
  // exception call, in which case we can just move both calls back to
  // the landing pad.
  if (dominates) {
    selector->moveBefore(lpad->getFirstNonPHI());
    exn->moveBefore(selector);
    return selector;
  }

  // Otherwise, we have to split at the first non-dominating block.
  // The CFG looks basically like this:
  //    lpad:
  //      phis_0
  //      insnsAndBranches_1
  //      br label %nonDominated
  //    nonDominated:
  //      phis_2
  //      insns_3
  //      %exn = call i8* @llvm.eh.exception()
  //      insnsAndBranches_4
  //      %selector = call @llvm.eh.selector(i8* %exn, ...
  // We need to turn this into:
  //    lpad:
  //      phis_0
  //      %exn0 = call i8* @llvm.eh.exception()
  //      %selector0 = call @llvm.eh.selector(i8* %exn0, ...
  //      insnsAndBranches_1
  //      br label %split // from lastDominated
  //    nonDominated:
  //      phis_2 (without edge from lastDominated)
  //      %exn1 = call i8* @llvm.eh.exception()
  //      %selector1 = call i8* @llvm.eh.selector(i8* %exn1, ...
  //      br label %split
  //    split:
  //      phis_2 (edge from lastDominated, edge from split)
  //      %exn = phi ...
  //      %selector = phi ...
  //      insns_3
  //      insnsAndBranches_4

  assert(nonDominated);
  assert(lastDominated);

  // First, make clones of the intrinsics to go in lpad.
  EHExceptionInst *lpadExn = cast<EHExceptionInst>(exn->clone());
  EHSelectorInst *lpadSelector = cast<EHSelectorInst>(selector->clone());
  lpadSelector->setArgOperand(0, lpadExn);
  lpadSelector->insertBefore(lpad->getFirstNonPHI());
  lpadExn->insertBefore(lpadSelector);

  // Split the non-dominated block.
  BasicBlock *split =
    nonDominated->splitBasicBlock(nonDominated->getFirstNonPHI(),
                                  nonDominated->getName() + ".lpad-fix");

  // Redirect the last dominated branch there.
  cast<BranchInst>(lastDominated->back()).setSuccessor(0, split);

  // Move the existing intrinsics to the end of the old block.
  selector->moveBefore(&nonDominated->back());
  exn->moveBefore(selector);

  Instruction *splitIP = &split->front();

  // For all the phis in nonDominated, make a new phi in split to join
  // that phi with the edge from lastDominated.
  for (BasicBlock::iterator
         i = nonDominated->begin(), e = nonDominated->end(); i != e; ++i) {
    PHINode *phi = dyn_cast<PHINode>(i);
    if (!phi) break;

    PHINode *splitPhi = PHINode::Create(phi->getType(), 2, phi->getName(),
                                        splitIP);
    phi->replaceAllUsesWith(splitPhi);
    splitPhi->addIncoming(phi, nonDominated);
    splitPhi->addIncoming(phi->removeIncomingValue(lastDominated),
                          lastDominated);
  }

  // Make new phis for the exception and selector.
  PHINode *exnPhi = PHINode::Create(exn->getType(), 2, "", splitIP);
  exn->replaceAllUsesWith(exnPhi);
  selector->setArgOperand(0, exn); // except for this use
  exnPhi->addIncoming(exn, nonDominated);
  exnPhi->addIncoming(lpadExn, lastDominated);

  PHINode *selectorPhi = PHINode::Create(selector->getType(), 2, "", splitIP);
  selector->replaceAllUsesWith(selectorPhi);
  selectorPhi->addIncoming(selector, nonDominated);
  selectorPhi->addIncoming(lpadSelector, lastDominated);

  return lpadSelector;
}

namespace {
  /// A class for recording information about inlining through an invoke.
  class InvokeInliningInfo {
    BasicBlock *OuterUnwindDest;
    EHSelectorInst *OuterSelector;
    BasicBlock *InnerUnwindDest;
    PHINode *InnerExceptionPHI;
    PHINode *InnerSelectorPHI;
    SmallVector<Value*, 8> UnwindDestPHIValues;

    // FIXME: New EH - These will replace the analogous ones above.
    BasicBlock *OuterResumeDest; //< Destination of the invoke's unwind.
    BasicBlock *InnerResumeDest; //< Destination for the callee's resume.
    LandingPadInst *CallerLPad;  //< LandingPadInst associated with the invoke.
    PHINode *InnerEHValuesPHI;   //< PHI for EH values from landingpad insts.

  public:
    InvokeInliningInfo(InvokeInst *II)
      : OuterUnwindDest(II->getUnwindDest()), OuterSelector(0),
        InnerUnwindDest(0), InnerExceptionPHI(0), InnerSelectorPHI(0),
        OuterResumeDest(II->getUnwindDest()), InnerResumeDest(0),
        CallerLPad(0), InnerEHValuesPHI(0) {
      // If there are PHI nodes in the unwind destination block, we need to keep
      // track of which values came into them from the invoke before removing
      // the edge from this block.
      llvm::BasicBlock *InvokeBB = II->getParent();
      BasicBlock::iterator I = OuterUnwindDest->begin();
      for (; isa<PHINode>(I); ++I) {
        // Save the value to use for this edge.
        PHINode *PHI = cast<PHINode>(I);
        UnwindDestPHIValues.push_back(PHI->getIncomingValueForBlock(InvokeBB));
      }

      // FIXME: With the new EH, this if/dyn_cast should be a 'cast'.
      if (LandingPadInst *LPI = dyn_cast<LandingPadInst>(I)) {
        CallerLPad = LPI;
      }
    }

    /// The outer unwind destination is the target of unwind edges
    /// introduced for calls within the inlined function.
    BasicBlock *getOuterUnwindDest() const {
      return OuterUnwindDest;
    }

    EHSelectorInst *getOuterSelector() {
      if (!OuterSelector)
        OuterSelector = findSelectorForLandingPad(OuterUnwindDest);
      return OuterSelector;
    }

    BasicBlock *getInnerUnwindDest();

    // FIXME: New EH - Rename when new EH is turned on.
    BasicBlock *getInnerUnwindDestNewEH();

    LandingPadInst *getLandingPadInst() const { return CallerLPad; }

    bool forwardEHResume(CallInst *call, BasicBlock *src);

    /// forwardResume - Forward the 'resume' instruction to the caller's landing
    /// pad block. When the landing pad block has only one predecessor, this is
    /// a simple branch. When there is more than one predecessor, we need to
    /// split the landing pad block after the landingpad instruction and jump
    /// to there.
    void forwardResume(ResumeInst *RI);

    /// addIncomingPHIValuesFor - Add incoming-PHI values to the unwind
    /// destination block for the given basic block, using the values for the
    /// original invoke's source block.
    void addIncomingPHIValuesFor(BasicBlock *BB) const {
      addIncomingPHIValuesForInto(BB, OuterUnwindDest);
    }

    void addIncomingPHIValuesForInto(BasicBlock *src, BasicBlock *dest) const {
      BasicBlock::iterator I = dest->begin();
      for (unsigned i = 0, e = UnwindDestPHIValues.size(); i != e; ++i, ++I) {
        PHINode *phi = cast<PHINode>(I);
        phi->addIncoming(UnwindDestPHIValues[i], src);
      }
    }
  };
}

/// [LIBUNWIND] Get or create a target for the branch out of rewritten calls to
/// llvm.eh.resume.
BasicBlock *InvokeInliningInfo::getInnerUnwindDest() {
  if (InnerUnwindDest) return InnerUnwindDest;

  // Find and hoist the llvm.eh.exception and llvm.eh.selector calls
  // in the outer landing pad to immediately following the phis.
  EHSelectorInst *selector = getOuterSelector();
  if (!selector) return 0;

  // The call to llvm.eh.exception *must* be in the landing pad.
  Instruction *exn = cast<Instruction>(selector->getArgOperand(0));
  assert(exn->getParent() == OuterUnwindDest);

  // TODO: recognize when we've already done this, so that we don't
  // get a linear number of these when inlining calls into lots of
  // invokes with the same landing pad.

  // Do the hoisting.
  Instruction *splitPoint = exn->getParent()->getFirstNonPHI();
  assert(splitPoint != selector && "selector-on-exception dominance broken!");
  if (splitPoint == exn) {
    selector->removeFromParent();
    selector->insertAfter(exn);
    splitPoint = selector->getNextNode();
  } else {
    exn->moveBefore(splitPoint);
    selector->moveBefore(splitPoint);
  }

  // Split the landing pad.
  InnerUnwindDest = OuterUnwindDest->splitBasicBlock(splitPoint,
                                        OuterUnwindDest->getName() + ".body");

  // The number of incoming edges we expect to the inner landing pad.
  const unsigned phiCapacity = 2;

  // Create corresponding new phis for all the phis in the outer landing pad.
  BasicBlock::iterator insertPoint = InnerUnwindDest->begin();
  BasicBlock::iterator I = OuterUnwindDest->begin();
  for (unsigned i = 0, e = UnwindDestPHIValues.size(); i != e; ++i, ++I) {
    PHINode *outerPhi = cast<PHINode>(I);
    PHINode *innerPhi = PHINode::Create(outerPhi->getType(), phiCapacity,
                                        outerPhi->getName() + ".lpad-body",
                                        insertPoint);
    outerPhi->replaceAllUsesWith(innerPhi);
    innerPhi->addIncoming(outerPhi, OuterUnwindDest);
  }

  // Create a phi for the exception value...
  InnerExceptionPHI = PHINode::Create(exn->getType(), phiCapacity,
                                      "exn.lpad-body", insertPoint);
  exn->replaceAllUsesWith(InnerExceptionPHI);
  selector->setArgOperand(0, exn); // restore this use
  InnerExceptionPHI->addIncoming(exn, OuterUnwindDest);

  // ...and the selector.
  InnerSelectorPHI = PHINode::Create(selector->getType(), phiCapacity,
                                     "selector.lpad-body", insertPoint);
  selector->replaceAllUsesWith(InnerSelectorPHI);
  InnerSelectorPHI->addIncoming(selector, OuterUnwindDest);

  // All done.
  return InnerUnwindDest;
}

/// [LIBUNWIND] Try to forward the given call, which logically occurs
/// at the end of the given block, as a branch to the inner unwind
/// block.  Returns true if the call was forwarded.
bool InvokeInliningInfo::forwardEHResume(CallInst *call, BasicBlock *src) {
  // First, check whether this is a call to the intrinsic.
  Function *fn = dyn_cast<Function>(call->getCalledValue());
  if (!fn || fn->getName() != "llvm.eh.resume")
    return false;
  
  // At this point, we need to return true on all paths, because
  // otherwise we'll construct an invoke of the intrinsic, which is
  // not well-formed.

  // Try to find or make an inner unwind dest, which will fail if we
  // can't find a selector call for the outer unwind dest.
  BasicBlock *dest = getInnerUnwindDest();
  bool hasSelector = (dest != 0);

  // If we failed, just use the outer unwind dest, dropping the
  // exception and selector on the floor.
  if (!hasSelector)
    dest = OuterUnwindDest;

  // Make a branch.
  BranchInst::Create(dest, src);

  // Update the phis in the destination.  They were inserted in an
  // order which makes this work.
  addIncomingPHIValuesForInto(src, dest);

  if (hasSelector) {
    InnerExceptionPHI->addIncoming(call->getArgOperand(0), src);
    InnerSelectorPHI->addIncoming(call->getArgOperand(1), src);
  }

  return true;
}

/// Get or create a target for the branch from ResumeInsts.
BasicBlock *InvokeInliningInfo::getInnerUnwindDestNewEH() {
  // FIXME: New EH - rename this function when new EH is turned on.
  if (InnerResumeDest) return InnerResumeDest;

  // Split the landing pad.
  BasicBlock::iterator SplitPoint = CallerLPad; ++SplitPoint;
  InnerResumeDest =
    OuterResumeDest->splitBasicBlock(SplitPoint,
                                     OuterResumeDest->getName() + ".body");

  // The number of incoming edges we expect to the inner landing pad.
  const unsigned PHICapacity = 2;

  // Create corresponding new PHIs for all the PHIs in the outer landing pad.
  BasicBlock::iterator InsertPoint = InnerResumeDest->begin();
  BasicBlock::iterator I = OuterResumeDest->begin();
  for (unsigned i = 0, e = UnwindDestPHIValues.size(); i != e; ++i, ++I) {
    PHINode *OuterPHI = cast<PHINode>(I);
    PHINode *InnerPHI = PHINode::Create(OuterPHI->getType(), PHICapacity,
                                        OuterPHI->getName() + ".lpad-body",
                                        InsertPoint);
    OuterPHI->replaceAllUsesWith(InnerPHI);
    InnerPHI->addIncoming(OuterPHI, OuterResumeDest);
  }

  // Create a PHI for the exception values.
  InnerEHValuesPHI = PHINode::Create(CallerLPad->getType(), PHICapacity,
                                     "eh.lpad-body", InsertPoint);
  CallerLPad->replaceAllUsesWith(InnerEHValuesPHI);
  InnerEHValuesPHI->addIncoming(CallerLPad, OuterResumeDest);

  // All done.
  return InnerResumeDest;
}

/// forwardResume - Forward the 'resume' instruction to the caller's landing pad
/// block. When the landing pad block has only one predecessor, this is a simple
/// branch. When there is more than one predecessor, we need to split the
/// landing pad block after the landingpad instruction and jump to there.
void InvokeInliningInfo::forwardResume(ResumeInst *RI) {
  BasicBlock *Dest = getInnerUnwindDestNewEH();
  BasicBlock *Src = RI->getParent();

  BranchInst::Create(Dest, Src);

  // Update the PHIs in the destination. They were inserted in an order which
  // makes this work.
  addIncomingPHIValuesForInto(Src, Dest);

  InnerEHValuesPHI->addIncoming(RI->getOperand(0), Src);
  RI->eraseFromParent();
}

/// [LIBUNWIND] Check whether this selector is "only cleanups":
///   call i32 @llvm.eh.selector(blah, blah, i32 0)
static bool isCleanupOnlySelector(EHSelectorInst *selector) {
  if (selector->getNumArgOperands() != 3) return false;
  ConstantInt *val = dyn_cast<ConstantInt>(selector->getArgOperand(2));
  return (val && val->isZero());
}

/// HandleCallsInBlockInlinedThroughInvoke - When we inline a basic block into
/// an invoke, we have to turn all of the calls that can throw into
/// invokes.  This function analyze BB to see if there are any calls, and if so,
/// it rewrites them to be invokes that jump to InvokeDest and fills in the PHI
/// nodes in that block with the values specified in InvokeDestPHIValues.
///
/// Returns true to indicate that the next block should be skipped.
static bool HandleCallsInBlockInlinedThroughInvoke(BasicBlock *BB,
                                                   InvokeInliningInfo &Invoke) {
  LandingPadInst *LPI = Invoke.getLandingPadInst();

  for (BasicBlock::iterator BBI = BB->begin(), E = BB->end(); BBI != E; ) {
    Instruction *I = BBI++;

    if (LPI) // FIXME: New EH - This won't be NULL in the new EH.
      if (LandingPadInst *L = dyn_cast<LandingPadInst>(I)) {
        unsigned NumClauses = LPI->getNumClauses();
        L->reserveClauses(NumClauses);
        for (unsigned i = 0; i != NumClauses; ++i)
          L->addClause(LPI->getClause(i));
      }

    // We only need to check for function calls: inlined invoke
    // instructions require no special handling.
    CallInst *CI = dyn_cast<CallInst>(I);
    if (CI == 0) continue;

    // LIBUNWIND: merge selector instructions.
    if (EHSelectorInst *Inner = dyn_cast<EHSelectorInst>(CI)) {
      EHSelectorInst *Outer = Invoke.getOuterSelector();
      if (!Outer) continue;

      bool innerIsOnlyCleanup = isCleanupOnlySelector(Inner);
      bool outerIsOnlyCleanup = isCleanupOnlySelector(Outer);

      // If both selectors contain only cleanups, we don't need to do
      // anything.  TODO: this is really just a very specific instance
      // of a much more general optimization.
      if (innerIsOnlyCleanup && outerIsOnlyCleanup) continue;

      // Otherwise, we just append the outer selector to the inner selector.
      SmallVector<Value*, 16> NewSelector;
      for (unsigned i = 0, e = Inner->getNumArgOperands(); i != e; ++i)
        NewSelector.push_back(Inner->getArgOperand(i));
      for (unsigned i = 2, e = Outer->getNumArgOperands(); i != e; ++i)
        NewSelector.push_back(Outer->getArgOperand(i));

      CallInst *NewInner =
        IRBuilder<>(Inner).CreateCall(Inner->getCalledValue(), NewSelector);
      // No need to copy attributes, calling convention, etc.
      NewInner->takeName(Inner);
      Inner->replaceAllUsesWith(NewInner);
      Inner->eraseFromParent();
      continue;
    }
    
    // If this call cannot unwind, don't convert it to an invoke.
    if (CI->doesNotThrow())
      continue;
    
    // Convert this function call into an invoke instruction.
    // First, split the basic block.
    BasicBlock *Split = BB->splitBasicBlock(CI, CI->getName()+".noexc");

    // Delete the unconditional branch inserted by splitBasicBlock
    BB->getInstList().pop_back();

    // LIBUNWIND: If this is a call to @llvm.eh.resume, just branch
    // directly to the new landing pad.
    if (Invoke.forwardEHResume(CI, BB)) {
      // TODO: 'Split' is now unreachable; clean it up.

      // We want to leave the original call intact so that the call
      // graph and other structures won't get misled.  We also have to
      // avoid processing the next block, or we'll iterate here forever.
      return true;
    }

    // Otherwise, create the new invoke instruction.
    ImmutableCallSite CS(CI);
    SmallVector<Value*, 8> InvokeArgs(CS.arg_begin(), CS.arg_end());
    InvokeInst *II =
      InvokeInst::Create(CI->getCalledValue(), Split,
                         Invoke.getOuterUnwindDest(),
                         InvokeArgs, CI->getName(), BB);
    II->setCallingConv(CI->getCallingConv());
    II->setAttributes(CI->getAttributes());
    
    // Make sure that anything using the call now uses the invoke!  This also
    // updates the CallGraph if present, because it uses a WeakVH.
    CI->replaceAllUsesWith(II);

    Split->getInstList().pop_front();  // Delete the original call

    // Update any PHI nodes in the exceptional block to indicate that
    // there is now a new entry in them.
    Invoke.addIncomingPHIValuesFor(BB);
    return false;
  }

  return false;
}
  

/// HandleInlinedInvoke - If we inlined an invoke site, we need to convert calls
/// in the body of the inlined function into invokes and turn unwind
/// instructions into branches to the invoke unwind dest.
///
/// II is the invoke instruction being inlined.  FirstNewBlock is the first
/// block of the inlined code (the last block is the end of the function),
/// and InlineCodeInfo is information about the code that got inlined.
static void HandleInlinedInvoke(InvokeInst *II, BasicBlock *FirstNewBlock,
                                ClonedCodeInfo &InlinedCodeInfo) {
  BasicBlock *InvokeDest = II->getUnwindDest();

  Function *Caller = FirstNewBlock->getParent();

  // The inlined code is currently at the end of the function, scan from the
  // start of the inlined code to its end, checking for stuff we need to
  // rewrite.  If the code doesn't have calls or unwinds, we know there is
  // nothing to rewrite.
  if (!InlinedCodeInfo.ContainsCalls && !InlinedCodeInfo.ContainsUnwinds) {
    // Now that everything is happy, we have one final detail.  The PHI nodes in
    // the exception destination block still have entries due to the original
    // invoke instruction.  Eliminate these entries (which might even delete the
    // PHI node) now.
    InvokeDest->removePredecessor(II->getParent());
    return;
  }

  InvokeInliningInfo Invoke(II);
  
  for (Function::iterator BB = FirstNewBlock, E = Caller->end(); BB != E; ++BB){
    if (InlinedCodeInfo.ContainsCalls)
      if (HandleCallsInBlockInlinedThroughInvoke(BB, Invoke)) {
        // Honor a request to skip the next block.  We don't need to
        // consider UnwindInsts in this case either.
        ++BB;
        continue;
      }

    if (UnwindInst *UI = dyn_cast<UnwindInst>(BB->getTerminator())) {
      // An UnwindInst requires special handling when it gets inlined into an
      // invoke site.  Once this happens, we know that the unwind would cause
      // a control transfer to the invoke exception destination, so we can
      // transform it into a direct branch to the exception destination.
      BranchInst::Create(InvokeDest, UI);

      // Delete the unwind instruction!
      UI->eraseFromParent();

      // Update any PHI nodes in the exceptional block to indicate that
      // there is now a new entry in them.
      Invoke.addIncomingPHIValuesFor(BB);
    }

    if (ResumeInst *RI = dyn_cast<ResumeInst>(BB->getTerminator())) {
      Invoke.forwardResume(RI);
    }
  }

  // Now that everything is happy, we have one final detail.  The PHI nodes in
  // the exception destination block still have entries due to the original
  // invoke instruction.  Eliminate these entries (which might even delete the
  // PHI node) now.
  InvokeDest->removePredecessor(II->getParent());
}

/// UpdateCallGraphAfterInlining - Once we have cloned code over from a callee
/// into the caller, update the specified callgraph to reflect the changes we
/// made.  Note that it's possible that not all code was copied over, so only
/// some edges of the callgraph may remain.
static void UpdateCallGraphAfterInlining(CallSite CS,
                                         Function::iterator FirstNewBlock,
                                         ValueToValueMapTy &VMap,
                                         InlineFunctionInfo &IFI) {
  CallGraph &CG = *IFI.CG;
  const Function *Caller = CS.getInstruction()->getParent()->getParent();
  const Function *Callee = CS.getCalledFunction();
  CallGraphNode *CalleeNode = CG[Callee];
  CallGraphNode *CallerNode = CG[Caller];

  // Since we inlined some uninlined call sites in the callee into the caller,
  // add edges from the caller to all of the callees of the callee.
  CallGraphNode::iterator I = CalleeNode->begin(), E = CalleeNode->end();

  // Consider the case where CalleeNode == CallerNode.
  CallGraphNode::CalledFunctionsVector CallCache;
  if (CalleeNode == CallerNode) {
    CallCache.assign(I, E);
    I = CallCache.begin();
    E = CallCache.end();
  }

  for (; I != E; ++I) {
    const Value *OrigCall = I->first;

    ValueToValueMapTy::iterator VMI = VMap.find(OrigCall);
    // Only copy the edge if the call was inlined!
    if (VMI == VMap.end() || VMI->second == 0)
      continue;
    
    // If the call was inlined, but then constant folded, there is no edge to
    // add.  Check for this case.
    Instruction *NewCall = dyn_cast<Instruction>(VMI->second);
    if (NewCall == 0) continue;

    // Remember that this call site got inlined for the client of
    // InlineFunction.
    IFI.InlinedCalls.push_back(NewCall);

    // It's possible that inlining the callsite will cause it to go from an
    // indirect to a direct call by resolving a function pointer.  If this
    // happens, set the callee of the new call site to a more precise
    // destination.  This can also happen if the call graph node of the caller
    // was just unnecessarily imprecise.
    if (I->second->getFunction() == 0)
      if (Function *F = CallSite(NewCall).getCalledFunction()) {
        // Indirect call site resolved to direct call.
        CallerNode->addCalledFunction(CallSite(NewCall), CG[F]);

        continue;
      }

    CallerNode->addCalledFunction(CallSite(NewCall), I->second);
  }
  
  // Update the call graph by deleting the edge from Callee to Caller.  We must
  // do this after the loop above in case Caller and Callee are the same.
  CallerNode->removeCallEdgeFor(CS);
}

/// HandleByValArgument - When inlining a call site that has a byval argument,
/// we have to make the implicit memcpy explicit by adding it.
static Value *HandleByValArgument(Value *Arg, Instruction *TheCall,
                                  const Function *CalledFunc,
                                  InlineFunctionInfo &IFI,
                                  unsigned ByValAlignment) {
  Type *AggTy = cast<PointerType>(Arg->getType())->getElementType();

  // If the called function is readonly, then it could not mutate the caller's
  // copy of the byval'd memory.  In this case, it is safe to elide the copy and
  // temporary.
  if (CalledFunc->onlyReadsMemory()) {
    // If the byval argument has a specified alignment that is greater than the
    // passed in pointer, then we either have to round up the input pointer or
    // give up on this transformation.
    if (ByValAlignment <= 1)  // 0 = unspecified, 1 = no particular alignment.
      return Arg;

    // If the pointer is already known to be sufficiently aligned, or if we can
    // round it up to a larger alignment, then we don't need a temporary.
    if (getOrEnforceKnownAlignment(Arg, ByValAlignment,
                                   IFI.TD) >= ByValAlignment)
      return Arg;
    
    // Otherwise, we have to make a memcpy to get a safe alignment.  This is bad
    // for code quality, but rarely happens and is required for correctness.
  }
  
  LLVMContext &Context = Arg->getContext();

  Type *VoidPtrTy = Type::getInt8PtrTy(Context);
  
  // Create the alloca.  If we have TargetData, use nice alignment.
  unsigned Align = 1;
  if (IFI.TD)
    Align = IFI.TD->getPrefTypeAlignment(AggTy);
  
  // If the byval had an alignment specified, we *must* use at least that
  // alignment, as it is required by the byval argument (and uses of the
  // pointer inside the callee).
  Align = std::max(Align, ByValAlignment);
  
  Function *Caller = TheCall->getParent()->getParent(); 
  
  Value *NewAlloca = new AllocaInst(AggTy, 0, Align, Arg->getName(), 
                                    &*Caller->begin()->begin());
  // Emit a memcpy.
  Type *Tys[3] = {VoidPtrTy, VoidPtrTy, Type::getInt64Ty(Context)};
  Function *MemCpyFn = Intrinsic::getDeclaration(Caller->getParent(),
                                                 Intrinsic::memcpy, 
                                                 Tys);
  Value *DestCast = new BitCastInst(NewAlloca, VoidPtrTy, "tmp", TheCall);
  Value *SrcCast = new BitCastInst(Arg, VoidPtrTy, "tmp", TheCall);
  
  Value *Size;
  if (IFI.TD == 0)
    Size = ConstantExpr::getSizeOf(AggTy);
  else
    Size = ConstantInt::get(Type::getInt64Ty(Context),
                            IFI.TD->getTypeStoreSize(AggTy));
  
  // Always generate a memcpy of alignment 1 here because we don't know
  // the alignment of the src pointer.  Other optimizations can infer
  // better alignment.
  Value *CallArgs[] = {
    DestCast, SrcCast, Size,
    ConstantInt::get(Type::getInt32Ty(Context), 1),
    ConstantInt::getFalse(Context) // isVolatile
  };
  IRBuilder<>(TheCall).CreateCall(MemCpyFn, CallArgs);
  
  // Uses of the argument in the function should use our new alloca
  // instead.
  return NewAlloca;
}

// isUsedByLifetimeMarker - Check whether this Value is used by a lifetime
// intrinsic.
static bool isUsedByLifetimeMarker(Value *V) {
  for (Value::use_iterator UI = V->use_begin(), UE = V->use_end(); UI != UE;
       ++UI) {
    if (IntrinsicInst *II = dyn_cast<IntrinsicInst>(*UI)) {
      switch (II->getIntrinsicID()) {
      default: break;
      case Intrinsic::lifetime_start:
      case Intrinsic::lifetime_end:
        return true;
      }
    }
  }
  return false;
}

// hasLifetimeMarkers - Check whether the given alloca already has
// lifetime.start or lifetime.end intrinsics.
static bool hasLifetimeMarkers(AllocaInst *AI) {
  Type *Int8PtrTy = Type::getInt8PtrTy(AI->getType()->getContext());
  if (AI->getType() == Int8PtrTy)
    return isUsedByLifetimeMarker(AI);

  // Do a scan to find all the casts to i8*.
  for (Value::use_iterator I = AI->use_begin(), E = AI->use_end(); I != E;
       ++I) {
    if (I->getType() != Int8PtrTy) continue;
    if (I->stripPointerCasts() != AI) continue;
    if (isUsedByLifetimeMarker(*I))
      return true;
  }
  return false;
}

/// updateInlinedAtInfo - Helper function used by fixupLineNumbers to recursively
/// update InlinedAtEntry of a DebugLoc.
static DebugLoc updateInlinedAtInfo(const DebugLoc &DL, 
                                    const DebugLoc &InlinedAtDL,
                                    LLVMContext &Ctx) {
  if (MDNode *IA = DL.getInlinedAt(Ctx)) {
    DebugLoc NewInlinedAtDL 
      = updateInlinedAtInfo(DebugLoc::getFromDILocation(IA), InlinedAtDL, Ctx);
    return DebugLoc::get(DL.getLine(), DL.getCol(), DL.getScope(Ctx),
                         NewInlinedAtDL.getAsMDNode(Ctx));
  }
                                             
  return DebugLoc::get(DL.getLine(), DL.getCol(), DL.getScope(Ctx),
                       InlinedAtDL.getAsMDNode(Ctx));
}


/// fixupLineNumbers - Update inlined instructions' line numbers to 
/// to encode location where these instructions are inlined.
static void fixupLineNumbers(Function *Fn, Function::iterator FI,
                              Instruction *TheCall) {
  DebugLoc TheCallDL = TheCall->getDebugLoc();
  if (TheCallDL.isUnknown())
    return;

  for (; FI != Fn->end(); ++FI) {
    for (BasicBlock::iterator BI = FI->begin(), BE = FI->end();
         BI != BE; ++BI) {
      DebugLoc DL = BI->getDebugLoc();
      if (!DL.isUnknown()) {
        BI->setDebugLoc(updateInlinedAtInfo(DL, TheCallDL, BI->getContext()));
        if (DbgValueInst *DVI = dyn_cast<DbgValueInst>(BI)) {
          LLVMContext &Ctx = BI->getContext();
          MDNode *InlinedAt = BI->getDebugLoc().getInlinedAt(Ctx);
          DVI->setOperand(2, createInlinedVariable(DVI->getVariable(), 
                                                   InlinedAt, Ctx));
        }
      }
    }
  }
}

// InlineFunction - This function inlines the called function into the basic
// block of the caller.  This returns false if it is not possible to inline this
// call.  The program is still in a well defined state if this occurs though.
//
// Note that this only does one level of inlining.  For example, if the
// instruction 'call B' is inlined, and 'B' calls 'C', then the call to 'C' now
// exists in the instruction stream.  Similarly this will inline a recursive
// function by one level.
//
bool llvm::InlineFunction(CallSite CS, InlineFunctionInfo &IFI) {
  Instruction *TheCall = CS.getInstruction();
  LLVMContext &Context = TheCall->getContext();
  assert(TheCall->getParent() && TheCall->getParent()->getParent() &&
         "Instruction not in function!");

  // If IFI has any state in it, zap it before we fill it in.
  IFI.reset();
  
  const Function *CalledFunc = CS.getCalledFunction();
  if (CalledFunc == 0 ||          // Can't inline external function or indirect
      CalledFunc->isDeclaration() || // call, or call to a vararg function!
      CalledFunc->getFunctionType()->isVarArg()) return false;

  // If the call to the callee is not a tail call, we must clear the 'tail'
  // flags on any calls that we inline.
  bool MustClearTailCallFlags =
    !(isa<CallInst>(TheCall) && cast<CallInst>(TheCall)->isTailCall());

  // If the call to the callee cannot throw, set the 'nounwind' flag on any
  // calls that we inline.
  bool MarkNoUnwind = CS.doesNotThrow();

  BasicBlock *OrigBB = TheCall->getParent();
  Function *Caller = OrigBB->getParent();

  // GC poses two hazards to inlining, which only occur when the callee has GC:
  //  1. If the caller has no GC, then the callee's GC must be propagated to the
  //     caller.
  //  2. If the caller has a differing GC, it is invalid to inline.
  if (CalledFunc->hasGC()) {
    if (!Caller->hasGC())
      Caller->setGC(CalledFunc->getGC());
    else if (CalledFunc->getGC() != Caller->getGC())
      return false;
  }

  // Find the personality function used by the landing pads of the caller. If it
  // exists, then check to see that it matches the personality function used in
  // the callee.
  for (Function::const_iterator
         I = Caller->begin(), E = Caller->end(); I != E; ++I)
    if (const InvokeInst *II = dyn_cast<InvokeInst>(I->getTerminator())) {
      const BasicBlock *BB = II->getUnwindDest();
      // FIXME: This 'isa' here should become go away once the new EH system is
      // in place.
      if (!isa<LandingPadInst>(BB->getFirstNonPHI()))
        continue;
      const LandingPadInst *LP = cast<LandingPadInst>(BB->getFirstNonPHI());
      const Value *CallerPersFn = LP->getPersonalityFn();

      // If the personality functions match, then we can perform the
      // inlining. Otherwise, we can't inline.
      // TODO: This isn't 100% true. Some personality functions are proper
      //       supersets of others and can be used in place of the other.
      for (Function::const_iterator
             I = CalledFunc->begin(), E = CalledFunc->end(); I != E; ++I)
        if (const InvokeInst *II = dyn_cast<InvokeInst>(I->getTerminator())) {
          const BasicBlock *BB = II->getUnwindDest();
          // FIXME: This 'if/dyn_cast' here should become a normal 'cast' once
          // the new EH system is in place.
          if (const LandingPadInst *LP =
              dyn_cast<LandingPadInst>(BB->getFirstNonPHI()))
            if (CallerPersFn != LP->getPersonalityFn())
              return false;
          break;
        }

      break;
    }

  // Get an iterator to the last basic block in the function, which will have
  // the new function inlined after it.
  //
  Function::iterator LastBlock = &Caller->back();

  // Make sure to capture all of the return instructions from the cloned
  // function.
  SmallVector<ReturnInst*, 8> Returns;
  ClonedCodeInfo InlinedFunctionInfo;
  Function::iterator FirstNewBlock;

  { // Scope to destroy VMap after cloning.
    ValueToValueMapTy VMap;

    assert(CalledFunc->arg_size() == CS.arg_size() &&
           "No varargs calls can be inlined!");

    // Calculate the vector of arguments to pass into the function cloner, which
    // matches up the formal to the actual argument values.
    CallSite::arg_iterator AI = CS.arg_begin();
    unsigned ArgNo = 0;
    for (Function::const_arg_iterator I = CalledFunc->arg_begin(),
         E = CalledFunc->arg_end(); I != E; ++I, ++AI, ++ArgNo) {
      Value *ActualArg = *AI;

      // When byval arguments actually inlined, we need to make the copy implied
      // by them explicit.  However, we don't do this if the callee is readonly
      // or readnone, because the copy would be unneeded: the callee doesn't
      // modify the struct.
      if (CalledFunc->paramHasAttr(ArgNo+1, Attribute::ByVal)) {
        ActualArg = HandleByValArgument(ActualArg, TheCall, CalledFunc, IFI,
                                        CalledFunc->getParamAlignment(ArgNo+1));
 
        // Calls that we inline may use the new alloca, so we need to clear
        // their 'tail' flags if HandleByValArgument introduced a new alloca and
        // the callee has calls.
        MustClearTailCallFlags |= ActualArg != *AI;
      }

      VMap[I] = ActualArg;
    }

    // We want the inliner to prune the code as it copies.  We would LOVE to
    // have no dead or constant instructions leftover after inlining occurs
    // (which can happen, e.g., because an argument was constant), but we'll be
    // happy with whatever the cloner can do.
    CloneAndPruneFunctionInto(Caller, CalledFunc, VMap, 
                              /*ModuleLevelChanges=*/false, Returns, ".i",
                              &InlinedFunctionInfo, IFI.TD, TheCall);

    // Remember the first block that is newly cloned over.
    FirstNewBlock = LastBlock; ++FirstNewBlock;

    // Update the callgraph if requested.
    if (IFI.CG)
      UpdateCallGraphAfterInlining(CS, FirstNewBlock, VMap, IFI);

    // Update inlined instructions' line number information.
    fixupLineNumbers(Caller, FirstNewBlock, TheCall);
  }

  // If there are any alloca instructions in the block that used to be the entry
  // block for the callee, move them to the entry block of the caller.  First
  // calculate which instruction they should be inserted before.  We insert the
  // instructions at the end of the current alloca list.
  //
  {
    BasicBlock::iterator InsertPoint = Caller->begin()->begin();
    for (BasicBlock::iterator I = FirstNewBlock->begin(),
         E = FirstNewBlock->end(); I != E; ) {
      AllocaInst *AI = dyn_cast<AllocaInst>(I++);
      if (AI == 0) continue;
      
      // If the alloca is now dead, remove it.  This often occurs due to code
      // specialization.
      if (AI->use_empty()) {
        AI->eraseFromParent();
        continue;
      }

      if (!isa<Constant>(AI->getArraySize()))
        continue;
      
      // Keep track of the static allocas that we inline into the caller.
      IFI.StaticAllocas.push_back(AI);
      
      // Scan for the block of allocas that we can move over, and move them
      // all at once.
      while (isa<AllocaInst>(I) &&
             isa<Constant>(cast<AllocaInst>(I)->getArraySize())) {
        IFI.StaticAllocas.push_back(cast<AllocaInst>(I));
        ++I;
      }

      // Transfer all of the allocas over in a block.  Using splice means
      // that the instructions aren't removed from the symbol table, then
      // reinserted.
      Caller->getEntryBlock().getInstList().splice(InsertPoint,
                                                   FirstNewBlock->getInstList(),
                                                   AI, I);
    }
  }

  // Leave lifetime markers for the static alloca's, scoping them to the
  // function we just inlined.
  if (!IFI.StaticAllocas.empty()) {
    IRBuilder<> builder(FirstNewBlock->begin());
    for (unsigned ai = 0, ae = IFI.StaticAllocas.size(); ai != ae; ++ai) {
      AllocaInst *AI = IFI.StaticAllocas[ai];

      // If the alloca is already scoped to something smaller than the whole
      // function then there's no need to add redundant, less accurate markers.
      if (hasLifetimeMarkers(AI))
        continue;

      builder.CreateLifetimeStart(AI);
      for (unsigned ri = 0, re = Returns.size(); ri != re; ++ri) {
        IRBuilder<> builder(Returns[ri]);
        builder.CreateLifetimeEnd(AI);
      }
    }
  }

  // If the inlined code contained dynamic alloca instructions, wrap the inlined
  // code with llvm.stacksave/llvm.stackrestore intrinsics.
  if (InlinedFunctionInfo.ContainsDynamicAllocas) {
    Module *M = Caller->getParent();
    // Get the two intrinsics we care about.
    Function *StackSave = Intrinsic::getDeclaration(M, Intrinsic::stacksave);
    Function *StackRestore=Intrinsic::getDeclaration(M,Intrinsic::stackrestore);

    // Insert the llvm.stacksave.
    CallInst *SavedPtr = IRBuilder<>(FirstNewBlock, FirstNewBlock->begin())
      .CreateCall(StackSave, "savedstack");

    // Insert a call to llvm.stackrestore before any return instructions in the
    // inlined function.
    for (unsigned i = 0, e = Returns.size(); i != e; ++i) {
      IRBuilder<>(Returns[i]).CreateCall(StackRestore, SavedPtr);
    }

    // Count the number of StackRestore calls we insert.
    unsigned NumStackRestores = Returns.size();

    // If we are inlining an invoke instruction, insert restores before each
    // unwind.  These unwinds will be rewritten into branches later.
    if (InlinedFunctionInfo.ContainsUnwinds && isa<InvokeInst>(TheCall)) {
      for (Function::iterator BB = FirstNewBlock, E = Caller->end();
           BB != E; ++BB)
        if (UnwindInst *UI = dyn_cast<UnwindInst>(BB->getTerminator())) {
          IRBuilder<>(UI).CreateCall(StackRestore, SavedPtr);
          ++NumStackRestores;
        }
    }
  }

  // If we are inlining tail call instruction through a call site that isn't
  // marked 'tail', we must remove the tail marker for any calls in the inlined
  // code.  Also, calls inlined through a 'nounwind' call site should be marked
  // 'nounwind'.
  if (InlinedFunctionInfo.ContainsCalls &&
      (MustClearTailCallFlags || MarkNoUnwind)) {
    for (Function::iterator BB = FirstNewBlock, E = Caller->end();
         BB != E; ++BB)
      for (BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I)
        if (CallInst *CI = dyn_cast<CallInst>(I)) {
          if (MustClearTailCallFlags)
            CI->setTailCall(false);
          if (MarkNoUnwind)
            CI->setDoesNotThrow();
        }
  }

  // If we are inlining through a 'nounwind' call site then any inlined 'unwind'
  // instructions are unreachable.
  if (InlinedFunctionInfo.ContainsUnwinds && MarkNoUnwind)
    for (Function::iterator BB = FirstNewBlock, E = Caller->end();
         BB != E; ++BB) {
      TerminatorInst *Term = BB->getTerminator();
      if (isa<UnwindInst>(Term)) {
        new UnreachableInst(Context, Term);
        BB->getInstList().erase(Term);
      }
    }

  // If we are inlining for an invoke instruction, we must make sure to rewrite
  // any inlined 'unwind' instructions into branches to the invoke exception
  // destination, and call instructions into invoke instructions.
  if (InvokeInst *II = dyn_cast<InvokeInst>(TheCall))
    HandleInlinedInvoke(II, FirstNewBlock, InlinedFunctionInfo);

  // If we cloned in _exactly one_ basic block, and if that block ends in a
  // return instruction, we splice the body of the inlined callee directly into
  // the calling basic block.
  if (Returns.size() == 1 && std::distance(FirstNewBlock, Caller->end()) == 1) {
    // Move all of the instructions right before the call.
    OrigBB->getInstList().splice(TheCall, FirstNewBlock->getInstList(),
                                 FirstNewBlock->begin(), FirstNewBlock->end());
    // Remove the cloned basic block.
    Caller->getBasicBlockList().pop_back();

    // If the call site was an invoke instruction, add a branch to the normal
    // destination.
    if (InvokeInst *II = dyn_cast<InvokeInst>(TheCall))
      BranchInst::Create(II->getNormalDest(), TheCall);

    // If the return instruction returned a value, replace uses of the call with
    // uses of the returned value.
    if (!TheCall->use_empty()) {
      ReturnInst *R = Returns[0];
      if (TheCall == R->getReturnValue())
        TheCall->replaceAllUsesWith(UndefValue::get(TheCall->getType()));
      else
        TheCall->replaceAllUsesWith(R->getReturnValue());
    }
    // Since we are now done with the Call/Invoke, we can delete it.
    TheCall->eraseFromParent();

    // Since we are now done with the return instruction, delete it also.
    Returns[0]->eraseFromParent();

    // We are now done with the inlining.
    return true;
  }

  // Otherwise, we have the normal case, of more than one block to inline or
  // multiple return sites.

  // We want to clone the entire callee function into the hole between the
  // "starter" and "ender" blocks.  How we accomplish this depends on whether
  // this is an invoke instruction or a call instruction.
  BasicBlock *AfterCallBB;
  if (InvokeInst *II = dyn_cast<InvokeInst>(TheCall)) {

    // Add an unconditional branch to make this look like the CallInst case...
    BranchInst *NewBr = BranchInst::Create(II->getNormalDest(), TheCall);

    // Split the basic block.  This guarantees that no PHI nodes will have to be
    // updated due to new incoming edges, and make the invoke case more
    // symmetric to the call case.
    AfterCallBB = OrigBB->splitBasicBlock(NewBr,
                                          CalledFunc->getName()+".exit");

  } else {  // It's a call
    // If this is a call instruction, we need to split the basic block that
    // the call lives in.
    //
    AfterCallBB = OrigBB->splitBasicBlock(TheCall,
                                          CalledFunc->getName()+".exit");
  }

  // Change the branch that used to go to AfterCallBB to branch to the first
  // basic block of the inlined function.
  //
  TerminatorInst *Br = OrigBB->getTerminator();
  assert(Br && Br->getOpcode() == Instruction::Br &&
         "splitBasicBlock broken!");
  Br->setOperand(0, FirstNewBlock);


  // Now that the function is correct, make it a little bit nicer.  In
  // particular, move the basic blocks inserted from the end of the function
  // into the space made by splitting the source basic block.
  Caller->getBasicBlockList().splice(AfterCallBB, Caller->getBasicBlockList(),
                                     FirstNewBlock, Caller->end());

  // Handle all of the return instructions that we just cloned in, and eliminate
  // any users of the original call/invoke instruction.
  Type *RTy = CalledFunc->getReturnType();

  PHINode *PHI = 0;
  if (Returns.size() > 1) {
    // The PHI node should go at the front of the new basic block to merge all
    // possible incoming values.
    if (!TheCall->use_empty()) {
      PHI = PHINode::Create(RTy, Returns.size(), TheCall->getName(),
                            AfterCallBB->begin());
      // Anything that used the result of the function call should now use the
      // PHI node as their operand.
      TheCall->replaceAllUsesWith(PHI);
    }

    // Loop over all of the return instructions adding entries to the PHI node
    // as appropriate.
    if (PHI) {
      for (unsigned i = 0, e = Returns.size(); i != e; ++i) {
        ReturnInst *RI = Returns[i];
        assert(RI->getReturnValue()->getType() == PHI->getType() &&
               "Ret value not consistent in function!");
        PHI->addIncoming(RI->getReturnValue(), RI->getParent());
      }
    }


    // Add a branch to the merge points and remove return instructions.
    for (unsigned i = 0, e = Returns.size(); i != e; ++i) {
      ReturnInst *RI = Returns[i];
      BranchInst::Create(AfterCallBB, RI);
      RI->eraseFromParent();
    }
  } else if (!Returns.empty()) {
    // Otherwise, if there is exactly one return value, just replace anything
    // using the return value of the call with the computed value.
    if (!TheCall->use_empty()) {
      if (TheCall == Returns[0]->getReturnValue())
        TheCall->replaceAllUsesWith(UndefValue::get(TheCall->getType()));
      else
        TheCall->replaceAllUsesWith(Returns[0]->getReturnValue());
    }

    // Update PHI nodes that use the ReturnBB to use the AfterCallBB.
    BasicBlock *ReturnBB = Returns[0]->getParent();
    ReturnBB->replaceAllUsesWith(AfterCallBB);

    // Splice the code from the return block into the block that it will return
    // to, which contains the code that was after the call.
    AfterCallBB->getInstList().splice(AfterCallBB->begin(),
                                      ReturnBB->getInstList());

    // Delete the return instruction now and empty ReturnBB now.
    Returns[0]->eraseFromParent();
    ReturnBB->eraseFromParent();
  } else if (!TheCall->use_empty()) {
    // No returns, but something is using the return value of the call.  Just
    // nuke the result.
    TheCall->replaceAllUsesWith(UndefValue::get(TheCall->getType()));
  }

  // Since we are now done with the Call/Invoke, we can delete it.
  TheCall->eraseFromParent();

  // We should always be able to fold the entry block of the function into the
  // single predecessor of the block...
  assert(cast<BranchInst>(Br)->isUnconditional() && "splitBasicBlock broken!");
  BasicBlock *CalleeEntry = cast<BranchInst>(Br)->getSuccessor(0);

  // Splice the code entry block into calling block, right before the
  // unconditional branch.
  CalleeEntry->replaceAllUsesWith(OrigBB);  // Update PHI nodes
  OrigBB->getInstList().splice(Br, CalleeEntry->getInstList());

  // Remove the unconditional branch.
  OrigBB->getInstList().erase(Br);

  // Now we can remove the CalleeEntry block, which is now empty.
  Caller->getBasicBlockList().erase(CalleeEntry);

  // If we inserted a phi node, check to see if it has a single value (e.g. all
  // the entries are the same or undef).  If so, remove the PHI so it doesn't
  // block other optimizations.
  if (PHI)
    if (Value *V = SimplifyInstruction(PHI, IFI.TD)) {
      PHI->replaceAllUsesWith(V);
      PHI->eraseFromParent();
    }

  return true;
}
