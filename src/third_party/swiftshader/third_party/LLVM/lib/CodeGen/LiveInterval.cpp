//===-- LiveInterval.cpp - Live Interval Representation -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the LiveRange and LiveInterval classes.  Given some
// numbering of each the machine instructions an interval [i, j) is said to be a
// live interval for register v if there is no instruction with number j' > j
// such that v is live at j' and there is no instruction with number i' < i such
// that v is live at i'. In this implementation intervals can have holes,
// i.e. an interval might look like [1,20), [50,65), [1000,1001).  Each
// individual range is represented as an instance of LiveRange, and the whole
// interval is represented as an instance of LiveInterval.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/LiveInterval.h"
#include "llvm/CodeGen/LiveIntervalAnalysis.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetRegisterInfo.h"
#include <algorithm>
using namespace llvm;

LiveInterval::iterator LiveInterval::find(SlotIndex Pos) {
  // This algorithm is basically std::upper_bound.
  // Unfortunately, std::upper_bound cannot be used with mixed types until we
  // adopt C++0x. Many libraries can do it, but not all.
  if (empty() || Pos >= endIndex())
    return end();
  iterator I = begin();
  size_t Len = ranges.size();
  do {
    size_t Mid = Len >> 1;
    if (Pos < I[Mid].end)
      Len = Mid;
    else
      I += Mid + 1, Len -= Mid + 1;
  } while (Len);
  return I;
}

/// killedInRange - Return true if the interval has kills in [Start,End).
bool LiveInterval::killedInRange(SlotIndex Start, SlotIndex End) const {
  Ranges::const_iterator r =
    std::lower_bound(ranges.begin(), ranges.end(), End);

  // Now r points to the first interval with start >= End, or ranges.end().
  if (r == ranges.begin())
    return false;

  --r;
  // Now r points to the last interval with end <= End.
  // r->end is the kill point.
  return r->end >= Start && r->end < End;
}

// overlaps - Return true if the intersection of the two live intervals is
// not empty.
//
// An example for overlaps():
//
// 0: A = ...
// 4: B = ...
// 8: C = A + B ;; last use of A
//
// The live intervals should look like:
//
// A = [3, 11)
// B = [7, x)
// C = [11, y)
//
// A->overlaps(C) should return false since we want to be able to join
// A and C.
//
bool LiveInterval::overlapsFrom(const LiveInterval& other,
                                const_iterator StartPos) const {
  assert(!empty() && "empty interval");
  const_iterator i = begin();
  const_iterator ie = end();
  const_iterator j = StartPos;
  const_iterator je = other.end();

  assert((StartPos->start <= i->start || StartPos == other.begin()) &&
         StartPos != other.end() && "Bogus start position hint!");

  if (i->start < j->start) {
    i = std::upper_bound(i, ie, j->start);
    if (i != ranges.begin()) --i;
  } else if (j->start < i->start) {
    ++StartPos;
    if (StartPos != other.end() && StartPos->start <= i->start) {
      assert(StartPos < other.end() && i < end());
      j = std::upper_bound(j, je, i->start);
      if (j != other.ranges.begin()) --j;
    }
  } else {
    return true;
  }

  if (j == je) return false;

  while (i != ie) {
    if (i->start > j->start) {
      std::swap(i, j);
      std::swap(ie, je);
    }

    if (i->end > j->start)
      return true;
    ++i;
  }

  return false;
}

/// overlaps - Return true if the live interval overlaps a range specified
/// by [Start, End).
bool LiveInterval::overlaps(SlotIndex Start, SlotIndex End) const {
  assert(Start < End && "Invalid range");
  const_iterator I = std::lower_bound(begin(), end(), End);
  return I != begin() && (--I)->end > Start;
}


/// ValNo is dead, remove it.  If it is the largest value number, just nuke it
/// (and any other deleted values neighboring it), otherwise mark it as ~1U so
/// it can be nuked later.
void LiveInterval::markValNoForDeletion(VNInfo *ValNo) {
  if (ValNo->id == getNumValNums()-1) {
    do {
      valnos.pop_back();
    } while (!valnos.empty() && valnos.back()->isUnused());
  } else {
    ValNo->setIsUnused(true);
  }
}

