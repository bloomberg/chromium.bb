//=== Target/TargetRegisterInfo.h - Target Register Information -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file describes an abstract interface used to get information about a
// target machines register file.  This information is used for a variety of
// purposed, especially register allocation.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TARGET_TARGETREGISTERINFO_H
#define LLVM_TARGET_TARGETREGISTERINFO_H

#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/ADT/ArrayRef.h"
#include <cassert>
#include <functional>

namespace llvm {

class BitVector;
class MachineFunction;
class RegScavenger;
template<class T> class SmallVectorImpl;
class raw_ostream;

class TargetRegisterClass {
public:
  typedef const unsigned* iterator;
  typedef const unsigned* const_iterator;
  typedef const EVT* vt_iterator;
  typedef const TargetRegisterClass* const * sc_iterator;
private:
  const MCRegisterClass *MC;
  const vt_iterator VTs;
  const unsigned *SubClassMask;
  const sc_iterator SuperClasses;
  const sc_iterator SuperRegClasses;
public:
  TargetRegisterClass(const MCRegisterClass *MC, const EVT *vts,
                      const unsigned *subcm,
                      const TargetRegisterClass * const *supcs,
                      const TargetRegisterClass * const *superregcs)
    : MC(MC), VTs(vts), SubClassMask(subcm), SuperClasses(supcs),
      SuperRegClasses(superregcs) {}

  virtual ~TargetRegisterClass() {}     // Allow subclasses

  /// getID() - Return the register class ID number.
  ///
  unsigned getID() const { return MC->getID(); }

  /// getName() - Return the register class name for debugging.
  ///
  const char *getName() const { return MC->getName(); }

  /// begin/end - Return all of the registers in this class.
  ///
  iterator       begin() const { return MC->begin(); }
  iterator         end() const { return MC->end(); }

  /// getNumRegs - Return the number of registers in this class.
  ///
  unsigned getNumRegs() const { return MC->getNumRegs(); }

  /// getRegister - Return the specified register in the class.
  ///
  unsigned getRegister(unsigned i) const {
    return MC->getRegister(i);
  }

  /// contains - Return true if the specified register is included in this
  /// register class.  This does not include virtual registers.
  bool contains(unsigned Reg) const {
    return MC->contains(Reg);
  }

  /// contains - Return true if both registers are in this class.
  bool contains(unsigned Reg1, unsigned Reg2) const {
    return MC->contains(Reg1, Reg2);
  }

  /// getSize - Return the size of the register in bytes, which is also the size
  /// of a stack slot allocated to hold a spilled copy of this register.
  unsigned getSize() const { return MC->getSize(); }

  /// getAlignment - Return the minimum required alignment for a register of
  /// this class.
  unsigned getAlignment() const { return MC->getAlignment(); }

  /// getCopyCost - Return the cost of copying a value between two registers in
  /// this class. A negative number means the register class is very expensive
  /// to copy e.g. status flag register classes.
  int getCopyCost() const { return MC->getCopyCost(); }

  /// isAllocatable - Return true if this register class may be used to create
  /// virtual registers.
  bool isAllocatable() const { return MC->isAllocatable(); }

  /// hasType - return true if this TargetRegisterClass has the ValueType vt.
  ///
  bool hasType(EVT vt) const {
    for(int i = 0; VTs[i] != MVT::Other; ++i)
      if (VTs[i] == vt)
        return true;
    return false;
  }

  /// vt_begin / vt_end - Loop over all of the value types that can be
  /// represented by values in this register class.
  vt_iterator vt_begin() const {
    return VTs;
  }

  vt_iterator vt_end() const {
    vt_iterator I = VTs;
    while (*I != MVT::Other) ++I;
    return I;
  }

  /// superregclasses_begin / superregclasses_end - Loop over all of
  /// the superreg register classes of this register class.
  sc_iterator superregclasses_begin() const {
    return SuperRegClasses;
  }

  sc_iterator superregclasses_end() const {
    sc_iterator I = SuperRegClasses;
    while (*I != NULL) ++I;
    return I;
  }

