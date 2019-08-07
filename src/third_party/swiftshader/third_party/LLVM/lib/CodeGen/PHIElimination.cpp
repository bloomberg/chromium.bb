//===-- PhiElimination.cpp - Eliminate PHI nodes by inserting copies ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass eliminates machine instruction PHI nodes by inserting copy
// instructions.  This destroys SSA information, but is the desired input for
// some register allocators.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "phielim"
#include "PHIEliminationUtils.h"
#include "llvm/CodeGen/LiveVariables.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Function.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include <algorithm>
using namespace llvm;

static cl::opt<bool>
DisableEdgeSplitting("disable-phi-elim-edge-splitting", cl::init(false),
                     cl::Hidden, cl::desc("Disable critical edge splitting "
                                          "during PHI elimination"));

namespace {
  class PHIElimination : public MachineFunctionPass {
    MachineRegisterInfo *MRI; // Machine register information

  public:
    static char ID; // Pass identification, replacement for typeid
    PHIElimination() : MachineFunctionPass(ID) {
      initializePHIEliminationPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnMachineFunction(MachineFunction &Fn);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const;

  private:
    /// EliminatePHINodes - Eliminate phi nodes by inserting copy instructions
    /// in predecessor basic blocks.
    ///
    bool EliminatePHINodes(MachineFunction &MF, MachineBasicBlock &MBB);
    void LowerAtomicPHINode(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator AfterPHIsIt);

    /// analyzePHINodes - Gather information about the PHI nodes in
    /// here. In particular, we want to map the number of uses of a virtual
    /// register which is used in a PHI node. We map that to the BB the
    /// vreg is coming from. This is used later to determine when the vreg
    /// is killed in the BB.
    ///
    void analyzePHINodes(const MachineFunction& Fn);

    /// Split critical edges where necessary for good coalescer performance.
    bool SplitPHIEdges(MachineFunction &MF, MachineBasicBlock &MBB,
                       LiveVariables &LV, MachineLoopInfo *MLI);

    typedef std::pair<unsigned, unsigned> BBVRegPair;
    typedef DenseMap<BBVRegPair, unsigned> VRegPHIUse;

    VRegPHIUse VRegPHIUseCount;

    // Defs of PHI sources which are implicit_def.
    SmallPtrSet<MachineInstr*, 4> ImpDefs;

    // Map reusable lowered PHI node -> incoming join register.
    typedef DenseMap<MachineInstr*, unsigned,
                     MachineInstrExpressionTrait> LoweredPHIMap;
    LoweredPHIMap LoweredPHIs;
  };
}

STATISTIC(NumAtomic, "Number of atomic phis lowered");
STATISTIC(NumCriticalEdgesSplit, "Number of critical edges split");
STATISTIC(NumReused, "Number of reused lowered phis");

char PHIElimination::ID = 0;
INITIALIZE_PASS(PHIElimination, "phi-node-elimination",
                "Eliminate PHI nodes for register allocation", false, false)

char& llvm::PHIEliminationID = PHIElimination::ID;

void PHIElimination::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addPreserved<LiveVariables>();
  AU.addPreserved<MachineDominatorTree>();
  AU.addPreserved<MachineLoopInfo>();
  MachineFunctionPass::getAnalysisUsage(AU);
}

bool PHIElimination::runOnMachineFunction(MachineFunction &MF) {
  MRI = &MF.getRegInfo();

  bool Changed = false;

  // This pass takes the function out of SSA form.
  MRI->leaveSSA();

  // Split critical edges to help the coalescer
  if (!DisableEdgeSplitting) {
    if (LiveVariables *LV = getAnalysisIfAvailable<LiveVariables>()) {
      MachineLoopInfo *MLI = getAnalysisIfAvailable<MachineLoopInfo>();
      for (MachineFunction::iterator I = MF.begin(), E = MF.end(); I != E; ++I)
        Changed |= SplitPHIEdges(MF, *I, *LV, MLI);
    }
  }

  // Populate VRegPHIUseCount
  analyzePHINodes(MF);

  // Eliminate PHI instructions by inserting copies into predecessor blocks.
  for (MachineFunction::iterator I = MF.begin(), E = MF.end(); I != E; ++I)
    Changed |= EliminatePHINodes(MF, *I);

  // Remove dead IMPLICIT_DEF instructions.
  for (SmallPtrSet<MachineInstr*, 4>::iterator I = ImpDefs.begin(),
         E = ImpDefs.end(); I != E; ++I) {
    MachineInstr *DefMI = *I;
    unsigned DefReg = DefMI->getOperand(0).getReg();
    if (MRI->use_nodbg_empty(DefReg))
      DefMI->eraseFromParent();
  }

  // Clean up the lowered PHI instructions.
  for (LoweredPHIMap::iterator I = LoweredPHIs.begin(), E = LoweredPHIs.end();
       I != E; ++I)
    MF.DeleteMachineInstr(I->first);

  LoweredPHIs.clear();
  ImpDefs.clear();
  VRegPHIUseCount.clear();

  return Changed;
}

