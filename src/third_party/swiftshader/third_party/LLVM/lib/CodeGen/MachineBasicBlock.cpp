//===-- llvm/CodeGen/MachineBasicBlock.cpp ----------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Collect the sequence of machine instructions for a basic block.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/BasicBlock.h"
#include "llvm/CodeGen/LiveVariables.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/SlotIndexes.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/Target/TargetRegisterInfo.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Assembly/Writer.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/LeakDetector.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
using namespace llvm;

MachineBasicBlock::MachineBasicBlock(MachineFunction &mf, const BasicBlock *bb)
  : BB(bb), Number(-1), xParent(&mf), Alignment(0), IsLandingPad(false),
    AddressTaken(false) {
  Insts.Parent = this;
}

MachineBasicBlock::~MachineBasicBlock() {
  LeakDetector::removeGarbageObject(this);
}

/// getSymbol - Return the MCSymbol for this basic block.
///
MCSymbol *MachineBasicBlock::getSymbol() const {
  const MachineFunction *MF = getParent();
  MCContext &Ctx = MF->getContext();
  const char *Prefix = Ctx.getAsmInfo().getPrivateGlobalPrefix();
  return Ctx.GetOrCreateSymbol(Twine(Prefix) + "BB" +
                               Twine(MF->getFunctionNumber()) + "_" +
                               Twine(getNumber()));
}


raw_ostream &llvm::operator<<(raw_ostream &OS, const MachineBasicBlock &MBB) {
  MBB.print(OS);
  return OS;
}

/// addNodeToList (MBB) - When an MBB is added to an MF, we need to update the
/// parent pointer of the MBB, the MBB numbering, and any instructions in the
/// MBB to be on the right operand list for registers.
///
/// MBBs start out as #-1. When a MBB is added to a MachineFunction, it
/// gets the next available unique MBB number. If it is removed from a
/// MachineFunction, it goes back to being #-1.
void ilist_traits<MachineBasicBlock>::addNodeToList(MachineBasicBlock *N) {
  MachineFunction &MF = *N->getParent();
  N->Number = MF.addToMBBNumbering(N);

  // Make sure the instructions have their operands in the reginfo lists.
  MachineRegisterInfo &RegInfo = MF.getRegInfo();
  for (MachineBasicBlock::iterator I = N->begin(), E = N->end(); I != E; ++I)
    I->AddRegOperandsToUseLists(RegInfo);

  LeakDetector::removeGarbageObject(N);
}

void ilist_traits<MachineBasicBlock>::removeNodeFromList(MachineBasicBlock *N) {
  N->getParent()->removeFromMBBNumbering(N->Number);
  N->Number = -1;
  LeakDetector::addGarbageObject(N);
}


/// addNodeToList (MI) - When we add an instruction to a basic block
/// list, we update its parent pointer and add its operands from reg use/def
/// lists if appropriate.
void ilist_traits<MachineInstr>::addNodeToList(MachineInstr *N) {
  assert(N->getParent() == 0 && "machine instruction already in a basic block");
  N->setParent(Parent);

  // Add the instruction's register operands to their corresponding
  // use/def lists.
  MachineFunction *MF = Parent->getParent();
  N->AddRegOperandsToUseLists(MF->getRegInfo());

  LeakDetector::removeGarbageObject(N);
}

/// removeNodeFromList (MI) - When we remove an instruction from a basic block
/// list, we update its parent pointer and remove its operands from reg use/def
/// lists if appropriate.
void ilist_traits<MachineInstr>::removeNodeFromList(MachineInstr *N) {
  assert(N->getParent() != 0 && "machine instruction not in a basic block");

  // Remove from the use/def lists.
  N->RemoveRegOperandsFromUseLists();

  N->setParent(0);

  LeakDetector::addGarbageObject(N);
}

/// transferNodesFromList (MI) - When moving a range of instructions from one
/// MBB list to another, we need to update the parent pointers and the use/def
/// lists.
void ilist_traits<MachineInstr>::
transferNodesFromList(ilist_traits<MachineInstr> &fromList,
                      MachineBasicBlock::iterator first,
                      MachineBasicBlock::iterator last) {
  assert(Parent->getParent() == fromList.Parent->getParent() &&
        "MachineInstr parent mismatch!");

  // Splice within the same MBB -> no change.
  if (Parent == fromList.Parent) return;

  // If splicing between two blocks within the same function, just update the
  // parent pointers.
  for (; first != last; ++first)
    first->setParent(Parent);
}

