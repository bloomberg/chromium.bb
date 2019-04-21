//===- DeadMachineInstructionElim.cpp - Remove dead machine instructions --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is an extremely simple MachineInstr-level dead-code-elimination pass.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "codegen-dce"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Pass.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/ADT/Statistic.h"
using namespace llvm;

STATISTIC(NumDeletes,          "Number of dead instructions deleted");

namespace {
  class DeadMachineInstructionElim : public MachineFunctionPass {
    virtual bool runOnMachineFunction(MachineFunction &MF);
    
    const TargetRegisterInfo *TRI;
    const MachineRegisterInfo *MRI;
    const TargetInstrInfo *TII;
    BitVector LivePhysRegs;

  public:
    static char ID; // Pass identification, replacement for typeid
    DeadMachineInstructionElim() : MachineFunctionPass(ID) {
     initializeDeadMachineInstructionElimPass(*PassRegistry::getPassRegistry());
    }

  private:
    bool isDead(const MachineInstr *MI) const;
  };
}
char DeadMachineInstructionElim::ID = 0;

INITIALIZE_PASS(DeadMachineInstructionElim, "dead-mi-elimination",
                "Remove dead machine instructions", false, false)

FunctionPass *llvm::createDeadMachineInstructionElimPass() {
  return new DeadMachineInstructionElim();
}

bool DeadMachineInstructionElim::isDead(const MachineInstr *MI) const {
  // Technically speaking inline asm without side effects and no defs can still
  // be deleted. But there is so much bad inline asm code out there, we should
  // let them be.
  if (MI->isInlineAsm())
    return false;

  // Don't delete instructions with side effects.
  bool SawStore = false;
  if (!MI->isSafeToMove(TII, 0, SawStore) && !MI->isPHI())
    return false;

  // Examine each operand.
  for (unsigned i = 0, e = MI->getNumOperands(); i != e; ++i) {
    const MachineOperand &MO = MI->getOperand(i);
    if (MO.isReg() && MO.isDef()) {
      unsigned Reg = MO.getReg();
      if (TargetRegisterInfo::isPhysicalRegister(Reg) ?
          LivePhysRegs[Reg] : !MRI->use_nodbg_empty(Reg)) {
        // This def has a non-debug use. Don't delete the instruction!
        return false;
      }
    }
  }

  // If there are no defs with uses, the instruction is dead.
  return true;
}

bool DeadMachineInstructionElim::runOnMachineFunction(MachineFunction &MF) {
  bool AnyChanges = false;
  MRI = &MF.getRegInfo();
  TRI = MF.getTarget().getRegisterInfo();
  TII = MF.getTarget().getInstrInfo();

  // Treat reserved registers as always live.
  BitVector ReservedRegs = TRI->getReservedRegs(MF);

  // Loop over all instructions in all blocks, from bottom to top, so that it's
  // more likely that chains of dependent but ultimately dead instructions will
  // be cleaned up.
  for (MachineFunction::reverse_iterator I = MF.rbegin(), E = MF.rend();
       I != E; ++I) {
    MachineBasicBlock *MBB = &*I;

    // Start out assuming that reserved registers are live out of this block.
    LivePhysRegs = ReservedRegs;

    // Also add any explicit live-out physregs for this block.
    if (!MBB->empty() && MBB->back().getDesc().isReturn())
      for (MachineRegisterInfo::liveout_iterator LOI = MRI->liveout_begin(),
           LOE = MRI->liveout_end(); LOI != LOE; ++LOI) {
        unsigned Reg = *LOI;
        if (TargetRegisterInfo::isPhysicalRegister(Reg))
          LivePhysRegs.set(Reg);
      }

    // Add live-ins from sucessors to LivePhysRegs. Normally, physregs are not
    // live across blocks, but some targets (x86) can have flags live out of a
    // block.
    for (MachineBasicBlock::succ_iterator S = MBB->succ_begin(),
           E = MBB->succ_end(); S != E; S++)
      for (MachineBasicBlock::livein_iterator LI = (*S)->livein_begin();
           LI != (*S)->livein_end(); LI++)
        LivePhysRegs.set(*LI);

    // Now scan the instructions and delete dead ones, tracking physreg
    // liveness as we go.
    for (MachineBasicBlock::reverse_iterator MII = MBB->rbegin(),
         MIE = MBB->rend(); MII != MIE; ) {
      MachineInstr *MI = &*MII;

      // If the instruction is dead, delete it!
      if (isDead(MI)) {
        DEBUG(dbgs() << "DeadMachineInstructionElim: DELETING: " << *MI);
        // It is possible that some DBG_VALUE instructions refer to this
        // instruction.  Examine each def operand for such references;
        // if found, mark the DBG_VALUE as undef (but don't delete it).
        for (unsigned i = 0, e = MI->getNumOperands(); i != e; ++i) {
          const MachineOperand &MO = MI->getOperand(i);
          if (!MO.isReg() || !MO.isDef())
            continue;
          unsigned Reg = MO.getReg();
          if (!TargetRegisterInfo::isVirtualRegister(Reg))
            continue;
          MachineRegisterInfo::use_iterator nextI;
          for (MachineRegisterInfo::use_iterator I = MRI->use_begin(Reg),
               E = MRI->use_end(); I!=E; I=nextI) {
            nextI = llvm::next(I);  // I is invalidated by the setReg
            MachineOperand& Use = I.getOperand();
            MachineInstr *UseMI = Use.getParent();
            if (UseMI==MI)
              continue;
            assert(Use.isDebug());
            UseMI->getOperand(0).setReg(0U);
          }
        }
        AnyChanges = true;
        MI->eraseFromParent();
        ++NumDeletes;
        MIE = MBB->rend();
        // MII is now pointing to the next instruction to process,
        // so don't increment it.
        continue;
      }

      // Record the physreg defs.
      for (unsigned i = 0, e = MI->getNumOperands(); i != e; ++i) {
        const MachineOperand &MO = MI->getOperand(i);
        if (MO.isReg() && MO.isDef()) {
          unsigned Reg = MO.getReg();
          if (TargetRegisterInfo::isPhysicalRegister(Reg)) {
            LivePhysRegs.reset(Reg);
            // Check the subreg set, not the alias set, because a def
            // of a super-register may still be partially live after
            // this def.
            for (const unsigned *SubRegs = TRI->getSubRegisters(Reg);
                 *SubRegs; ++SubRegs)
              LivePhysRegs.reset(*SubRegs);
          }
        }
      }
      // Record the physreg uses, after the defs, in case a physreg is
      // both defined and used in the same instruction.
      for (unsigned i = 0, e = MI->getNumOperands(); i != e; ++i) {
        const MachineOperand &MO = MI->getOperand(i);
        if (MO.isReg() && MO.isUse()) {
          unsigned Reg = MO.getReg();
          if (TargetRegisterInfo::isPhysicalRegister(Reg)) {
            LivePhysRegs.set(Reg);
            for (const unsigned *AliasSet = TRI->getAliasSet(Reg);
                 *AliasSet; ++AliasSet)
              LivePhysRegs.set(*AliasSet);
          }
        }
      }

      // We didn't delete the current instruction, so increment MII to
      // the next one.
      ++MII;
    }
  }

  LivePhysRegs.clear();
  return AnyChanges;
}
