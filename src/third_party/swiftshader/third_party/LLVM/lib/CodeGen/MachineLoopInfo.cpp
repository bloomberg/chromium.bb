//===- MachineLoopInfo.cpp - Natural Loop Calculator ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the MachineLoopInfo class that is used to identify natural
// loops and determine the loop depth of various nodes of the CFG.  Note that
// the loops identified may actually be several natural loops that share the 
// same header node... not just a single natural loop.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Support/Debug.h"
using namespace llvm;

namespace llvm {
#define MLB class LoopBase<MachineBasicBlock, MachineLoop>
TEMPLATE_INSTANTIATION(MLB);
#undef MLB
#define MLIB class LoopInfoBase<MachineBasicBlock, MachineLoop>
TEMPLATE_INSTANTIATION(MLIB);
#undef MLIB
}

char MachineLoopInfo::ID = 0;
INITIALIZE_PASS_BEGIN(MachineLoopInfo, "machine-loops",
                "Machine Natural Loop Construction", true, true)
INITIALIZE_PASS_DEPENDENCY(MachineDominatorTree)
INITIALIZE_PASS_END(MachineLoopInfo, "machine-loops",
                "Machine Natural Loop Construction", true, true)

char &llvm::MachineLoopInfoID = MachineLoopInfo::ID;

bool MachineLoopInfo::runOnMachineFunction(MachineFunction &) {
  releaseMemory();
  LI.Calculate(getAnalysis<MachineDominatorTree>().getBase());    // Update
  return false;
}

void MachineLoopInfo::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<MachineDominatorTree>();
  MachineFunctionPass::getAnalysisUsage(AU);
}

MachineBasicBlock *MachineLoop::getTopBlock() {
  MachineBasicBlock *TopMBB = getHeader();
  MachineFunction::iterator Begin = TopMBB->getParent()->begin();
  if (TopMBB != Begin) {
    MachineBasicBlock *PriorMBB = prior(MachineFunction::iterator(TopMBB));
    while (contains(PriorMBB)) {
      TopMBB = PriorMBB;
      if (TopMBB == Begin) break;
      PriorMBB = prior(MachineFunction::iterator(TopMBB));
    }
  }
  return TopMBB;
}

MachineBasicBlock *MachineLoop::getBottomBlock() {
  MachineBasicBlock *BotMBB = getHeader();
  MachineFunction::iterator End = BotMBB->getParent()->end();
  if (BotMBB != prior(End)) {
    MachineBasicBlock *NextMBB = llvm::next(MachineFunction::iterator(BotMBB));
    while (contains(NextMBB)) {
      BotMBB = NextMBB;
      if (BotMBB == llvm::next(MachineFunction::iterator(BotMBB))) break;
      NextMBB = llvm::next(MachineFunction::iterator(BotMBB));
    }
  }
  return BotMBB;
}

void MachineLoop::dump() const {
  print(dbgs());
}