  /// hasSubClass - return true if the specified TargetRegisterClass
  /// is a proper sub-class of this TargetRegisterClass.
  bool hasSubClass(const TargetRegisterClass *RC) const {
    return RC != this && hasSubClassEq(RC);
  }

  /// hasSubClassEq - Returns true if RC is a sub-class of or equal to this
  /// class.
  bool hasSubClassEq(const TargetRegisterClass *RC) const {
    unsigned ID = RC->getID();
    return (SubClassMask[ID / 32] >> (ID % 32)) & 1;
  }

  /// hasSuperClass - return true if the specified TargetRegisterClass is a
  /// proper super-class of this TargetRegisterClass.
  bool hasSuperClass(const TargetRegisterClass *RC) const {
    return RC->hasSubClass(this);
  }

  /// hasSuperClassEq - Returns true if RC is a super-class of or equal to this
  /// class.
  bool hasSuperClassEq(const TargetRegisterClass *RC) const {
    return RC->hasSubClassEq(this);
  }

  /// getSubClassMask - Returns a bit vector of subclasses, including this one.
  /// The vector is indexed by class IDs, see hasSubClassEq() above for how to
  /// use it.
  const unsigned *getSubClassMask() const {
    return SubClassMask;
  }

  /// getSuperClasses - Returns a NULL terminated list of super-classes.  The
  /// classes are ordered by ID which is also a topological ordering from large
  /// to small classes.  The list does NOT include the current class.
  sc_iterator getSuperClasses() const {
    return SuperClasses;
  }

  /// isASubClass - return true if this TargetRegisterClass is a subset
  /// class of at least one other TargetRegisterClass.
  bool isASubClass() const {
    return SuperClasses[0] != 0;
  }

  /// getRawAllocationOrder - Returns the preferred order for allocating
  /// registers from this register class in MF. The raw order comes directly
  /// from the .td file and may include reserved registers that are not
  /// allocatable. Register allocators should also make sure to allocate
  /// callee-saved registers only after all the volatiles are used. The
  /// RegisterClassInfo class provides filtered allocation orders with
  /// callee-saved registers moved to the end.
  ///
  /// The MachineFunction argument can be used to tune the allocatable
  /// registers based on the characteristics of the function, subtarget, or
  /// other criteria.
  ///
  /// By default, this method returns all registers in the class.
  ///
  virtual
  ArrayRef<unsigned> getRawAllocationOrder(const MachineFunction &MF) const {
    return makeArrayRef(begin(), getNumRegs());
  }
};

/// TargetRegisterInfoDesc - Extra information, not in MCRegisterDesc, about
/// registers. These are used by codegen, not by MC.
struct TargetRegisterInfoDesc {
  unsigned CostPerUse;          // Extra cost of instructions using register.
  bool inAllocatableClass;      // Register belongs to an allocatable regclass.
};

/// TargetRegisterInfo base class - We assume that the target defines a static
/// array of TargetRegisterDesc objects that represent all of the machine
/// registers that the target has.  As such, we simply have to track a pointer
/// to this array so that we can turn register number into a register
/// descriptor.
///
class TargetRegisterInfo : public MCRegisterInfo {
public:
  typedef const TargetRegisterClass * const * regclass_iterator;
private:
  const TargetRegisterInfoDesc *InfoDesc;     // Extra desc array for codegen
  const char *const *SubRegIndexNames;        // Names of subreg indexes.
  regclass_iterator RegClassBegin, RegClassEnd;   // List of regclasses

protected:
  TargetRegisterInfo(const TargetRegisterInfoDesc *ID,
                     regclass_iterator RegClassBegin,
                     regclass_iterator RegClassEnd,
                     const char *const *subregindexnames);
  virtual ~TargetRegisterInfo();
public:

