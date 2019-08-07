//===-- llvm/CodeGen/MachineRegisterInfo.h ----------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the MachineRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_MACHINEREGISTERINFO_H
#define LLVM_CODEGEN_MACHINEREGISTERINFO_H

#include "llvm/Target/TargetRegisterInfo.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/IndexedMap.h"
#include <vector>

namespace llvm {
  
/// MachineRegisterInfo - Keep track of information for virtual and physical
/// registers, including vreg register classes, use/def chains for registers,
/// etc.
class MachineRegisterInfo {
  const TargetRegisterInfo *const TRI;

  /// IsSSA - True when the machine function is in SSA form and virtual
  /// registers have a single def.
  bool IsSSA;

  /// VRegInfo - Information we keep for each virtual register.
  ///
  /// Each element in this list contains the register class of the vreg and the
  /// start of the use/def list for the register.
  IndexedMap<std::pair<const TargetRegisterClass*, MachineOperand*>,
             VirtReg2IndexFunctor> VRegInfo;

  /// RegAllocHints - This vector records register allocation hints for virtual
  /// registers. For each virtual register, it keeps a register and hint type
  /// pair making up the allocation hint. Hint type is target specific except
  /// for the value 0 which means the second value of the pair is the preferred
  /// register for allocation. For example, if the hint is <0, 1024>, it means
  /// the allocator should prefer the physical register allocated to the virtual
  /// register of the hint.
  IndexedMap<std::pair<unsigned, unsigned>, VirtReg2IndexFunctor> RegAllocHints;
  
  /// PhysRegUseDefLists - This is an array of the head of the use/def list for
  /// physical registers.
  MachineOperand **PhysRegUseDefLists; 
  
  /// UsedPhysRegs - This is a bit vector that is computed and set by the
  /// register allocator, and must be kept up to date by passes that run after
  /// register allocation (though most don't modify this).  This is used
  /// so that the code generator knows which callee save registers to save and
  /// for other target specific uses.
  BitVector UsedPhysRegs;
  
  /// LiveIns/LiveOuts - Keep track of the physical registers that are
  /// livein/liveout of the function.  Live in values are typically arguments in
  /// registers, live out values are typically return values in registers.
  /// LiveIn values are allowed to have virtual registers associated with them,
  /// stored in the second element.
  std::vector<std::pair<unsigned, unsigned> > LiveIns;
  std::vector<unsigned> LiveOuts;
  
  MachineRegisterInfo(const MachineRegisterInfo&); // DO NOT IMPLEMENT
  void operator=(const MachineRegisterInfo&);      // DO NOT IMPLEMENT
public:
  explicit MachineRegisterInfo(const TargetRegisterInfo &TRI);
  ~MachineRegisterInfo();

  //===--------------------------------------------------------------------===//
  // Function State
  //===--------------------------------------------------------------------===//

  // isSSA - Returns true when the machine function is in SSA form. Early
  // passes require the machine function to be in SSA form where every virtual
  // register has a single defining instruction.
  //
  // The TwoAddressInstructionPass and PHIElimination passes take the machine
  // function out of SSA form when they introduce multiple defs per virtual
  // register.
  bool isSSA() const { return IsSSA; }

  // leaveSSA - Indicates that the machine function is no longer in SSA form.
  void leaveSSA() { IsSSA = false; }

  //===--------------------------------------------------------------------===//
  // Register Info
  //===--------------------------------------------------------------------===//

  /// reg_begin/reg_end - Provide iteration support to walk over all definitions
  /// and uses of a register within the MachineFunction that corresponds to this
  /// MachineRegisterInfo object.
  template<bool Uses, bool Defs, bool SkipDebug>
  class defusechain_iterator;

  /// reg_iterator/reg_begin/reg_end - Walk all defs and uses of the specified
  /// register.
  typedef defusechain_iterator<true,true,false> reg_iterator;
  reg_iterator reg_begin(unsigned RegNo) const {
    return reg_iterator(getRegUseDefListHead(RegNo));
  }
  static reg_iterator reg_end() { return reg_iterator(0); }

  /// reg_empty - Return true if there are no instructions using or defining the
  /// specified register (it may be live-in).
  bool reg_empty(unsigned RegNo) const { return reg_begin(RegNo) == reg_end(); }

