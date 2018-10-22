//===- InlineSimple.cpp - Code to perform simple function inlining --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements bottom-up inlining of functions into callees.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "inline"
#include "llvm/CallingConv.h"
#include "llvm/Instructions.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/Module.h"
#include "llvm/Type.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/InlineCost.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/IPO/InlinerPass.h"
#include "llvm/Target/TargetData.h"
#include "llvm/ADT/SmallPtrSet.h"

using namespace llvm;

namespace {

  class SimpleInliner : public Inliner {
    // Functions that are never inlined
    SmallPtrSet<const Function*, 16> NeverInline;
    InlineCostAnalyzer CA;
  public:
    SimpleInliner() : Inliner(ID) {
      initializeSimpleInlinerPass(*PassRegistry::getPassRegistry());
    }
    SimpleInliner(int Threshold) : Inliner(ID, Threshold) {
      initializeSimpleInlinerPass(*PassRegistry::getPassRegistry());
    }
    static char ID; // Pass identification, replacement for typeid
    InlineCost getInlineCost(CallSite CS) {
      return CA.getInlineCost(CS, NeverInline);
    }
    float getInlineFudgeFactor(CallSite CS) {
      return CA.getInlineFudgeFactor(CS);
    }
    void resetCachedCostInfo(Function *Caller) {
      CA.resetCachedCostInfo(Caller);
    }
    void growCachedCostInfo(Function* Caller, Function* Callee) {
      CA.growCachedCostInfo(Caller, Callee);
    }
    virtual bool doInitialization(CallGraph &CG);
    void releaseMemory() {
      CA.clear();
    }
  };
}

char SimpleInliner::ID = 0;
INITIALIZE_PASS_BEGIN(SimpleInliner, "inline",
                "Function Integration/Inlining", false, false)
INITIALIZE_AG_DEPENDENCY(CallGraph)
INITIALIZE_PASS_END(SimpleInliner, "inline",
                "Function Integration/Inlining", false, false)

Pass *llvm::createFunctionInliningPass() { return new SimpleInliner(); }

Pass *llvm::createFunctionInliningPass(int Threshold) {
  return new SimpleInliner(Threshold);
}

// doInitialization - Initializes the vector of functions that have been
// annotated with the noinline attribute.
bool SimpleInliner::doInitialization(CallGraph &CG) {
  CA.setTargetData(getAnalysisIfAvailable<TargetData>());

  Module &M = CG.getModule();

  for (Module::iterator I = M.begin(), E = M.end();
       I != E; ++I)
    if (!I->isDeclaration() && I->hasFnAttr(Attribute::NoInline))
      NeverInline.insert(I);

  // Get llvm.noinline
  GlobalVariable *GV = M.getNamedGlobal("llvm.noinline");

  if (GV == 0)
    return false;

  // Don't crash on invalid code
  if (!GV->hasDefinitiveInitializer())
    return false;

  const ConstantArray *InitList = dyn_cast<ConstantArray>(GV->getInitializer());

  if (InitList == 0)
    return false;

  // Iterate over each element and add to the NeverInline set
  for (unsigned i = 0, e = InitList->getNumOperands(); i != e; ++i) {

    // Get Source
    const Constant *Elt = InitList->getOperand(i);

    if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(Elt))
      if (CE->getOpcode() == Instruction::BitCast)
        Elt = CE->getOperand(0);

    // Insert into set of functions to never inline
    if (const Function *F = dyn_cast<Function>(Elt))
      NeverInline.insert(F);
  }

  return false;
}