  // Register numbers can represent physical registers, virtual registers, and
  // sometimes stack slots. The unsigned values are divided into these ranges:
  //
  //   0           Not a register, can be used as a sentinel.
  //   [1;2^30)    Physical registers assigned by TableGen.
  //   [2^30;2^31) Stack slots. (Rarely used.)
  //   [2^31;2^32) Virtual registers assigned by MachineRegisterInfo.
  //
  // Further sentinels can be allocated from the small negative integers.
  // DenseMapInfo<unsigned> uses -1u and -2u.

  /// isStackSlot - Sometimes it is useful the be able to store a non-negative
  /// frame index in a variable that normally holds a register. isStackSlot()
  /// returns true if Reg is in the range used for stack slots.
  ///
  /// Note that isVirtualRegister() and isPhysicalRegister() cannot handle stack
  /// slots, so if a variable may contains a stack slot, always check
  /// isStackSlot() first.
  ///
  static bool isStackSlot(unsigned Reg) {
    return int(Reg) >= (1 << 30);
  }

  /// stackSlot2Index - Compute the frame index from a register value
  /// representing a stack slot.
  static int stackSlot2Index(unsigned Reg) {
    assert(isStackSlot(Reg) && "Not a stack slot");
    return int(Reg - (1u << 30));
  }

  /// index2StackSlot - Convert a non-negative frame index to a stack slot
  /// register value.
  static unsigned index2StackSlot(int FI) {
    assert(FI >= 0 && "Cannot hold a negative frame index.");
    return FI + (1u << 30);
  }

  /// isPhysicalRegister - Return true if the specified register number is in
  /// the physical register namespace.
  static bool isPhysicalRegister(unsigned Reg) {
    assert(!isStackSlot(Reg) && "Not a register! Check isStackSlot() first.");
    return int(Reg) > 0;
  }

  /// isVirtualRegister - Return true if the specified register number is in
  /// the virtual register namespace.
  static bool isVirtualRegister(unsigned Reg) {
    assert(!isStackSlot(Reg) && "Not a register! Check isStackSlot() first.");
    return int(Reg) < 0;
  }

  /// virtReg2Index - Convert a virtual register number to a 0-based index.
  /// The first virtual register in a function will get the index 0.
  static unsigned virtReg2Index(unsigned Reg) {
    assert(isVirtualRegister(Reg) && "Not a virtual register");
    return Reg & ~(1u << 31);
  }

  /// index2VirtReg - Convert a 0-based index to a virtual register number.
  /// This is the inverse operation of VirtReg2IndexFunctor below.
  static unsigned index2VirtReg(unsigned Index) {
    return Index | (1u << 31);
  }

  /// getMinimalPhysRegClass - Returns the Register Class of a physical
  /// register of the given type, picking the most sub register class of
  /// the right type that contains this physreg.
  const TargetRegisterClass *
    getMinimalPhysRegClass(unsigned Reg, EVT VT = MVT::Other) const;

  /// getAllocatableSet - Returns a bitset indexed by register number
  /// indicating if a register is allocatable or not. If a register class is
  /// specified, returns the subset for the class.
  BitVector getAllocatableSet(const MachineFunction &MF,
                              const TargetRegisterClass *RC = NULL) const;

  /// getCostPerUse - Return the additional cost of using this register instead
  /// of other registers in its class.
  unsigned getCostPerUse(unsigned RegNo) const {
    return InfoDesc[RegNo].CostPerUse;
  }

  /// isInAllocatableClass - Return true if the register is in the allocation
  /// of any register class.
  bool isInAllocatableClass(unsigned RegNo) const {
    return InfoDesc[RegNo].inAllocatableClass;
  }

  /// getSubRegIndexName - Return the human-readable symbolic target-specific
  /// name for the specified SubRegIndex.
  const char *getSubRegIndexName(unsigned SubIdx) const {
    assert(SubIdx && "This is not a subregister index");
    return SubRegIndexNames[SubIdx-1];
  }