void ilist_traits<MachineInstr>::deleteNode(MachineInstr* MI) {
  assert(!MI->getParent() && "MI is still in a block!");
  Parent->getParent()->DeleteMachineInstr(MI);
}

MachineBasicBlock::iterator MachineBasicBlock::getFirstNonPHI() {
  iterator I = begin();
  while (I != end() && I->isPHI())
    ++I;
  return I;
}

MachineBasicBlock::iterator
MachineBasicBlock::SkipPHIsAndLabels(MachineBasicBlock::iterator I) {
  while (I != end() && (I->isPHI() || I->isLabel() || I->isDebugValue()))
    ++I;
  return I;
}

MachineBasicBlock::iterator MachineBasicBlock::getFirstTerminator() {
  iterator I = end();
  while (I != begin() && ((--I)->getDesc().isTerminator() || I->isDebugValue()))
    ; /*noop */
  while (I != end() && !I->getDesc().isTerminator())
    ++I;
  return I;
}

MachineBasicBlock::iterator MachineBasicBlock::getLastNonDebugInstr() {
  iterator B = begin(), I = end();
  while (I != B) {
    --I;
    if (I->isDebugValue())
      continue;
    return I;
  }
  // The block is all debug values.
  return end();
}

const MachineBasicBlock *MachineBasicBlock::getLandingPadSuccessor() const {
  // A block with a landing pad successor only has one other successor.
  if (succ_size() > 2)
    return 0;
  for (const_succ_iterator I = succ_begin(), E = succ_end(); I != E; ++I)
    if ((*I)->isLandingPad())
      return *I;
  return 0;
}

void MachineBasicBlock::dump() const {
  print(dbgs());
}

StringRef MachineBasicBlock::getName() const {
  if (const BasicBlock *LBB = getBasicBlock())
    return LBB->getName();
  else
    return "(null)";
}

void MachineBasicBlock::print(raw_ostream &OS, SlotIndexes *Indexes) const {
  const MachineFunction *MF = getParent();
  if (!MF) {
    OS << "Can't print out MachineBasicBlock because parent MachineFunction"
       << " is null\n";
    return;
  }

  if (Alignment) { OS << "Alignment " << Alignment << "\n"; }

  if (Indexes)
    OS << Indexes->getMBBStartIdx(this) << '\t';

  OS << "BB#" << getNumber() << ": ";

  const char *Comma = "";
  if (const BasicBlock *LBB = getBasicBlock()) {
    OS << Comma << "derived from LLVM BB ";
    WriteAsOperand(OS, LBB, /*PrintType=*/false);
    Comma = ", ";
  }
  if (isLandingPad()) { OS << Comma << "EH LANDING PAD"; Comma = ", "; }
  if (hasAddressTaken()) { OS << Comma << "ADDRESS TAKEN"; Comma = ", "; }
  OS << '\n';

  const TargetRegisterInfo *TRI = MF->getTarget().getRegisterInfo();
  if (!livein_empty()) {
    if (Indexes) OS << '\t';
    OS << "    Live Ins:";
    for (livein_iterator I = livein_begin(),E = livein_end(); I != E; ++I)
      OS << ' ' << PrintReg(*I, TRI);
    OS << '\n';
  }
  // Print the preds of this block according to the CFG.
  if (!pred_empty()) {
    if (Indexes) OS << '\t';
    OS << "    Predecessors according to CFG:";
    for (const_pred_iterator PI = pred_begin(), E = pred_end(); PI != E; ++PI)
      OS << " BB#" << (*PI)->getNumber();
    OS << '\n';
  }

  for (const_iterator I = begin(); I != end(); ++I) {
    if (Indexes) {
      if (Indexes->hasIndex(I))
        OS << Indexes->getInstructionIndex(I);
      OS << '\t';
    }
    OS << '\t';
    I->print(OS, &getParent()->getTarget());
  }

  // Print the successors of this block according to the CFG.
  if (!succ_empty()) {
    if (Indexes) OS << '\t';
    OS << "    Successors according to CFG:";
    for (const_succ_iterator SI = succ_begin(), E = succ_end(); SI != E; ++SI)
      OS << " BB#" << (*SI)->getNumber();
    OS << '\n';
  }
}