/// RenumberValues - Renumber all values in order of appearance and delete the
/// remaining unused values.
void LiveInterval::RenumberValues(LiveIntervals &lis) {
  SmallPtrSet<VNInfo*, 8> Seen;
  valnos.clear();
  for (const_iterator I = begin(), E = end(); I != E; ++I) {
    VNInfo *VNI = I->valno;
    if (!Seen.insert(VNI))
      continue;
    assert(!VNI->isUnused() && "Unused valno used by live range");
    VNI->id = (unsigned)valnos.size();
    valnos.push_back(VNI);
  }
}

/// extendIntervalEndTo - This method is used when we want to extend the range
/// specified by I to end at the specified endpoint.  To do this, we should
/// merge and eliminate all ranges that this will overlap with.  The iterator is
/// not invalidated.
void LiveInterval::extendIntervalEndTo(Ranges::iterator I, SlotIndex NewEnd) {
  assert(I != ranges.end() && "Not a valid interval!");
  VNInfo *ValNo = I->valno;

  // Search for the first interval that we can't merge with.
  Ranges::iterator MergeTo = llvm::next(I);
  for (; MergeTo != ranges.end() && NewEnd >= MergeTo->end; ++MergeTo) {
    assert(MergeTo->valno == ValNo && "Cannot merge with differing values!");
  }

  // If NewEnd was in the middle of an interval, make sure to get its endpoint.
  I->end = std::max(NewEnd, prior(MergeTo)->end);

  // Erase any dead ranges.
  ranges.erase(llvm::next(I), MergeTo);

  // If the newly formed range now touches the range after it and if they have
  // the same value number, merge the two ranges into one range.
  Ranges::iterator Next = llvm::next(I);
  if (Next != ranges.end() && Next->start <= I->end && Next->valno == ValNo) {
    I->end = Next->end;
    ranges.erase(Next);
  }
}


/// extendIntervalStartTo - This method is used when we want to extend the range
/// specified by I to start at the specified endpoint.  To do this, we should
/// merge and eliminate all ranges that this will overlap with.
LiveInterval::Ranges::iterator
LiveInterval::extendIntervalStartTo(Ranges::iterator I, SlotIndex NewStart) {
  assert(I != ranges.end() && "Not a valid interval!");
  VNInfo *ValNo = I->valno;

  // Search for the first interval that we can't merge with.
  Ranges::iterator MergeTo = I;
  do {
    if (MergeTo == ranges.begin()) {
      I->start = NewStart;
      ranges.erase(MergeTo, I);
      return I;
    }
    assert(MergeTo->valno == ValNo && "Cannot merge with differing values!");
    --MergeTo;
  } while (NewStart <= MergeTo->start);

  // If we start in the middle of another interval, just delete a range and
  // extend that interval.
  if (MergeTo->end >= NewStart && MergeTo->valno == ValNo) {
    MergeTo->end = I->end;
  } else {
    // Otherwise, extend the interval right after.
    ++MergeTo;
    MergeTo->start = NewStart;
    MergeTo->end = I->end;
  }

  ranges.erase(llvm::next(MergeTo), llvm::next(I));
  return MergeTo;
}

LiveInterval::iterator
LiveInterval::addRangeFrom(LiveRange LR, iterator From) {
  SlotIndex Start = LR.start, End = LR.end;
  iterator it = std::upper_bound(From, ranges.end(), Start);

  // If the inserted interval starts in the middle or right at the end of
  // another interval, just extend that interval to contain the range of LR.
  if (it != ranges.begin()) {
    iterator B = prior(it);
    if (LR.valno == B->valno) {
      if (B->start <= Start && B->end >= Start) {
        extendIntervalEndTo(B, End);
        return B;
      }
    } else {
      // Check to make sure that we are not overlapping two live ranges with
      // different valno's.
      assert(B->end <= Start &&
             "Cannot overlap two LiveRanges with differing ValID's"
             " (did you def the same reg twice in a MachineInstr?)");
    }
  }

  // Otherwise, if this range ends in the middle of, or right next to, another
  // interval, merge it into that interval.
  if (it != ranges.end()) {
    if (LR.valno == it->valno) {
      if (it->start <= End) {
        it = extendIntervalStartTo(it, Start);

        // If LR is a complete superset of an interval, we may need to grow its
        // endpoint as well.
        if (End > it->end)
          extendIntervalEndTo(it, End);
        return it;
      }
    } else {
      // Check to make sure that we are not overlapping two live ranges with
      // different valno's.
      assert(it->start >= End &&
             "Cannot overlap two LiveRanges with differing ValID's");
    }
  }

  // Otherwise, this is just a new range that doesn't interact with anything.
  // Insert it.
  return ranges.insert(it, LR);
}