  /// regsOverlap - Returns true if the two registers are equal or alias each
  /// other. The registers may be virtual register.
  bool regsOverlap(unsigned regA, unsigned regB) const {
    if (regA == regB) return true;
    if (isVirtualRegister(regA) || isVirtualRegister(regB))
      return false;
    for (const unsigned *regList = getOverlaps(regA)+1; *regList; ++regList) {
      if (*regList == regB) return true;
    }
    return false;
  }

  /// isSubRegister - Returns true if regB is a sub-register of regA.
  ///
  bool isSubRegister(unsigned regA, unsigned regB) const {
    return isSuperRegister(regB, regA);
  }

  /// isSuperRegister - Returns true if regB is a super-register of regA.
  ///
  bool isSuperRegister(unsigned regA, unsigned regB) const {
    for (const unsigned *regList = getSuperRegisters(regA); *regList;++regList){
      if (*regList == regB) return true;
    }
    return false;
  }

  /// getCalleeSavedRegs - Return a null-terminated list of all of the
  /// callee saved registers on this target. The register should be in the
  /// order of desired callee-save stack frame offset. The first register is
  /// closed to the incoming stack pointer if stack grows down, and vice versa.
  virtual const unsigned* getCalleeSavedRegs(const MachineFunction *MF = 0)
                                                                      const = 0;


  /// getReservedRegs - Returns a bitset indexed by physical register number
  /// indicating if a register is a special register that has particular uses
  /// and should be considered unavailable at all times, e.g. SP, RA. This is
  /// used by register scavenger to determine what registers are free.
  virtual BitVector getReservedRegs(const MachineFunction &MF) const = 0;

  /// getSubReg - Returns the physical register number of sub-register "Index"
  /// for physical register RegNo. Return zero if the sub-register does not
  /// exist.
  virtual unsigned getSubReg(unsigned RegNo, unsigned Index) const = 0;

  /// getSubRegIndex - For a given register pair, return the sub-register index
  /// if the second register is a sub-register of the first. Return zero
  /// otherwise.
  virtual unsigned getSubRegIndex(unsigned RegNo, unsigned SubRegNo) const = 0;

  /// getMatchingSuperReg - Return a super-register of the specified register
  /// Reg so its sub-register of index SubIdx is Reg.
  unsigned getMatchingSuperReg(unsigned Reg, unsigned SubIdx,
                               const TargetRegisterClass *RC) const {
    for (const unsigned *SRs = getSuperRegisters(Reg); unsigned SR = *SRs;++SRs)
      if (Reg == getSubReg(SR, SubIdx) && RC->contains(SR))
        return SR;
    return 0;
  }

  /// canCombineSubRegIndices - Given a register class and a list of
  /// subregister indices, return true if it's possible to combine the
  /// subregister indices into one that corresponds to a larger
  /// subregister. Return the new subregister index by reference. Note the
  /// new index may be zero if the given subregisters can be combined to
  /// form the whole register.
  virtual bool canCombineSubRegIndices(const TargetRegisterClass *RC,
                                       SmallVectorImpl<unsigned> &SubIndices,
                                       unsigned &NewSubIdx) const {
    return 0;
  }

  /// getMatchingSuperRegClass - Return a subclass of the specified register
  /// class A so that each register in it has a sub-register of the
  /// specified sub-register index which is in the specified register class B.
  virtual const TargetRegisterClass *
  getMatchingSuperRegClass(const TargetRegisterClass *A,
                           const TargetRegisterClass *B, unsigned Idx) const {
    return 0;
  }

  /// getSubClassWithSubReg - Returns the largest legal sub-class of RC that
  /// supports the sub-register index Idx.
  /// If no such sub-class exists, return NULL.
  /// If all registers in RC already have an Idx sub-register, return RC.
  ///
  /// TableGen generates a version of this function that is good enough in most
  /// cases.  Targets can override if they have constraints that TableGen
  /// doesn't understand.  For example, the x86 sub_8bit sub-register index is
  /// supported by the full GR32 register class in 64-bit mode, but only by the
  /// GR32_ABCD regiister class in 32-bit mode.
  ///
  virtual const TargetRegisterClass *
  getSubClassWithSubReg(const TargetRegisterClass *RC, unsigned Idx) const =0;