void MachineBasicBlock::removeLiveIn(unsigned Reg) {
  std::vector<unsigned>::iterator I =
    std::find(LiveIns.begin(), LiveIns.end(), Reg);
  assert(I != LiveIns.end() && "Not a live in!");
  LiveIns.erase(I);
}

bool MachineBasicBlock::isLiveIn(unsigned Reg) const {
  livein_iterator I = std::find(livein_begin(), livein_end(), Reg);
  return I != livein_end();
}

void MachineBasicBlock::moveBefore(MachineBasicBlock *NewAfter) {
  getParent()->splice(NewAfter, this);
}

void MachineBasicBlock::moveAfter(MachineBasicBlock *NewBefore) {
  MachineFunction::iterator BBI = NewBefore;
  getParent()->splice(++BBI, this);
}

void MachineBasicBlock::updateTerminator() {
  const TargetInstrInfo *TII = getParent()->getTarget().getInstrInfo();
  // A block with no successors has no concerns with fall-through edges.
  if (this->succ_empty()) return;

  MachineBasicBlock *TBB = 0, *FBB = 0;
  SmallVector<MachineOperand, 4> Cond;
  DebugLoc dl;  // FIXME: this is nowhere
  bool B = TII->AnalyzeBranch(*this, TBB, FBB, Cond);
  (void) B;
  assert(!B && "UpdateTerminators requires analyzable predecessors!");
  if (Cond.empty()) {
    if (TBB) {
      // The block has an unconditional branch. If its successor is now
      // its layout successor, delete the branch.
      if (isLayoutSuccessor(TBB))
        TII->RemoveBranch(*this);
    } else {
      // The block has an unconditional fallthrough. If its successor is not
      // its layout successor, insert a branch.
      TBB = *succ_begin();
      if (!isLayoutSuccessor(TBB))
        TII->InsertBranch(*this, TBB, 0, Cond, dl);
    }
  } else {
    if (FBB) {
      // The block has a non-fallthrough conditional branch. If one of its
      // successors is its layout successor, rewrite it to a fallthrough
      // conditional branch.
      if (isLayoutSuccessor(TBB)) {
        if (TII->ReverseBranchCondition(Cond))
          return;
        TII->RemoveBranch(*this);
        TII->InsertBranch(*this, FBB, 0, Cond, dl);
      } else if (isLayoutSuccessor(FBB)) {
        TII->RemoveBranch(*this);
        TII->InsertBranch(*this, TBB, 0, Cond, dl);
      }
    } else {
      // The block has a fallthrough conditional branch.
      MachineBasicBlock *MBBA = *succ_begin();
      MachineBasicBlock *MBBB = *llvm::next(succ_begin());
      if (MBBA == TBB) std::swap(MBBB, MBBA);
      if (isLayoutSuccessor(TBB)) {
        if (TII->ReverseBranchCondition(Cond)) {
          // We can't reverse the condition, add an unconditional branch.
          Cond.clear();
          TII->InsertBranch(*this, MBBA, 0, Cond, dl);
          return;
        }
        TII->RemoveBranch(*this);
        TII->InsertBranch(*this, MBBA, 0, Cond, dl);
      } else if (!isLayoutSuccessor(MBBA)) {
        TII->RemoveBranch(*this);
        TII->InsertBranch(*this, TBB, MBBA, Cond, dl);
      }
    }
  }
}

void MachineBasicBlock::addSuccessor(MachineBasicBlock *succ, uint32_t weight) {

  // If we see non-zero value for the first time it means we actually use Weight
  // list, so we fill all Weights with 0's.
  if (weight != 0 && Weights.empty())
    Weights.resize(Successors.size());

  if (weight != 0 || !Weights.empty())
    Weights.push_back(weight);

   Successors.push_back(succ);
   succ->addPredecessor(this);
 }

void MachineBasicBlock::removeSuccessor(MachineBasicBlock *succ) {
  succ->removePredecessor(this);
  succ_iterator I = std::find(Successors.begin(), Successors.end(), succ);
  assert(I != Successors.end() && "Not a current successor!");

  // If Weight list is empty it means we don't use it (disabled optimization).
  if (!Weights.empty()) {
    weight_iterator WI = getWeightIterator(I);
    Weights.erase(WI);
  }

  Successors.erase(I);
}

