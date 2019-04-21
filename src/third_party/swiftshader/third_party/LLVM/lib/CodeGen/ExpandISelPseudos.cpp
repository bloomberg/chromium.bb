//===-- llvm/CodeGen/ExpandISelPseudos.cpp ----------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Expand Pseudo-instructions produced by ISel. These are usually to allow
// the expansion to contain control flow, such as a conditional move
// implemented with a conditional branch and a phi, or an atomic operation
// implemented with a loop.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "expand-isel-pseudos"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Target/TargetLowering.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/Debug.h"
using namespace llvm;

namespace {
  class ExpandISelPseudos : public MachineFunctionPass {
  public:
    static char ID; // Pass identification, replacement for typeid
    ExpandISelPseudos() : MachineFunctionPass(ID) {}

  private:
    virtual bool runOnMachineFunction(MachineFunction &MF);

    const char *getPassName() const {
      return "Expand ISel Pseudo-instructions";
    }

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      MachineFunctionPass::getAnalysisUsage(AU);
    }
  };
} // end anonymous namespace

char ExpandISelPseudos::ID = 0;
INITIALIZE_PASS(ExpandISelPseudos, "expand-isel-pseudos",
                "Expand CodeGen Pseudo-instructions", false, false)

FunctionPass *llvm::createExpandISelPseudosPass() {
  return new ExpandISelPseudos();
}

bool ExpandISelPseudos::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;
  const TargetLowering *TLI = MF.getTarget().getTargetLowering();

  // Iterate through each instruction in the function, looking for pseudos.
  for (MachineFunction::iterator I = MF.begin(), E = MF.end(); I != E; ++I) {
    MachineBasicBlock *MBB = I;
    for (MachineBasicBlock::iterator MBBI = MBB->begin(), MBBE = MBB->end();
         MBBI != MBBE; ) {
      MachineInstr *MI = MBBI++;

      // If MI is a pseudo, expand it.
      const MCInstrDesc &MCID = MI->getDesc();
      if (MCID.usesCustomInsertionHook()) {
        Changed = true;
        MachineBasicBlock *NewMBB =
          TLI->EmitInstrWithCustomInserter(MI, MBB);
        // The expansion may involve new basic blocks.
        if (NewMBB != MBB) {
          MBB = NewMBB;
          I = NewMBB;
          MBBI = NewMBB->begin();
          MBBE = NewMBB->end();
        }
      }
    }
  }

  return Changed;
}