  /// composeSubRegIndices - Return the subregister index you get from composing
  /// two subregister indices.
  ///
  /// If R:a:b is the same register as R:c, then composeSubRegIndices(a, b)
  /// returns c. Note that composeSubRegIndices does not tell you about illegal
  /// compositions. If R does not have a subreg a, or R:a does not have a subreg
  /// b, composeSubRegIndices doesn't tell you.
  ///
  /// The ARM register Q0 has two D subregs dsub_0:D0 and dsub_1:D1. It also has
  /// ssub_0:S0 - ssub_3:S3 subregs.
  /// If you compose subreg indices dsub_1, ssub_0 you get ssub_2.
  ///
  virtual unsigned composeSubRegIndices(unsigned a, unsigned b) const {
    // This default implementation is correct for most targets.
    return b;
  }

  //===--------------------------------------------------------------------===//
  // Register Class Information
  //

  /// Register class iterators
  ///
  regclass_iterator regclass_begin() const { return RegClassBegin; }
  regclass_iterator regclass_end() const { return RegClassEnd; }

  unsigned getNumRegClasses() const {
    return (unsigned)(regclass_end()-regclass_begin());
  }

  /// getRegClass - Returns the register class associated with the enumeration
  /// value.  See class MCOperandInfo.
  const TargetRegisterClass *getRegClass(unsigned i) const {
    assert(i < getNumRegClasses() && "Register Class ID out of range");
    return RegClassBegin[i];
  }

  /// getCommonSubClass - find the largest common subclass of A and B. Return
  /// NULL if there is no common subclass.
  const TargetRegisterClass *
  getCommonSubClass(const TargetRegisterClass *A,
                    const TargetRegisterClass *B) const;

  /// getPointerRegClass - Returns a TargetRegisterClass used for pointer
  /// values.  If a target supports multiple different pointer register classes,
  /// kind specifies which one is indicated.
  virtual const TargetRegisterClass *getPointerRegClass(unsigned Kind=0) const {
    assert(0 && "Target didn't implement getPointerRegClass!");
    return 0; // Must return a value in order to compile with VS 2005
  }

  /// getCrossCopyRegClass - Returns a legal register class to copy a register
  /// in the specified class to or from. If it is possible to copy the register
  /// directly without using a cross register class copy, return the specified
  /// RC. Returns NULL if it is not possible to copy between a two registers of
  /// the specified class.
  virtual const TargetRegisterClass *
  getCrossCopyRegClass(const TargetRegisterClass *RC) const {
    return RC;
  }

  /// getLargestLegalSuperClass - Returns the largest super class of RC that is
  /// legal to use in the current sub-target and has the same spill size.
  /// The returned register class can be used to create virtual registers which
  /// means that all its registers can be copied and spilled.
  virtual const TargetRegisterClass*
  getLargestLegalSuperClass(const TargetRegisterClass *RC) const {
    /// The default implementation is very conservative and doesn't allow the
    /// register allocator to inflate register classes.
    return RC;
  }

  /// getRegPressureLimit - Return the register pressure "high water mark" for
  /// the specific register class. The scheduler is in high register pressure
  /// mode (for the specific register class) if it goes over the limit.
  virtual unsigned getRegPressureLimit(const TargetRegisterClass *RC,
                                       MachineFunction &MF) const {
    return 0;
  }

  /// getRawAllocationOrder - Returns the register allocation order for a
  /// specified register class with a target-dependent hint. The returned list
  /// may contain reserved registers that cannot be allocated.
  ///
  /// Register allocators need only call this function to resolve
  /// target-dependent hints, but it should work without hinting as well.
  virtual ArrayRef<unsigned>
  getRawAllocationOrder(const TargetRegisterClass *RC,
                        unsigned HintType, unsigned HintReg,
                        const MachineFunction &MF) const {
    return RC->getRawAllocationOrder(MF);
  }