MachineBasicBlock::succ_iterator
MachineBasicBlock::removeSuccessor(succ_iterator I) {
  assert(I != Successors.end() && "Not a current successor!");

  // If Weight list is empty it means we don't use it (disabled optimization).
  if (!Weights.empty()) {
    weight_iterator WI = getWeightIterator(I);
    Weights.erase(WI);
  }

  (*I)->removePredecessor(this);
  return Successors.erase(I);
}

void MachineBasicBlock::replaceSuccessor(MachineBasicBlock *Old,
                                         MachineBasicBlock *New) {
  uint32_t weight = 0;
  succ_iterator SI = std::find(Successors.begin(), Successors.end(), Old);

  // If Weight list is empty it means we don't use it (disabled optimization).
  if (!Weights.empty()) {
    weight_iterator WI = getWeightIterator(SI);
    weight = *WI;
  }

  // Update the successor information.
  removeSuccessor(SI);
  addSuccessor(New, weight);
}

void MachineBasicBlock::addPredecessor(MachineBasicBlock *pred) {
  Predecessors.push_back(pred);
}

void MachineBasicBlock::removePredecessor(MachineBasicBlock *pred) {
  pred_iterator I = std::find(Predecessors.begin(), Predecessors.end(), pred);
  assert(I != Predecessors.end() && "Pred is not a predecessor of this block!");
  Predecessors.erase(I);
}

void MachineBasicBlock::transferSuccessors(MachineBasicBlock *fromMBB) {
  if (this == fromMBB)
    return;

  while (!fromMBB->succ_empty()) {
    MachineBasicBlock *Succ = *fromMBB->succ_begin();
    uint32_t weight = 0;


    // If Weight list is empty it means we don't use it (disabled optimization).
    if (!fromMBB->Weights.empty())
      weight = *fromMBB->Weights.begin();

    addSuccessor(Succ, weight);
    fromMBB->removeSuccessor(Succ);
  }
}

void
MachineBasicBlock::transferSuccessorsAndUpdatePHIs(MachineBasicBlock *fromMBB) {
  if (this == fromMBB)
    return;

  while (!fromMBB->succ_empty()) {
    MachineBasicBlock *Succ = *fromMBB->succ_begin();
    addSuccessor(Succ);
    fromMBB->removeSuccessor(Succ);

    // Fix up any PHI nodes in the successor.
    for (MachineBasicBlock::iterator MI = Succ->begin(), ME = Succ->end();
         MI != ME && MI->isPHI(); ++MI)
      for (unsigned i = 2, e = MI->getNumOperands()+1; i != e; i += 2) {
        MachineOperand &MO = MI->getOperand(i);
        if (MO.getMBB() == fromMBB)
          MO.setMBB(this);
      }
  }
}

bool MachineBasicBlock::isSuccessor(const MachineBasicBlock *MBB) const {
  const_succ_iterator I = std::find(Successors.begin(), Successors.end(), MBB);
  return I != Successors.end();
}

bool MachineBasicBlock::isLayoutSuccessor(const MachineBasicBlock *MBB) const {
  MachineFunction::const_iterator I(this);
  return llvm::next(I) == MachineFunction::const_iterator(MBB);
}

bool MachineBasicBlock::canFallThrough() {
  MachineFunction::iterator Fallthrough = this;
  ++Fallthrough;
  // If FallthroughBlock is off the end of the function, it can't fall through.
  if (Fallthrough == getParent()->end())
    return false;

  // If FallthroughBlock isn't a successor, no fallthrough is possible.
  if (!isSuccessor(Fallthrough))
    return false;

  // Analyze the branches, if any, at the end of the block.
  MachineBasicBlock *TBB = 0, *FBB = 0;
  SmallVector<MachineOperand, 4> Cond;
  const TargetInstrInfo *TII = getParent()->getTarget().getInstrInfo();
  if (TII->AnalyzeBranch(*this, TBB, FBB, Cond)) {
    // If we couldn't analyze the branch, examine the last instruction.
    // If the block doesn't end in a known control barrier, assume fallthrough
    // is possible. The isPredicable check is needed because this code can be
    // called during IfConversion, where an instruction which is normally a
    // Barrier is predicated and thus no longer an actual control barrier. This
    // is over-conservative though, because if an instruction isn't actually
    // predicated we could still treat it like a barrier.
    return empty() || !back().getDesc().isBarrier() ||
           back().getDesc().isPredicable();
  }

  // If there is no branch, control always falls through.
  if (TBB == 0) return true;

  // If there is some explicit branch to the fallthrough block, it can obviously
  // reach, even though the branch should get folded to fall through implicitly.
  if (MachineFunction::iterator(TBB) == Fallthrough ||
      MachineFunction::iterator(FBB) == Fallthrough)
    return true;

  // If it's an unconditional branch to some block not the fall through, it
  // doesn't fall through.
  if (Cond.empty()) return false;

  // Otherwise, if it is conditional and has no explicit false block, it falls
  // through.
  return FBB == 0;
}