  /// reg_nodbg_iterator/reg_nodbg_begin/reg_nodbg_end - Walk all defs and uses
  /// of the specified register, skipping those marked as Debug.
  typedef defusechain_iterator<true,true,true> reg_nodbg_iterator;
  reg_nodbg_iterator reg_nodbg_begin(unsigned RegNo) const {
    return reg_nodbg_iterator(getRegUseDefListHead(RegNo));
  }
  static reg_nodbg_iterator reg_nodbg_end() { return reg_nodbg_iterator(0); }

  /// reg_nodbg_empty - Return true if the only instructions using or defining
  /// Reg are Debug instructions.
  bool reg_nodbg_empty(unsigned RegNo) const {
    return reg_nodbg_begin(RegNo) == reg_nodbg_end();
  }

  /// def_iterator/def_begin/def_end - Walk all defs of the specified register.
  typedef defusechain_iterator<false,true,false> def_iterator;
  def_iterator def_begin(unsigned RegNo) const {
    return def_iterator(getRegUseDefListHead(RegNo));
  }
  static def_iterator def_end() { return def_iterator(0); }

  /// def_empty - Return true if there are no instructions defining the
  /// specified register (it may be live-in).
  bool def_empty(unsigned RegNo) const { return def_begin(RegNo) == def_end(); }

  /// use_iterator/use_begin/use_end - Walk all uses of the specified register.
  typedef defusechain_iterator<true,false,false> use_iterator;
  use_iterator use_begin(unsigned RegNo) const {
    return use_iterator(getRegUseDefListHead(RegNo));
  }
  static use_iterator use_end() { return use_iterator(0); }
  
  /// use_empty - Return true if there are no instructions using the specified
  /// register.
  bool use_empty(unsigned RegNo) const { return use_begin(RegNo) == use_end(); }

  /// hasOneUse - Return true if there is exactly one instruction using the
  /// specified register.
  bool hasOneUse(unsigned RegNo) const;

  /// use_nodbg_iterator/use_nodbg_begin/use_nodbg_end - Walk all uses of the
  /// specified register, skipping those marked as Debug.
  typedef defusechain_iterator<true,false,true> use_nodbg_iterator;
  use_nodbg_iterator use_nodbg_begin(unsigned RegNo) const {
    return use_nodbg_iterator(getRegUseDefListHead(RegNo));
  }
  static use_nodbg_iterator use_nodbg_end() { return use_nodbg_iterator(0); }
  
  /// use_nodbg_empty - Return true if there are no non-Debug instructions
  /// using the specified register.
  bool use_nodbg_empty(unsigned RegNo) const {
    return use_nodbg_begin(RegNo) == use_nodbg_end();
  }

  /// hasOneNonDBGUse - Return true if there is exactly one non-Debug
  /// instruction using the specified register.
  bool hasOneNonDBGUse(unsigned RegNo) const;

  /// replaceRegWith - Replace all instances of FromReg with ToReg in the
  /// machine function.  This is like llvm-level X->replaceAllUsesWith(Y),
  /// except that it also changes any definitions of the register as well.
  void replaceRegWith(unsigned FromReg, unsigned ToReg);
  
  /// getRegUseDefListHead - Return the head pointer for the register use/def
  /// list for the specified virtual or physical register.
  MachineOperand *&getRegUseDefListHead(unsigned RegNo) {
    if (TargetRegisterInfo::isVirtualRegister(RegNo))
      return VRegInfo[RegNo].second;
    return PhysRegUseDefLists[RegNo];
  }
  
  MachineOperand *getRegUseDefListHead(unsigned RegNo) const {
    if (TargetRegisterInfo::isVirtualRegister(RegNo))
      return VRegInfo[RegNo].second;
    return PhysRegUseDefLists[RegNo];
  }

  /// getVRegDef - Return the machine instr that defines the specified virtual
  /// register or null if none is found.  This assumes that the code is in SSA
  /// form, so there should only be one definition.
  MachineInstr *getVRegDef(unsigned Reg) const;

  /// clearKillFlags - Iterate over all the uses of the given register and
  /// clear the kill flag from the MachineOperand. This function is used by
  /// optimization passes which extend register lifetimes and need only
  /// preserve conservative kill flag information.
  void clearKillFlags(unsigned Reg) const;
  
#ifndef NDEBUG
  void dumpUses(unsigned RegNo) const;
#endif
  
  //===--------------------------------------------------------------------===//
  // Virtual Register Info
  //===--------------------------------------------------------------------===//
  
  /// getRegClass - Return the register class of the specified virtual register.
  ///
  const TargetRegisterClass *getRegClass(unsigned Reg) const {
    return VRegInfo[Reg].first;
  }