/// extendInBlock - If this interval is live before Kill in the basic
/// block that starts at StartIdx, extend it to be live up to Kill and return
/// the value. If there is no live range before Kill, return NULL.
VNInfo *LiveInterval::extendInBlock(SlotIndex StartIdx, SlotIndex Kill) {
  if (empty())
    return 0;
  iterator I = std::upper_bound(begin(), end(), Kill.getPrevSlot());
  if (I == begin())
    return 0;
  --I;
  if (I->end <= StartIdx)
    return 0;
  if (I->end < Kill)
    extendIntervalEndTo(I, Kill);
  return I->valno;
}

/// removeRange - Remove the specified range from this interval.  Note that
/// the range must be in a single LiveRange in its entirety.
void LiveInterval::removeRange(SlotIndex Start, SlotIndex End,
                               bool RemoveDeadValNo) {
  // Find the LiveRange containing this span.
  Ranges::iterator I = find(Start);
  assert(I != ranges.end() && "Range is not in interval!");
  assert(I->containsRange(Start, End) && "Range is not entirely in interval!");

  // If the span we are removing is at the start of the LiveRange, adjust it.
  VNInfo *ValNo = I->valno;
  if (I->start == Start) {
    if (I->end == End) {
      if (RemoveDeadValNo) {
        // Check if val# is dead.
        bool isDead = true;
        for (const_iterator II = begin(), EE = end(); II != EE; ++II)
          if (II != I && II->valno == ValNo) {
            isDead = false;
            break;
          }
        if (isDead) {
          // Now that ValNo is dead, remove it.
          markValNoForDeletion(ValNo);
        }
      }

      ranges.erase(I);  // Removed the whole LiveRange.
    } else
      I->start = End;
    return;
  }

  // Otherwise if the span we are removing is at the end of the LiveRange,
  // adjust the other way.
  if (I->end == End) {
    I->end = Start;
    return;
  }

  // Otherwise, we are splitting the LiveRange into two pieces.
  SlotIndex OldEnd = I->end;
  I->end = Start;   // Trim the old interval.

  // Insert the new one.
  ranges.insert(llvm::next(I), LiveRange(End, OldEnd, ValNo));
}

/// removeValNo - Remove all the ranges defined by the specified value#.
/// Also remove the value# from value# list.
void LiveInterval::removeValNo(VNInfo *ValNo) {
  if (empty()) return;
  Ranges::iterator I = ranges.end();
  Ranges::iterator E = ranges.begin();
  do {
    --I;
    if (I->valno == ValNo)
      ranges.erase(I);
  } while (I != E);
  // Now that ValNo is dead, remove it.
  markValNoForDeletion(ValNo);
}

/// findDefinedVNInfo - Find the VNInfo defined by the specified
/// index (register interval).
VNInfo *LiveInterval::findDefinedVNInfoForRegInt(SlotIndex Idx) const {
  for (LiveInterval::const_vni_iterator i = vni_begin(), e = vni_end();
       i != e; ++i) {
    if ((*i)->def == Idx)
      return *i;
  }

  return 0;
}