MachineBasicBlock *
MachineBasicBlock::SplitCriticalEdge(MachineBasicBlock *Succ, Pass *P) {
  MachineFunction *MF = getParent();
  DebugLoc dl;  // FIXME: this is nowhere

  // We may need to update this's terminator, but we can't do that if
  // AnalyzeBranch fails. If this uses a jump table, we won't touch it.
  const TargetInstrInfo *TII = MF->getTarget().getInstrInfo();
  MachineBasicBlock *TBB = 0, *FBB = 0;
  SmallVector<MachineOperand, 4> Cond;
  if (TII->AnalyzeBranch(*this, TBB, FBB, Cond))
    return NULL;

  // Avoid bugpoint weirdness: A block may end with a conditional branch but
  // jumps to the same MBB is either case. We have duplicate CFG edges in that
  // case that we can't handle. Since this never happens in properly optimized
  // code, just skip those edges.
  if (TBB && TBB == FBB) {
    DEBUG(dbgs() << "Won't split critical edge after degenerate BB#"
                 << getNumber() << '\n');
    return NULL;
  }

  MachineBasicBlock *NMBB = MF->CreateMachineBasicBlock();
  MF->insert(llvm::next(MachineFunction::iterator(this)), NMBB);
  DEBUG(dbgs() << "Splitting critical edge:"
        " BB#" << getNumber()
        << " -- BB#" << NMBB->getNumber()
        << " -- BB#" << Succ->getNumber() << '\n');

  // On some targets like Mips, branches may kill virtual registers. Make sure
  // that LiveVariables is properly updated after updateTerminator replaces the
  // terminators.
  LiveVariables *LV = P->getAnalysisIfAvailable<LiveVariables>();

  // Collect a list of virtual registers killed by the terminators.
  SmallVector<unsigned, 4> KilledRegs;
  if (LV)
    for (iterator I = getFirstTerminator(), E = end(); I != E; ++I) {
      MachineInstr *MI = I;
      for (MachineInstr::mop_iterator OI = MI->operands_begin(),
           OE = MI->operands_end(); OI != OE; ++OI) {
        if (!OI->isReg() || !OI->isUse() || !OI->isKill() || OI->isUndef())
          continue;
        unsigned Reg = OI->getReg();
        if (TargetRegisterInfo::isVirtualRegister(Reg) &&
            LV->getVarInfo(Reg).removeKill(MI)) {
          KilledRegs.push_back(Reg);
          DEBUG(dbgs() << "Removing terminator kill: " << *MI);
          OI->setIsKill(false);
        }
      }
    }

  ReplaceUsesOfBlockWith(Succ, NMBB);
  updateTerminator();

  // Insert unconditional "jump Succ" instruction in NMBB if necessary.
  NMBB->addSuccessor(Succ);
  if (!NMBB->isLayoutSuccessor(Succ)) {
    Cond.clear();
    MF->getTarget().getInstrInfo()->InsertBranch(*NMBB, Succ, NULL, Cond, dl);
  }

  // Fix PHI nodes in Succ so they refer to NMBB instead of this
  for (MachineBasicBlock::iterator i = Succ->begin(), e = Succ->end();
       i != e && i->isPHI(); ++i)
    for (unsigned ni = 1, ne = i->getNumOperands(); ni != ne; ni += 2)
      if (i->getOperand(ni+1).getMBB() == this)
        i->getOperand(ni+1).setMBB(NMBB);

  // Inherit live-ins from the successor
  for (MachineBasicBlock::livein_iterator I = Succ->livein_begin(),
	 E = Succ->livein_end(); I != E; ++I)
    NMBB->addLiveIn(*I);

  // Update LiveVariables.
  if (LV) {
    // Restore kills of virtual registers that were killed by the terminators.
    while (!KilledRegs.empty()) {
      unsigned Reg = KilledRegs.pop_back_val();
      for (iterator I = end(), E = begin(); I != E;) {
        if (!(--I)->addRegisterKilled(Reg, NULL, /* addIfNotFound= */ false))
          continue;
        LV->getVarInfo(Reg).Kills.push_back(I);
        DEBUG(dbgs() << "Restored terminator kill: " << *I);
        break;
      }
    }
    // Update relevant live-through information.
    LV->addNewBlock(NMBB, this, Succ);
  }

  if (MachineDominatorTree *MDT =
      P->getAnalysisIfAvailable<MachineDominatorTree>()) {
    // Update dominator information.
    MachineDomTreeNode *SucccDTNode = MDT->getNode(Succ);

    bool IsNewIDom = true;
    for (const_pred_iterator PI = Succ->pred_begin(), E = Succ->pred_end();
         PI != E; ++PI) {
      MachineBasicBlock *PredBB = *PI;
      if (PredBB == NMBB)
        continue;
      if (!MDT->dominates(SucccDTNode, MDT->getNode(PredBB))) {
        IsNewIDom = false;
        break;
      }
    }

    // We know "this" dominates the newly created basic block.
    MachineDomTreeNode *NewDTNode = MDT->addNewBlock(NMBB, this);

    // If all the other predecessors of "Succ" are dominated by "Succ" itself
    // then the new block is the new immediate dominator of "Succ". Otherwise,
    // the new block doesn't dominate anything.
    if (IsNewIDom)
      MDT->changeImmediateDominator(SucccDTNode, NewDTNode);
  }

  if (MachineLoopInfo *MLI = P->getAnalysisIfAvailable<MachineLoopInfo>())
    if (MachineLoop *TIL = MLI->getLoopFor(this)) {
      // If one or the other blocks were not in a loop, the new block is not
      // either, and thus LI doesn't need to be updated.
      if (MachineLoop *DestLoop = MLI->getLoopFor(Succ)) {
        if (TIL == DestLoop) {
          // Both in the same loop, the NMBB joins loop.
          DestLoop->addBasicBlockToLoop(NMBB, MLI->getBase());
        } else if (TIL->contains(DestLoop)) {
          // Edge from an outer loop to an inner loop.  Add to the outer loop.
          TIL->addBasicBlockToLoop(NMBB, MLI->getBase());
        } else if (DestLoop->contains(TIL)) {
          // Edge from an inner loop to an outer loop.  Add to the outer loop.
          DestLoop->addBasicBlockToLoop(NMBB, MLI->getBase());
        } else {
          // Edge from two loops with no containment relation.  Because these
          // are natural loops, we know that the destination block must be the
          // header of its loop (adding a branch into a loop elsewhere would
          // create an irreducible loop).
          assert(DestLoop->getHeader() == Succ &&
                 "Should not create irreducible loops!");
          if (MachineLoop *P = DestLoop->getParentLoop())
            P->addBasicBlockToLoop(NMBB, MLI->getBase());
        }
      }
    }

  return NMBB;
}