  /// ResolveRegAllocHint - Resolves the specified register allocation hint
  /// to a physical register. Returns the physical register if it is successful.
  virtual unsigned ResolveRegAllocHint(unsigned Type, unsigned Reg,
                                       const MachineFunction &MF) const {
    if (Type == 0 && Reg && isPhysicalRegister(Reg))
      return Reg;
    return 0;
  }

  /// avoidWriteAfterWrite - Return true if the register allocator should avoid
  /// writing a register from RC in two consecutive instructions.
  /// This can avoid pipeline stalls on certain architectures.
  /// It does cause increased register pressure, though.
  virtual bool avoidWriteAfterWrite(const TargetRegisterClass *RC) const {
    return false;
  }

  /// UpdateRegAllocHint - A callback to allow target a chance to update
  /// register allocation hints when a register is "changed" (e.g. coalesced)
  /// to another register. e.g. On ARM, some virtual registers should target
  /// register pairs, if one of pair is coalesced to another register, the
  /// allocation hint of the other half of the pair should be changed to point
  /// to the new register.
  virtual void UpdateRegAllocHint(unsigned Reg, unsigned NewReg,
                                  MachineFunction &MF) const {
    // Do nothing.
  }

  /// requiresRegisterScavenging - returns true if the target requires (and can
  /// make use of) the register scavenger.
  virtual bool requiresRegisterScavenging(const MachineFunction &MF) const {
    return false;
  }

  /// useFPForScavengingIndex - returns true if the target wants to use
  /// frame pointer based accesses to spill to the scavenger emergency spill
  /// slot.
  virtual bool useFPForScavengingIndex(const MachineFunction &MF) const {
    return true;
  }

  /// requiresFrameIndexScavenging - returns true if the target requires post
  /// PEI scavenging of registers for materializing frame index constants.
  virtual bool requiresFrameIndexScavenging(const MachineFunction &MF) const {
    return false;
  }

  /// requiresVirtualBaseRegisters - Returns true if the target wants the
  /// LocalStackAllocation pass to be run and virtual base registers
  /// used for more efficient stack access.
  virtual bool requiresVirtualBaseRegisters(const MachineFunction &MF) const {
    return false;
  }

  /// hasReservedSpillSlot - Return true if target has reserved a spill slot in
  /// the stack frame of the given function for the specified register. e.g. On
  /// x86, if the frame register is required, the first fixed stack object is
  /// reserved as its spill slot. This tells PEI not to create a new stack frame
  /// object for the given register. It should be called only after
  /// processFunctionBeforeCalleeSavedScan().
  virtual bool hasReservedSpillSlot(const MachineFunction &MF, unsigned Reg,
                                    int &FrameIdx) const {
    return false;
  }

  /// needsStackRealignment - true if storage within the function requires the
  /// stack pointer to be aligned more than the normal calling convention calls
  /// for.
  virtual bool needsStackRealignment(const MachineFunction &MF) const {
    return false;
  }

  /// getFrameIndexInstrOffset - Get the offset from the referenced frame
  /// index in the instruction, if there is one.
  virtual int64_t getFrameIndexInstrOffset(const MachineInstr *MI,
                                           int Idx) const {
    return 0;
  }

  /// needsFrameBaseReg - Returns true if the instruction's frame index
  /// reference would be better served by a base register other than FP
  /// or SP. Used by LocalStackFrameAllocation to determine which frame index
  /// references it should create new base registers for.
  virtual bool needsFrameBaseReg(MachineInstr *MI, int64_t Offset) const {
    return false;
  }

  /// materializeFrameBaseRegister - Insert defining instruction(s) for
  /// BaseReg to be a pointer to FrameIdx before insertion point I.
  virtual void materializeFrameBaseRegister(MachineBasicBlock *MBB,
                                            unsigned BaseReg, int FrameIdx,
                                            int64_t Offset) const {
    assert(0 && "materializeFrameBaseRegister does not exist on this target");
  }