/// EliminatePHINodes - Eliminate phi nodes by inserting copy instructions in
/// predecessor basic blocks.
///
bool PHIElimination::EliminatePHINodes(MachineFunction &MF,
                                             MachineBasicBlock &MBB) {
  if (MBB.empty() || !MBB.front().isPHI())
    return false;   // Quick exit for basic blocks without PHIs.

  // Get an iterator to the first instruction after the last PHI node (this may
  // also be the end of the basic block).
  MachineBasicBlock::iterator AfterPHIsIt = MBB.SkipPHIsAndLabels(MBB.begin());

  while (MBB.front().isPHI())
    LowerAtomicPHINode(MBB, AfterPHIsIt);

  return true;
}

/// isSourceDefinedByImplicitDef - Return true if all sources of the phi node
/// are implicit_def's.
static bool isSourceDefinedByImplicitDef(const MachineInstr *MPhi,
                                         const MachineRegisterInfo *MRI) {
  for (unsigned i = 1; i != MPhi->getNumOperands(); i += 2) {
    unsigned SrcReg = MPhi->getOperand(i).getReg();
    const MachineInstr *DefMI = MRI->getVRegDef(SrcReg);
    if (!DefMI || !DefMI->isImplicitDef())
      return false;
  }
  return true;
}



/// LowerAtomicPHINode - Lower the PHI node at the top of the specified block,
/// under the assuption that it needs to be lowered in a way that supports
/// atomic execution of PHIs.  This lowering method is always correct all of the
/// time.
///
void PHIElimination::LowerAtomicPHINode(
                                      MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator AfterPHIsIt) {
  ++NumAtomic;
  // Unlink the PHI node from the basic block, but don't delete the PHI yet.
  MachineInstr *MPhi = MBB.remove(MBB.begin());

  unsigned NumSrcs = (MPhi->getNumOperands() - 1) / 2;
  unsigned DestReg = MPhi->getOperand(0).getReg();
  assert(MPhi->getOperand(0).getSubReg() == 0 && "Can't handle sub-reg PHIs");
  bool isDead = MPhi->getOperand(0).isDead();

  // Create a new register for the incoming PHI arguments.
  MachineFunction &MF = *MBB.getParent();
  unsigned IncomingReg = 0;
  bool reusedIncoming = false;  // Is IncomingReg reused from an earlier PHI?

  // Insert a register to register copy at the top of the current block (but
  // after any remaining phi nodes) which copies the new incoming register
  // into the phi node destination.
  const TargetInstrInfo *TII = MF.getTarget().getInstrInfo();
  if (isSourceDefinedByImplicitDef(MPhi, MRI))
    // If all sources of a PHI node are implicit_def, just emit an
    // implicit_def instead of a copy.
    BuildMI(MBB, AfterPHIsIt, MPhi->getDebugLoc(),
            TII->get(TargetOpcode::IMPLICIT_DEF), DestReg);
  else {
    // Can we reuse an earlier PHI node? This only happens for critical edges,
    // typically those created by tail duplication.
    unsigned &entry = LoweredPHIs[MPhi];
    if (entry) {
      // An identical PHI node was already lowered. Reuse the incoming register.
      IncomingReg = entry;
      reusedIncoming = true;
      ++NumReused;
      DEBUG(dbgs() << "Reusing " << PrintReg(IncomingReg) << " for " << *MPhi);
    } else {
      const TargetRegisterClass *RC = MF.getRegInfo().getRegClass(DestReg);
      entry = IncomingReg = MF.getRegInfo().createVirtualRegister(RC);
    }
    BuildMI(MBB, AfterPHIsIt, MPhi->getDebugLoc(),
            TII->get(TargetOpcode::COPY), DestReg)
      .addReg(IncomingReg);
  }

  // Update live variable information if there is any.
  LiveVariables *LV = getAnalysisIfAvailable<LiveVariables>();
  if (LV) {
    MachineInstr *PHICopy = prior(AfterPHIsIt);

    if (IncomingReg) {
      LiveVariables::VarInfo &VI = LV->getVarInfo(IncomingReg);

      // Increment use count of the newly created virtual register.
      VI.NumUses++;
      LV->setPHIJoin(IncomingReg);

      // When we are reusing the incoming register, it may already have been
      // killed in this block. The old kill will also have been inserted at
      // AfterPHIsIt, so it appears before the current PHICopy.
      if (reusedIncoming)
        if (MachineInstr *OldKill = VI.findKill(&MBB)) {
          DEBUG(dbgs() << "Remove old kill from " << *OldKill);
          LV->removeVirtualRegisterKilled(IncomingReg, OldKill);
          DEBUG(MBB.dump());
        }

      // Add information to LiveVariables to know that the incoming value is
      // killed.  Note that because the value is defined in several places (once
      // each for each incoming block), the "def" block and instruction fields
      // for the VarInfo is not filled in.
      LV->addVirtualRegisterKilled(IncomingReg, PHICopy);
    }

    // Since we are going to be deleting the PHI node, if it is the last use of
    // any registers, or if the value itself is dead, we need to move this
    // information over to the new copy we just inserted.
    LV->removeVirtualRegistersKilled(MPhi);

    // If the result is dead, update LV.
    if (isDead) {
      LV->addVirtualRegisterDead(DestReg, PHICopy);
      LV->removeVirtualRegisterDead(DestReg, MPhi);
    }
  }

  // Adjust the VRegPHIUseCount map to account for the removal of this PHI node.
  for (unsigned i = 1; i != MPhi->getNumOperands(); i += 2)
    --VRegPHIUseCount[BBVRegPair(MPhi->getOperand(i+1).getMBB()->getNumber(),
                                 MPhi->getOperand(i).getReg())];

  // Now loop over all of the incoming arguments, changing them to copy into the
  // IncomingReg register in the corresponding predecessor basic block.
  SmallPtrSet<MachineBasicBlock*, 8> MBBsInsertedInto;
  for (int i = NumSrcs - 1; i >= 0; --i) {
    unsigned SrcReg = MPhi->getOperand(i*2+1).getReg();
    unsigned SrcSubReg = MPhi->getOperand(i*2+1).getSubReg();

    assert(TargetRegisterInfo::isVirtualRegister(SrcReg) &&
           "Machine PHI Operands must all be virtual registers!");

    // Get the MachineBasicBlock equivalent of the BasicBlock that is the source
    // path the PHI.
    MachineBasicBlock &opBlock = *MPhi->getOperand(i*2+2).getMBB();

    // If source is defined by an implicit def, there is no need to insert a
    // copy.
    MachineInstr *DefMI = MRI->getVRegDef(SrcReg);
    if (DefMI->isImplicitDef()) {
      ImpDefs.insert(DefMI);
      continue;
    }

    // Check to make sure we haven't already emitted the copy for this block.
    // This can happen because PHI nodes may have multiple entries for the same
    // basic block.
    if (!MBBsInsertedInto.insert(&opBlock))
      continue;  // If the copy has already been emitted, we're done.

    // Find a safe location to insert the copy, this may be the first terminator
    // in the block (or end()).
    MachineBasicBlock::iterator InsertPos =
      findPHICopyInsertPoint(&opBlock, &MBB, SrcReg);

    // Insert the copy.
    if (!reusedIncoming && IncomingReg)
      BuildMI(opBlock, InsertPos, MPhi->getDebugLoc(),
              TII->get(TargetOpcode::COPY), IncomingReg).addReg(SrcReg, 0, SrcSubReg);

    // Now update live variable information if we have it.  Otherwise we're done
    if (!LV) continue;

    // We want to be able to insert a kill of the register if this PHI (aka, the
    // copy we just inserted) is the last use of the source value.  Live
    // variable analysis conservatively handles this by saying that the value is
    // live until the end of the block the PHI entry lives in.  If the value
    // really is dead at the PHI copy, there will be no successor blocks which
    // have the value live-in.

    // Also check to see if this register is in use by another PHI node which
    // has not yet been eliminated.  If so, it will be killed at an appropriate
    // point later.

    // Is it used by any PHI instructions in this block?
    bool ValueIsUsed = VRegPHIUseCount[BBVRegPair(opBlock.getNumber(), SrcReg)];

    // Okay, if we now know that the value is not live out of the block, we can
    // add a kill marker in this block saying that it kills the incoming value!
    if (!ValueIsUsed && !LV->isLiveOut(SrcReg, opBlock)) {
      // In our final twist, we have to decide which instruction kills the
      // register.  In most cases this is the copy, however, the first
      // terminator instruction at the end of the block may also use the value.
      // In this case, we should mark *it* as being the killing block, not the
      // copy.
      MachineBasicBlock::iterator KillInst;
      MachineBasicBlock::iterator Term = opBlock.getFirstTerminator();
      if (Term != opBlock.end() && Term->readsRegister(SrcReg)) {
        KillInst = Term;

        // Check that no other terminators use values.
#ifndef NDEBUG
        for (MachineBasicBlock::iterator TI = llvm::next(Term);
             TI != opBlock.end(); ++TI) {
          if (TI->isDebugValue())
            continue;
          assert(!TI->readsRegister(SrcReg) &&
                 "Terminator instructions cannot use virtual registers unless"
                 "they are the first terminator in a block!");
        }
#endif
      } else if (reusedIncoming || !IncomingReg) {
        // We may have to rewind a bit if we didn't insert a copy this time.
        KillInst = Term;
        while (KillInst != opBlock.begin()) {
          --KillInst;
          if (KillInst->isDebugValue())
            continue;
          if (KillInst->readsRegister(SrcReg))
            break;
        }
      } else {
        // We just inserted this copy.
        KillInst = prior(InsertPos);
      }
      assert(KillInst->readsRegister(SrcReg) && "Cannot find kill instruction");

      // Finally, mark it killed.
      LV->addVirtualRegisterKilled(SrcReg, KillInst);

      // This vreg no longer lives all of the way through opBlock.
      unsigned opBlockNum = opBlock.getNumber();
      LV->getVarInfo(SrcReg).AliveBlocks.reset(opBlockNum);
    }
  }

  // Really delete the PHI instruction now, if it is not in the LoweredPHIs map.
  if (reusedIncoming || !IncomingReg)
    MF.DeleteMachineInstr(MPhi);
}