/// removeFromParent - This method unlinks 'this' from the containing function,
/// and returns it, but does not delete it.
MachineBasicBlock *MachineBasicBlock::removeFromParent() {
  assert(getParent() && "Not embedded in a function!");
  getParent()->remove(this);
  return this;
}


/// eraseFromParent - This method unlinks 'this' from the containing function,
/// and deletes it.
void MachineBasicBlock::eraseFromParent() {
  assert(getParent() && "Not embedded in a function!");
  getParent()->erase(this);
}


/// ReplaceUsesOfBlockWith - Given a machine basic block that branched to
/// 'Old', change the code and CFG so that it branches to 'New' instead.
void MachineBasicBlock::ReplaceUsesOfBlockWith(MachineBasicBlock *Old,
                                               MachineBasicBlock *New) {
  assert(Old != New && "Cannot replace self with self!");

  MachineBasicBlock::iterator I = end();
  while (I != begin()) {
    --I;
    if (!I->getDesc().isTerminator()) break;

    // Scan the operands of this machine instruction, replacing any uses of Old
    // with New.
    for (unsigned i = 0, e = I->getNumOperands(); i != e; ++i)
      if (I->getOperand(i).isMBB() &&
          I->getOperand(i).getMBB() == Old)
        I->getOperand(i).setMBB(New);
  }

  // Update the successor information.
  replaceSuccessor(Old, New);
}