  /// setRegClass - Set the register class of the specified virtual register.
  ///
  void setRegClass(unsigned Reg, const TargetRegisterClass *RC);

  /// constrainRegClass - Constrain the register class of the specified virtual
  /// register to be a common subclass of RC and the current register class,
  /// but only if the new class has at least MinNumRegs registers.  Return the
  /// new register class, or NULL if no such class exists.
  /// This should only be used when the constraint is known to be trivial, like
  /// GR32 -> GR32_NOSP. Beware of increasing register pressure.
  ///
  const TargetRegisterClass *constrainRegClass(unsigned Reg,
                                               const TargetRegisterClass *RC,
                                               unsigned MinNumRegs = 0);

  /// recomputeRegClass - Try to find a legal super-class of Reg's register
  /// class that still satisfies the constraints from the instructions using
  /// Reg.  Returns true if Reg was upgraded.
  ///
  /// This method can be used after constraints have been removed from a
  /// virtual register, for example after removing instructions or splitting
  /// the live range.
  ///
  bool recomputeRegClass(unsigned Reg, const TargetMachine&);

  /// createVirtualRegister - Create and return a new virtual register in the
  /// function with the specified register class.
  ///
  unsigned createVirtualRegister(const TargetRegisterClass *RegClass);

  /// getNumVirtRegs - Return the number of virtual registers created.
  ///
  unsigned getNumVirtRegs() const { return VRegInfo.size(); }

  /// setRegAllocationHint - Specify a register allocation hint for the
  /// specified virtual register.
  void setRegAllocationHint(unsigned Reg, unsigned Type, unsigned PrefReg) {
    RegAllocHints[Reg].first  = Type;
    RegAllocHints[Reg].second = PrefReg;
  }

  /// getRegAllocationHint - Return the register allocation hint for the
  /// specified virtual register.
  std::pair<unsigned, unsigned>
  getRegAllocationHint(unsigned Reg) const {
    return RegAllocHints[Reg];
  }

  /// getSimpleHint - Return the preferred register allocation hint, or 0 if a
  /// standard simple hint (Type == 0) is not set.
  unsigned getSimpleHint(unsigned Reg) const {
    std::pair<unsigned, unsigned> Hint = getRegAllocationHint(Reg);
    return Hint.first ? 0 : Hint.second;
  }


  //===--------------------------------------------------------------------===//
  // Physical Register Use Info
  //===--------------------------------------------------------------------===//
  
  /// isPhysRegUsed - Return true if the specified register is used in this
  /// function.  This only works after register allocation.
  bool isPhysRegUsed(unsigned Reg) const { return UsedPhysRegs[Reg]; }
  
  /// setPhysRegUsed - Mark the specified register used in this function.
  /// This should only be called during and after register allocation.
  void setPhysRegUsed(unsigned Reg) { UsedPhysRegs[Reg] = true; }

  /// addPhysRegsUsed - Mark the specified registers used in this function.
  /// This should only be called during and after register allocation.
  void addPhysRegsUsed(const BitVector &Regs) { UsedPhysRegs |= Regs; }

  /// setPhysRegUnused - Mark the specified register unused in this function.
  /// This should only be called during and after register allocation.
  void setPhysRegUnused(unsigned Reg) { UsedPhysRegs[Reg] = false; }

  /// closePhysRegsUsed - Expand UsedPhysRegs to its transitive closure over
  /// subregisters. That means that if R is used, so are all subregisters.
  void closePhysRegsUsed(const TargetRegisterInfo&);

  //===--------------------------------------------------------------------===//
  // LiveIn/LiveOut Management
  //===--------------------------------------------------------------------===//
  
  /// addLiveIn/Out - Add the specified register as a live in/out.  Note that it
  /// is an error to add the same register to the same set more than once.
  void addLiveIn(unsigned Reg, unsigned vreg = 0) {
    LiveIns.push_back(std::make_pair(Reg, vreg));
  }
  void addLiveOut(unsigned Reg) { LiveOuts.push_back(Reg); }
  
  // Iteration support for live in/out sets.  These sets are kept in sorted
  // order by their register number.
  typedef std::vector<std::pair<unsigned,unsigned> >::const_iterator
  livein_iterator;
  typedef std::vector<unsigned>::const_iterator liveout_iterator;
  livein_iterator livein_begin() const { return LiveIns.begin(); }
  livein_iterator livein_end()   const { return LiveIns.end(); }
  bool            livein_empty() const { return LiveIns.empty(); }
  liveout_iterator liveout_begin() const { return LiveOuts.begin(); }
  liveout_iterator liveout_end()   const { return LiveOuts.end(); }
  bool             liveout_empty() const { return LiveOuts.empty(); }