/// join - Join two live intervals (this, and other) together.  This applies
/// mappings to the value numbers in the LHS/RHS intervals as specified.  If
/// the intervals are not joinable, this aborts.
void LiveInterval::join(LiveInterval &Other,
                        const int *LHSValNoAssignments,
                        const int *RHSValNoAssignments,
                        SmallVector<VNInfo*, 16> &NewVNInfo,
                        MachineRegisterInfo *MRI) {
  // Determine if any of our live range values are mapped.  This is uncommon, so
  // we want to avoid the interval scan if not.
  bool MustMapCurValNos = false;
  unsigned NumVals = getNumValNums();
  unsigned NumNewVals = NewVNInfo.size();
  for (unsigned i = 0; i != NumVals; ++i) {
    unsigned LHSValID = LHSValNoAssignments[i];
    if (i != LHSValID ||
        (NewVNInfo[LHSValID] && NewVNInfo[LHSValID] != getValNumInfo(i)))
      MustMapCurValNos = true;
  }

  // If we have to apply a mapping to our base interval assignment, rewrite it
  // now.
  if (MustMapCurValNos) {
    // Map the first live range.
    iterator OutIt = begin();
    OutIt->valno = NewVNInfo[LHSValNoAssignments[OutIt->valno->id]];
    ++OutIt;
    for (iterator I = OutIt, E = end(); I != E; ++I) {
      OutIt->valno = NewVNInfo[LHSValNoAssignments[I->valno->id]];

      // If this live range has the same value # as its immediate predecessor,
      // and if they are neighbors, remove one LiveRange.  This happens when we
      // have [0,3:0)[4,7:1) and map 0/1 onto the same value #.
      if (OutIt->valno == (OutIt-1)->valno && (OutIt-1)->end == OutIt->start) {
        (OutIt-1)->end = OutIt->end;
      } else {
        if (I != OutIt) {
          OutIt->start = I->start;
          OutIt->end = I->end;
        }

        // Didn't merge, on to the next one.
        ++OutIt;
      }
    }

    // If we merge some live ranges, chop off the end.
    ranges.erase(OutIt, end());
  }

  // Remember assignements because val# ids are changing.
  SmallVector<unsigned, 16> OtherAssignments;
  for (iterator I = Other.begin(), E = Other.end(); I != E; ++I)
    OtherAssignments.push_back(RHSValNoAssignments[I->valno->id]);

  // Update val# info. Renumber them and make sure they all belong to this
  // LiveInterval now. Also remove dead val#'s.
  unsigned NumValNos = 0;
  for (unsigned i = 0; i < NumNewVals; ++i) {
    VNInfo *VNI = NewVNInfo[i];
    if (VNI) {
      if (NumValNos >= NumVals)
        valnos.push_back(VNI);
      else
        valnos[NumValNos] = VNI;
      VNI->id = NumValNos++;  // Renumber val#.
    }
  }
  if (NumNewVals < NumVals)
    valnos.resize(NumNewVals);  // shrinkify

  // Okay, now insert the RHS live ranges into the LHS.
  iterator InsertPos = begin();
  unsigned RangeNo = 0;
  for (iterator I = Other.begin(), E = Other.end(); I != E; ++I, ++RangeNo) {
    // Map the valno in the other live range to the current live range.
    I->valno = NewVNInfo[OtherAssignments[RangeNo]];
    assert(I->valno && "Adding a dead range?");
    InsertPos = addRangeFrom(*I, InsertPos);
  }

  ComputeJoinedWeight(Other);
}

/// MergeRangesInAsValue - Merge all of the intervals in RHS into this live
/// interval as the specified value number.  The LiveRanges in RHS are
/// allowed to overlap with LiveRanges in the current interval, but only if
/// the overlapping LiveRanges have the specified value number.
void LiveInterval::MergeRangesInAsValue(const LiveInterval &RHS,
                                        VNInfo *LHSValNo) {
  // TODO: Make this more efficient.
  iterator InsertPos = begin();
  for (const_iterator I = RHS.begin(), E = RHS.end(); I != E; ++I) {
    // Map the valno in the other live range to the current live range.
    LiveRange Tmp = *I;
    Tmp.valno = LHSValNo;
    InsertPos = addRangeFrom(Tmp, InsertPos);
  }
}


/// MergeValueInAsValue - Merge all of the live ranges of a specific val#
/// in RHS into this live interval as the specified value number.
/// The LiveRanges in RHS are allowed to overlap with LiveRanges in the
/// current interval, it will replace the value numbers of the overlaped
/// live ranges with the specified value number.
void LiveInterval::MergeValueInAsValue(
                                    const LiveInterval &RHS,
                                    const VNInfo *RHSValNo, VNInfo *LHSValNo) {
  // TODO: Make this more efficient.
  iterator InsertPos = begin();
  for (const_iterator I = RHS.begin(), E = RHS.end(); I != E; ++I) {
    if (I->valno != RHSValNo)
      continue;
    // Map the valno in the other live range to the current live range.
    LiveRange Tmp = *I;
    Tmp.valno = LHSValNo;
    InsertPos = addRangeFrom(Tmp, InsertPos);
  }
}