  /// resolveFrameIndex - Resolve a frame index operand of an instruction
  /// to reference the indicated base register plus offset instead.
  virtual void resolveFrameIndex(MachineBasicBlock::iterator I,
                                 unsigned BaseReg, int64_t Offset) const {
    assert(0 && "resolveFrameIndex does not exist on this target");
  }

  /// isFrameOffsetLegal - Determine whether a given offset immediate is
  /// encodable to resolve a frame index.
  virtual bool isFrameOffsetLegal(const MachineInstr *MI,
                                  int64_t Offset) const {
    assert(0 && "isFrameOffsetLegal does not exist on this target");
    return false; // Must return a value in order to compile with VS 2005
  }

  /// eliminateCallFramePseudoInstr - This method is called during prolog/epilog
  /// code insertion to eliminate call frame setup and destroy pseudo
  /// instructions (but only if the Target is using them).  It is responsible
  /// for eliminating these instructions, replacing them with concrete
  /// instructions.  This method need only be implemented if using call frame
  /// setup/destroy pseudo instructions.
  ///
  virtual void
  eliminateCallFramePseudoInstr(MachineFunction &MF,
                                MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MI) const {
    assert(0 && "Call Frame Pseudo Instructions do not exist on this target!");
  }


  /// saveScavengerRegister - Spill the register so it can be used by the
  /// register scavenger. Return true if the register was spilled, false
  /// otherwise. If this function does not spill the register, the scavenger
  /// will instead spill it to the emergency spill slot.
  ///
  virtual bool saveScavengerRegister(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator I,
                                     MachineBasicBlock::iterator &UseMI,
                                     const TargetRegisterClass *RC,
                                     unsigned Reg) const {
    return false;
  }

  /// eliminateFrameIndex - This method must be overriden to eliminate abstract
  /// frame indices from instructions which may use them.  The instruction
  /// referenced by the iterator contains an MO_FrameIndex operand which must be
  /// eliminated by this method.  This method may modify or replace the
  /// specified instruction, as long as it keeps the iterator pointing at the
  /// finished product. SPAdj is the SP adjustment due to call frame setup
  /// instruction.
  virtual void eliminateFrameIndex(MachineBasicBlock::iterator MI,
                                   int SPAdj, RegScavenger *RS=NULL) const = 0;

  //===--------------------------------------------------------------------===//
  /// Debug information queries.

  /// getFrameRegister - This method should return the register used as a base
  /// for values allocated in the current stack frame.
  virtual unsigned getFrameRegister(const MachineFunction &MF) const = 0;

  /// getCompactUnwindRegNum - This function maps the register to the number for
  /// compact unwind encoding. Return -1 if the register isn't valid.
  virtual int getCompactUnwindRegNum(unsigned, bool) const {
    return -1;
  }
};


// This is useful when building IndexedMaps keyed on virtual registers
struct VirtReg2IndexFunctor : public std::unary_function<unsigned, unsigned> {
  unsigned operator()(unsigned Reg) const {
    return TargetRegisterInfo::virtReg2Index(Reg);
  }
};

/// PrintReg - Helper class for printing registers on a raw_ostream.
/// Prints virtual and physical registers with or without a TRI instance.
///
/// The format is:
///   %noreg          - NoRegister
///   %vreg5          - a virtual register.
///   %vreg5:sub_8bit - a virtual register with sub-register index (with TRI).
///   %EAX            - a physical register
///   %physreg17      - a physical register when no TRI instance given.
///
/// Usage: OS << PrintReg(Reg, TRI) << '\n';
///
class PrintReg {
  const TargetRegisterInfo *TRI;
  unsigned Reg;
  unsigned SubIdx;
public:
  PrintReg(unsigned reg, const TargetRegisterInfo *tri = 0, unsigned subidx = 0)
    : TRI(tri), Reg(reg), SubIdx(subidx) {}
  void print(raw_ostream&) const;
};

static inline raw_ostream &operator<<(raw_ostream &OS, const PrintReg &PR) {
  PR.print(OS);
  return OS;
}

} // End llvm namespace

#endif