/// analyzePHINodes - Gather information about the PHI nodes in here. In
/// particular, we want to map the number of uses of a virtual register which is
/// used in a PHI node. We map that to the BB the vreg is coming from. This is
/// used later to determine when the vreg is killed in the BB.
///
void PHIElimination::analyzePHINodes(const MachineFunction& MF) {
  for (MachineFunction::const_iterator I = MF.begin(), E = MF.end();
       I != E; ++I)
    for (MachineBasicBlock::const_iterator BBI = I->begin(), BBE = I->end();
         BBI != BBE && BBI->isPHI(); ++BBI)
      for (unsigned i = 1, e = BBI->getNumOperands(); i != e; i += 2)
        ++VRegPHIUseCount[BBVRegPair(BBI->getOperand(i+1).getMBB()->getNumber(),
                                     BBI->getOperand(i).getReg())];
}

bool PHIElimination::SplitPHIEdges(MachineFunction &MF,
                                   MachineBasicBlock &MBB,
                                   LiveVariables &LV,
                                   MachineLoopInfo *MLI) {
  if (MBB.empty() || !MBB.front().isPHI() || MBB.isLandingPad())
    return false;   // Quick exit for basic blocks without PHIs.

  bool Changed = false;
  for (MachineBasicBlock::const_iterator BBI = MBB.begin(), BBE = MBB.end();
       BBI != BBE && BBI->isPHI(); ++BBI) {
    for (unsigned i = 1, e = BBI->getNumOperands(); i != e; i += 2) {
      unsigned Reg = BBI->getOperand(i).getReg();
      MachineBasicBlock *PreMBB = BBI->getOperand(i+1).getMBB();
      // We break edges when registers are live out from the predecessor block
      // (not considering PHI nodes). If the register is live in to this block
      // anyway, we would gain nothing from splitting.
      // Avoid splitting backedges of loops. It would introduce small
      // out-of-line blocks into the loop which is very bad for code placement.
      if (PreMBB != &MBB &&
          !LV.isLiveIn(Reg, MBB) && LV.isLiveOut(Reg, *PreMBB)) {
        if (!MLI ||
            !(MLI->getLoopFor(PreMBB) == MLI->getLoopFor(&MBB) &&
              MLI->isLoopHeader(&MBB))) {
          if (PreMBB->SplitCriticalEdge(&MBB, this)) {
            Changed = true;
            ++NumCriticalEdgesSplit;
          }
        }
      }
    }
  }
  return Changed;
}