/// MergeValueNumberInto - This method is called when two value nubmers
/// are found to be equivalent.  This eliminates V1, replacing all
/// LiveRanges with the V1 value number with the V2 value number.  This can
/// cause merging of V1/V2 values numbers and compaction of the value space.
VNInfo* LiveInterval::MergeValueNumberInto(VNInfo *V1, VNInfo *V2) {
  assert(V1 != V2 && "Identical value#'s are always equivalent!");

  // This code actually merges the (numerically) larger value number into the
  // smaller value number, which is likely to allow us to compactify the value
  // space.  The only thing we have to be careful of is to preserve the
  // instruction that defines the result value.

  // Make sure V2 is smaller than V1.
  if (V1->id < V2->id) {
    V1->copyFrom(*V2);
    std::swap(V1, V2);
  }

  // Merge V1 live ranges into V2.
  for (iterator I = begin(); I != end(); ) {
    iterator LR = I++;
    if (LR->valno != V1) continue;  // Not a V1 LiveRange.

    // Okay, we found a V1 live range.  If it had a previous, touching, V2 live
    // range, extend it.
    if (LR != begin()) {
      iterator Prev = LR-1;
      if (Prev->valno == V2 && Prev->end == LR->start) {
        Prev->end = LR->end;

        // Erase this live-range.
        ranges.erase(LR);
        I = Prev+1;
        LR = Prev;
      }
    }

    // Okay, now we have a V1 or V2 live range that is maximally merged forward.
    // Ensure that it is a V2 live-range.
    LR->valno = V2;

    // If we can merge it into later V2 live ranges, do so now.  We ignore any
    // following V1 live ranges, as they will be merged in subsequent iterations
    // of the loop.
    if (I != end()) {
      if (I->start == LR->end && I->valno == V2) {
        LR->end = I->end;
        ranges.erase(I);
        I = LR+1;
      }
    }
  }

  // Merge the relevant flags.
  V2->mergeFlags(V1);

  // Now that V1 is dead, remove it.
  markValNoForDeletion(V1);

  return V2;
}

void LiveInterval::Copy(const LiveInterval &RHS,
                        MachineRegisterInfo *MRI,
                        VNInfo::Allocator &VNInfoAllocator) {
  ranges.clear();
  valnos.clear();
  std::pair<unsigned, unsigned> Hint = MRI->getRegAllocationHint(RHS.reg);
  MRI->setRegAllocationHint(reg, Hint.first, Hint.second);

  weight = RHS.weight;
  for (unsigned i = 0, e = RHS.getNumValNums(); i != e; ++i) {
    const VNInfo *VNI = RHS.getValNumInfo(i);
    createValueCopy(VNI, VNInfoAllocator);
  }
  for (unsigned i = 0, e = RHS.ranges.size(); i != e; ++i) {
    const LiveRange &LR = RHS.ranges[i];
    addRange(LiveRange(LR.start, LR.end, getValNumInfo(LR.valno->id)));
  }
}

unsigned LiveInterval::getSize() const {
  unsigned Sum = 0;
  for (const_iterator I = begin(), E = end(); I != E; ++I)
    Sum += I->start.distance(I->end);
  return Sum;
}

/// ComputeJoinedWeight - Set the weight of a live interval Joined
/// after Other has been merged into it.
void LiveInterval::ComputeJoinedWeight(const LiveInterval &Other) {
  // If either of these intervals was spilled, the weight is the
  // weight of the non-spilled interval.  This can only happen with
  // iterative coalescers.

  if (Other.weight != HUGE_VALF) {
    weight += Other.weight;
  }
  else if (weight == HUGE_VALF &&
      !TargetRegisterInfo::isPhysicalRegister(reg)) {
    // Remove this assert if you have an iterative coalescer
    assert(0 && "Joining to spilled interval");
    weight = Other.weight;
  }
  else {
    // Otherwise the weight stays the same
    // Remove this assert if you have an iterative coalescer
    assert(0 && "Joining from spilled interval");
  }
}

raw_ostream& llvm::operator<<(raw_ostream& os, const LiveRange &LR) {
  return os << '[' << LR.start << ',' << LR.end << ':' << LR.valno->id << ")";
}

void LiveRange::dump() const {
  dbgs() << *this << "\n";
}

void LiveInterval::print(raw_ostream &OS, const TargetRegisterInfo *TRI) const {
  OS << PrintReg(reg, TRI);
  if (weight != 0)
    OS << ',' << weight;

  if (empty())
    OS << " EMPTY";
  else {
    OS << " = ";
    for (LiveInterval::Ranges::const_iterator I = ranges.begin(),
           E = ranges.end(); I != E; ++I) {
      OS << *I;
      assert(I->valno == getValNumInfo(I->valno->id) && "Bad VNInfo");
    }
  }

  // Print value number info.
  if (getNumValNums()) {
    OS << "  ";
    unsigned vnum = 0;
    for (const_vni_iterator i = vni_begin(), e = vni_end(); i != e;
         ++i, ++vnum) {
      const VNInfo *vni = *i;
      if (vnum) OS << " ";
      OS << vnum << "@";
      if (vni->isUnused()) {
        OS << "x";
      } else {
        OS << vni->def;
        if (vni->isPHIDef())
          OS << "-phidef";
        if (vni->hasPHIKill())
          OS << "-phikill";
        if (vni->hasRedefByEC())
          OS << "-ec";
      }
    }
  }
}

