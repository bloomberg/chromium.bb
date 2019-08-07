//===---- LiveRangeCalc.cpp - Calculate live ranges -----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Implementation of the LiveRangeCalc class.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "regalloc"
#include "LiveRangeCalc.h"
#include "llvm/CodeGen/MachineDominators.h"

using namespace llvm;

void LiveRangeCalc::reset(const MachineFunction *MF) {
  unsigned N = MF->getNumBlockIDs();
  Seen.clear();
  Seen.resize(N);
  LiveOut.resize(N);
  LiveIn.clear();
}


// Transfer information from the LiveIn vector to the live ranges.
void LiveRangeCalc::updateLiveIns(VNInfo *OverrideVNI, SlotIndexes *Indexes) {
  for (SmallVectorImpl<LiveInBlock>::iterator I = LiveIn.begin(),
         E = LiveIn.end(); I != E; ++I) {
    if (!I->DomNode)
      continue;
    MachineBasicBlock *MBB = I->DomNode->getBlock();

    VNInfo *VNI = OverrideVNI ? OverrideVNI : I->Value;
    assert(VNI && "No live-in value found");

    SlotIndex Start, End;
    tie(Start, End) = Indexes->getMBBRange(MBB);

    if (I->Kill.isValid())
      I->LI->addRange(LiveRange(Start, I->Kill, VNI));
    else {
      I->LI->addRange(LiveRange(Start, End, VNI));
      // The value is live-through, update LiveOut as well.  Defer the Domtree
      // lookup until it is needed.
      assert(Seen.test(MBB->getNumber()));
      LiveOut[MBB] = LiveOutPair(VNI, (MachineDomTreeNode *)0);
    }
  }
  LiveIn.clear();
}


void LiveRangeCalc::extend(LiveInterval *LI,
                           SlotIndex Kill,
                           SlotIndexes *Indexes,
                           MachineDominatorTree *DomTree,
                           VNInfo::Allocator *Alloc) {
  assert(LI && "Missing live range");
  assert(Kill.isValid() && "Invalid SlotIndex");
  assert(Indexes && "Missing SlotIndexes");
  assert(DomTree && "Missing dominator tree");

  MachineBasicBlock *KillMBB = Indexes->getMBBFromIndex(Kill.getPrevSlot());
  assert(Kill && "No MBB at Kill");

  // Is there a def in the same MBB we can extend?
  if (LI->extendInBlock(Indexes->getMBBStartIdx(KillMBB), Kill))
    return;

  // Find the single reaching def, or determine if Kill is jointly dominated by
  // multiple values, and we may need to create even more phi-defs to preserve
  // VNInfo SSA form.  Perform a search for all predecessor blocks where we
  // know the dominating VNInfo.
  VNInfo *VNI = findReachingDefs(LI, KillMBB, Kill, Indexes, DomTree);

  // When there were multiple different values, we may need new PHIs.
  if (!VNI)
    updateSSA(Indexes, DomTree, Alloc);

  updateLiveIns(VNI, Indexes);
}


// This function is called by a client after using the low-level API to add
// live-out and live-in blocks.  The unique value optimization is not
// available, SplitEditor::transferValues handles that case directly anyway.
void LiveRangeCalc::calculateValues(SlotIndexes *Indexes,
                                    MachineDominatorTree *DomTree,
                                    VNInfo::Allocator *Alloc) {
  assert(Indexes && "Missing SlotIndexes");
  assert(DomTree && "Missing dominator tree");
  updateSSA(Indexes, DomTree, Alloc);
  updateLiveIns(0, Indexes);
}


VNInfo *LiveRangeCalc::findReachingDefs(LiveInterval *LI,
                                        MachineBasicBlock *KillMBB,
                                        SlotIndex Kill,
                                        SlotIndexes *Indexes,
                                        MachineDominatorTree *DomTree) {
  // Blocks where LI should be live-in.
  SmallVector<MachineBasicBlock*, 16> WorkList(1, KillMBB);

  // Remember if we have seen more than one value.
  bool UniqueVNI = true;
  VNInfo *TheVNI = 0;

  // Using Seen as a visited set, perform a BFS for all reaching defs.
  for (unsigned i = 0; i != WorkList.size(); ++i) {
    MachineBasicBlock *MBB = WorkList[i];
    assert(!MBB->pred_empty() && "Value live-in to entry block?");
    for (MachineBasicBlock::pred_iterator PI = MBB->pred_begin(),
           PE = MBB->pred_end(); PI != PE; ++PI) {
       MachineBasicBlock *Pred = *PI;

       // Is this a known live-out block?
       if (Seen.test(Pred->getNumber())) {
         if (VNInfo *VNI = LiveOut[Pred].first) {
           if (TheVNI && TheVNI != VNI)
             UniqueVNI = false;
           TheVNI = VNI;
         }
         continue;
       }

       SlotIndex Start, End;
       tie(Start, End) = Indexes->getMBBRange(Pred);

       // First time we see Pred.  Try to determine the live-out value, but set
       // it as null if Pred is live-through with an unknown value.
       VNInfo *VNI = LI->extendInBlock(Start, End);
       setLiveOutValue(Pred, VNI);
       if (VNI) {
         if (TheVNI && TheVNI != VNI)
           UniqueVNI = false;
         TheVNI = VNI;
         continue;
       }

       // No, we need a live-in value for Pred as well
       if (Pred != KillMBB)
          WorkList.push_back(Pred);
       else
          // Loopback to KillMBB, so value is really live through.
         Kill = SlotIndex();
    }
  }

  // Transfer WorkList to LiveInBlocks in reverse order.
  // This ordering works best with updateSSA().
  LiveIn.clear();
  LiveIn.reserve(WorkList.size());
  while(!WorkList.empty())
    addLiveInBlock(LI, DomTree->getNode(WorkList.pop_back_val()));

  // The kill block may not be live-through.
  assert(LiveIn.back().DomNode->getBlock() == KillMBB);
  LiveIn.back().Kill = Kill;

  return UniqueVNI ? TheVNI : 0;
}