/// CorrectExtraCFGEdges - Various pieces of code can cause excess edges in the
/// CFG to be inserted.  If we have proven that MBB can only branch to DestA and
/// DestB, remove any other MBB successors from the CFG.  DestA and DestB can be
/// null.
///
/// Besides DestA and DestB, retain other edges leading to LandingPads
/// (currently there can be only one; we don't check or require that here).
/// Note it is possible that DestA and/or DestB are LandingPads.
bool MachineBasicBlock::CorrectExtraCFGEdges(MachineBasicBlock *DestA,
                                             MachineBasicBlock *DestB,
                                             bool isCond) {
  // The values of DestA and DestB frequently come from a call to the
  // 'TargetInstrInfo::AnalyzeBranch' method. We take our meaning of the initial
  // values from there.
  //
  // 1. If both DestA and DestB are null, then the block ends with no branches
  //    (it falls through to its successor).
  // 2. If DestA is set, DestB is null, and isCond is false, then the block ends
  //    with only an unconditional branch.
  // 3. If DestA is set, DestB is null, and isCond is true, then the block ends
  //    with a conditional branch that falls through to a successor (DestB).
  // 4. If DestA and DestB is set and isCond is true, then the block ends with a
  //    conditional branch followed by an unconditional branch. DestA is the
  //    'true' destination and DestB is the 'false' destination.

  bool Changed = false;

  MachineFunction::iterator FallThru =
    llvm::next(MachineFunction::iterator(this));

  if (DestA == 0 && DestB == 0) {
    // Block falls through to successor.
    DestA = FallThru;
    DestB = FallThru;
  } else if (DestA != 0 && DestB == 0) {
    if (isCond)
      // Block ends in conditional jump that falls through to successor.
      DestB = FallThru;
  } else {
    assert(DestA && DestB && isCond &&
           "CFG in a bad state. Cannot correct CFG edges");
  }

  // Remove superfluous edges. I.e., those which aren't destinations of this
  // basic block, duplicate edges, or landing pads.
  SmallPtrSet<const MachineBasicBlock*, 8> SeenMBBs;
  MachineBasicBlock::succ_iterator SI = succ_begin();
  while (SI != succ_end()) {
    const MachineBasicBlock *MBB = *SI;
    if (!SeenMBBs.insert(MBB) ||
        (MBB != DestA && MBB != DestB && !MBB->isLandingPad())) {
      // This is a superfluous edge, remove it.
      SI = removeSuccessor(SI);
      Changed = true;
    } else {
      ++SI;
    }
  }

  return Changed;
}

/// findDebugLoc - find the next valid DebugLoc starting at MBBI, skipping
/// any DBG_VALUE instructions.  Return UnknownLoc if there is none.
DebugLoc
MachineBasicBlock::findDebugLoc(MachineBasicBlock::iterator &MBBI) {
  DebugLoc DL;
  MachineBasicBlock::iterator E = end();
  if (MBBI != E) {
    // Skip debug declarations, we don't want a DebugLoc from them.
    MachineBasicBlock::iterator MBBI2 = MBBI;
    while (MBBI2 != E && MBBI2->isDebugValue())
      MBBI2++;
    if (MBBI2 != E)
      DL = MBBI2->getDebugLoc();
  }
  return DL;
}

/// getSuccWeight - Return weight of the edge from this block to MBB.
///
uint32_t MachineBasicBlock::getSuccWeight(MachineBasicBlock *succ) {
  if (Weights.empty())
    return 0;

  succ_iterator I = std::find(Successors.begin(), Successors.end(), succ);
  return *getWeightIterator(I);
}

/// getWeightIterator - Return wight iterator corresonding to the I successor
/// iterator
MachineBasicBlock::weight_iterator MachineBasicBlock::
getWeightIterator(MachineBasicBlock::succ_iterator I) {
  assert(Weights.size() == Successors.size() && "Async weight list!");
  size_t index = std::distance(Successors.begin(), I);
  assert(index < Weights.size() && "Not a current successor!");
  return Weights.begin() + index;
}

void llvm::WriteAsOperand(raw_ostream &OS, const MachineBasicBlock *MBB,
                          bool t) {
  OS << "BB#" << MBB->getNumber();
}