void LiveInterval::dump() const {
  dbgs() << *this << "\n";
}


void LiveRange::print(raw_ostream &os) const {
  os << *this;
}

unsigned ConnectedVNInfoEqClasses::Classify(const LiveInterval *LI) {
  // Create initial equivalence classes.
  EqClass.clear();
  EqClass.grow(LI->getNumValNums());

  const VNInfo *used = 0, *unused = 0;

  // Determine connections.
  for (LiveInterval::const_vni_iterator I = LI->vni_begin(), E = LI->vni_end();
       I != E; ++I) {
    const VNInfo *VNI = *I;
    // Group all unused values into one class.
    if (VNI->isUnused()) {
      if (unused)
        EqClass.join(unused->id, VNI->id);
      unused = VNI;
      continue;
    }
    used = VNI;
    if (VNI->isPHIDef()) {
      const MachineBasicBlock *MBB = LIS.getMBBFromIndex(VNI->def);
      assert(MBB && "Phi-def has no defining MBB");
      // Connect to values live out of predecessors.
      for (MachineBasicBlock::const_pred_iterator PI = MBB->pred_begin(),
           PE = MBB->pred_end(); PI != PE; ++PI)
        if (const VNInfo *PVNI =
              LI->getVNInfoAt(LIS.getMBBEndIdx(*PI).getPrevSlot()))
          EqClass.join(VNI->id, PVNI->id);
    } else {
      // Normal value defined by an instruction. Check for two-addr redef.
      // FIXME: This could be coincidental. Should we really check for a tied
      // operand constraint?
      // Note that VNI->def may be a use slot for an early clobber def.
      if (const VNInfo *UVNI = LI->getVNInfoAt(VNI->def.getPrevSlot()))
        EqClass.join(VNI->id, UVNI->id);
    }
  }

  // Lump all the unused values in with the last used value.
  if (used && unused)
    EqClass.join(used->id, unused->id);

  EqClass.compress();
  return EqClass.getNumClasses();
}

void ConnectedVNInfoEqClasses::Distribute(LiveInterval *LIV[],
                                          MachineRegisterInfo &MRI) {
  assert(LIV[0] && "LIV[0] must be set");
  LiveInterval &LI = *LIV[0];

  // Rewrite instructions.
  for (MachineRegisterInfo::reg_iterator RI = MRI.reg_begin(LI.reg),
       RE = MRI.reg_end(); RI != RE;) {
    MachineOperand &MO = RI.getOperand();
    MachineInstr *MI = MO.getParent();
    ++RI;
    if (MO.isUse() && MO.isUndef())
      continue;
    // DBG_VALUE instructions should have been eliminated earlier.
    SlotIndex Idx = LIS.getInstructionIndex(MI);
    Idx = MO.isUse() ? Idx.getUseIndex() : Idx.getDefIndex();
    const VNInfo *VNI = LI.getVNInfoAt(Idx);
    assert(VNI && "Interval not live at use.");
    MO.setReg(LIV[getEqClass(VNI)]->reg);
  }

  // Move runs to new intervals.
  LiveInterval::iterator J = LI.begin(), E = LI.end();
  while (J != E && EqClass[J->valno->id] == 0)
    ++J;
  for (LiveInterval::iterator I = J; I != E; ++I) {
    if (unsigned eq = EqClass[I->valno->id]) {
      assert((LIV[eq]->empty() || LIV[eq]->expiredAt(I->start)) &&
             "New intervals should be empty");
      LIV[eq]->ranges.push_back(*I);
    } else
      *J++ = *I;
  }
  LI.ranges.erase(J, E);

  // Transfer VNInfos to their new owners and renumber them.
  unsigned j = 0, e = LI.getNumValNums();
  while (j != e && EqClass[j] == 0)
    ++j;
  for (unsigned i = j; i != e; ++i) {
    VNInfo *VNI = LI.getValNumInfo(i);
    if (unsigned eq = EqClass[i]) {
      VNI->id = LIV[eq]->getNumValNums();
      LIV[eq]->valnos.push_back(VNI);
    } else {
      VNI->id = j;
      LI.valnos[j++] = VNI;
    }
  }
  LI.valnos.resize(j);
}