// This is essentially the same iterative algorithm that SSAUpdater uses,
// except we already have a dominator tree, so we don't have to recompute it.
void LiveRangeCalc::updateSSA(SlotIndexes *Indexes,
                              MachineDominatorTree *DomTree,
                              VNInfo::Allocator *Alloc) {
  assert(Indexes && "Missing SlotIndexes");
  assert(DomTree && "Missing dominator tree");

  // Interate until convergence.
  unsigned Changes;
  do {
    Changes = 0;
    // Propagate live-out values down the dominator tree, inserting phi-defs
    // when necessary.
    for (SmallVectorImpl<LiveInBlock>::iterator I = LiveIn.begin(),
           E = LiveIn.end(); I != E; ++I) {
      MachineDomTreeNode *Node = I->DomNode;
      // Skip block if the live-in value has already been determined.
      if (!Node)
        continue;
      MachineBasicBlock *MBB = Node->getBlock();
      MachineDomTreeNode *IDom = Node->getIDom();
      LiveOutPair IDomValue;

      // We need a live-in value to a block with no immediate dominator?
      // This is probably an unreachable block that has survived somehow.
      bool needPHI = !IDom || !Seen.test(IDom->getBlock()->getNumber());

      // IDom dominates all of our predecessors, but it may not be their
      // immediate dominator. Check if any of them have live-out values that are
      // properly dominated by IDom. If so, we need a phi-def here.
      if (!needPHI) {
        IDomValue = LiveOut[IDom->getBlock()];

        // Cache the DomTree node that defined the value.
        if (IDomValue.first && !IDomValue.second)
          LiveOut[IDom->getBlock()].second = IDomValue.second =
            DomTree->getNode(Indexes->getMBBFromIndex(IDomValue.first->def));

        for (MachineBasicBlock::pred_iterator PI = MBB->pred_begin(),
               PE = MBB->pred_end(); PI != PE; ++PI) {
          LiveOutPair &Value = LiveOut[*PI];
          if (!Value.first || Value.first == IDomValue.first)
            continue;

          // Cache the DomTree node that defined the value.
          if (!Value.second)
            Value.second =
              DomTree->getNode(Indexes->getMBBFromIndex(Value.first->def));

          // This predecessor is carrying something other than IDomValue.
          // It could be because IDomValue hasn't propagated yet, or it could be
          // because MBB is in the dominance frontier of that value.
          if (DomTree->dominates(IDom, Value.second)) {
            needPHI = true;
            break;
          }
        }
      }

      // The value may be live-through even if Kill is set, as can happen when
      // we are called from extendRange. In that case LiveOutSeen is true, and
      // LiveOut indicates a foreign or missing value.
      LiveOutPair &LOP = LiveOut[MBB];

      // Create a phi-def if required.
      if (needPHI) {
        ++Changes;
        assert(Alloc && "Need VNInfo allocator to create PHI-defs");
        SlotIndex Start, End;
        tie(Start, End) = Indexes->getMBBRange(MBB);
        VNInfo *VNI = I->LI->getNextValue(Start, 0, *Alloc);
        VNI->setIsPHIDef(true);
        I->Value = VNI;
        // This block is done, we know the final value.
        I->DomNode = 0;

        // Add liveness since updateLiveIns now skips this node.
        if (I->Kill.isValid())
          I->LI->addRange(LiveRange(Start, I->Kill, VNI));
        else {
          I->LI->addRange(LiveRange(Start, End, VNI));
          LOP = LiveOutPair(VNI, Node);
        }
      } else if (IDomValue.first) {
        // No phi-def here. Remember incoming value.
        I->Value = IDomValue.first;

        // If the IDomValue is killed in the block, don't propagate through.
        if (I->Kill.isValid())
          continue;

        // Propagate IDomValue if it isn't killed:
        // MBB is live-out and doesn't define its own value.
        if (LOP.first == IDomValue.first)
          continue;
        ++Changes;
        LOP = IDomValue;
      }
    }
  } while (Changes);
}