  bool isLiveIn(unsigned Reg) const;
  bool isLiveOut(unsigned Reg) const;

  /// getLiveInPhysReg - If VReg is a live-in virtual register, return the
  /// corresponding live-in physical register.
  unsigned getLiveInPhysReg(unsigned VReg) const;

  /// getLiveInVirtReg - If PReg is a live-in physical register, return the
  /// corresponding live-in physical register.
  unsigned getLiveInVirtReg(unsigned PReg) const;

  /// EmitLiveInCopies - Emit copies to initialize livein virtual registers
  /// into the given entry block.
  void EmitLiveInCopies(MachineBasicBlock *EntryMBB,
                        const TargetRegisterInfo &TRI,
                        const TargetInstrInfo &TII);

private:
  void HandleVRegListReallocation();
  
public:
  /// defusechain_iterator - This class provides iterator support for machine
  /// operands in the function that use or define a specific register.  If
  /// ReturnUses is true it returns uses of registers, if ReturnDefs is true it
  /// returns defs.  If neither are true then you are silly and it always
  /// returns end().  If SkipDebug is true it skips uses marked Debug
  /// when incrementing.
  template<bool ReturnUses, bool ReturnDefs, bool SkipDebug>
  class defusechain_iterator
    : public std::iterator<std::forward_iterator_tag, MachineInstr, ptrdiff_t> {
    MachineOperand *Op;
    explicit defusechain_iterator(MachineOperand *op) : Op(op) {
      // If the first node isn't one we're interested in, advance to one that
      // we are interested in.
      if (op) {
        if ((!ReturnUses && op->isUse()) ||
            (!ReturnDefs && op->isDef()) ||
            (SkipDebug && op->isDebug()))
          ++*this;
      }
    }
    friend class MachineRegisterInfo;
  public:
    typedef std::iterator<std::forward_iterator_tag,
                          MachineInstr, ptrdiff_t>::reference reference;
    typedef std::iterator<std::forward_iterator_tag,
                          MachineInstr, ptrdiff_t>::pointer pointer;
    
    defusechain_iterator(const defusechain_iterator &I) : Op(I.Op) {}
    defusechain_iterator() : Op(0) {}
    
    bool operator==(const defusechain_iterator &x) const {
      return Op == x.Op;
    }
    bool operator!=(const defusechain_iterator &x) const {
      return !operator==(x);
    }
    
    /// atEnd - return true if this iterator is equal to reg_end() on the value.
    bool atEnd() const { return Op == 0; }
    
    // Iterator traversal: forward iteration only
    defusechain_iterator &operator++() {          // Preincrement
      assert(Op && "Cannot increment end iterator!");
      Op = Op->getNextOperandForReg();
      
      // If this is an operand we don't care about, skip it.
      while (Op && ((!ReturnUses && Op->isUse()) || 
                    (!ReturnDefs && Op->isDef()) ||
                    (SkipDebug && Op->isDebug())))
        Op = Op->getNextOperandForReg();
      
      return *this;
    }
    defusechain_iterator operator++(int) {        // Postincrement
      defusechain_iterator tmp = *this; ++*this; return tmp;
    }

    /// skipInstruction - move forward until reaching a different instruction.
    /// Return the skipped instruction that is no longer pointed to, or NULL if
    /// already pointing to end().
    MachineInstr *skipInstruction() {
      if (!Op) return 0;
      MachineInstr *MI = Op->getParent();
      do ++*this;
      while (Op && Op->getParent() == MI);
      return MI;
    }

    MachineOperand &getOperand() const {
      assert(Op && "Cannot dereference end iterator!");
      return *Op;
    }
    
    /// getOperandNo - Return the operand # of this MachineOperand in its
    /// MachineInstr.
    unsigned getOperandNo() const {
      assert(Op && "Cannot dereference end iterator!");
      return Op - &Op->getParent()->getOperand(0);
    }
    
    // Retrieve a reference to the current operand.
    MachineInstr &operator*() const {
      assert(Op && "Cannot dereference end iterator!");
      return *Op->getParent();
    }
    
    MachineInstr *operator->() const {
      assert(Op && "Cannot dereference end iterator!");
      return Op->getParent();
    }
  };
  
};

} // End llvm namespace

#endif
