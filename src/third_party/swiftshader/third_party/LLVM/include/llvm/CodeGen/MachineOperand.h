//===-- llvm/CodeGen/MachineOperand.h - MachineOperand class ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the MachineOperand class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_MACHINEOPERAND_H
#define LLVM_CODEGEN_MACHINEOPERAND_H

#include "llvm/Support/DataTypes.h"
#include <cassert>

namespace llvm {

class BlockAddress;
class ConstantFP;
class ConstantInt;
class GlobalValue;
class MachineBasicBlock;
class MachineInstr;
class MachineRegisterInfo;
class MDNode;
class TargetMachine;
class TargetRegisterInfo;
class raw_ostream;
class MCSymbol;

/// MachineOperand class - Representation of each machine instruction operand.
///
class MachineOperand {
public:
  enum MachineOperandType {
    MO_Register,               ///< Register operand.
    MO_Immediate,              ///< Immediate operand
    MO_CImmediate,             ///< Immediate >64bit operand
    MO_FPImmediate,            ///< Floating-point immediate operand
    MO_MachineBasicBlock,      ///< MachineBasicBlock reference
    MO_FrameIndex,             ///< Abstract Stack Frame Index
    MO_ConstantPoolIndex,      ///< Address of indexed Constant in Constant Pool
    MO_JumpTableIndex,         ///< Address of indexed Jump Table for switch
    MO_ExternalSymbol,         ///< Name of external global symbol
    MO_GlobalAddress,          ///< Address of a global value
    MO_BlockAddress,           ///< Address of a basic block
    MO_Metadata,               ///< Metadata reference (for debug info)
    MO_MCSymbol                ///< MCSymbol reference (for debug/eh info)
  };

private:
  /// OpKind - Specify what kind of operand this is.  This discriminates the
  /// union.
  unsigned char OpKind; // MachineOperandType

  /// SubReg - Subregister number, only valid for MO_Register.  A value of 0
  /// indicates the MO_Register has no subReg.
  unsigned char SubReg;

  /// TargetFlags - This is a set of target-specific operand flags.
  unsigned char TargetFlags;

  /// IsDef/IsImp/IsKill/IsDead flags - These are only valid for MO_Register
  /// operands.

  /// IsDef - True if this is a def, false if this is a use of the register.
  ///
  bool IsDef : 1;

  /// IsImp - True if this is an implicit def or use, false if it is explicit.
  ///
  bool IsImp : 1;

  /// IsKill - True if this instruction is the last use of the register on this
  /// path through the function.  This is only valid on uses of registers.
  bool IsKill : 1;

  /// IsDead - True if this register is never used by a subsequent instruction.
  /// This is only valid on definitions of registers.
  bool IsDead : 1;

  /// IsUndef - True if this register operand reads an "undef" value, i.e. the
  /// read value doesn't matter.  This flag can be set on both use and def
  /// operands.  On a sub-register def operand, it refers to the part of the
  /// register that isn't written.  On a full-register def operand, it is a
  /// noop.  See readsReg().
  ///
  /// This is only valid on registers.
  ///
  /// Note that an instruction may have multiple <undef> operands referring to
  /// the same register.  In that case, the instruction may depend on those
  /// operands reading the same dont-care value.  For example:
  ///
  ///   %vreg1<def> = XOR %vreg2<undef>, %vreg2<undef>
  ///
  /// Any register can be used for %vreg2, and its value doesn't matter, but
  /// the two operands must be the same register.
  ///
  bool IsUndef : 1;

  /// IsEarlyClobber - True if this MO_Register 'def' operand is written to
  /// by the MachineInstr before all input registers are read.  This is used to
  /// model the GCC inline asm '&' constraint modifier.
  bool IsEarlyClobber : 1;

  /// IsDebug - True if this MO_Register 'use' operand is in a debug pseudo,
  /// not a real instruction.  Such uses should be ignored during codegen.
  bool IsDebug : 1;

  /// SmallContents - This really should be part of the Contents union, but
  /// lives out here so we can get a better packed struct.
  /// MO_Register: Register number.
  /// OffsetedInfo: Low bits of offset.
  union {
    unsigned RegNo;           // For MO_Register.
    unsigned OffsetLo;        // Matches Contents.OffsetedInfo.OffsetHi.
  } SmallContents;

  /// ParentMI - This is the instruction that this operand is embedded into.
  /// This is valid for all operand types, when the operand is in an instr.
  MachineInstr *ParentMI;

  /// Contents union - This contains the payload for the various operand types.
  union {
    MachineBasicBlock *MBB;   // For MO_MachineBasicBlock.
    const ConstantFP *CFP;    // For MO_FPImmediate.
    const ConstantInt *CI;    // For MO_CImmediate. Integers > 64bit.
    int64_t ImmVal;           // For MO_Immediate.
    const MDNode *MD;         // For MO_Metadata.
    MCSymbol *Sym;            // For MO_MCSymbol

    struct {                  // For MO_Register.
      // Register number is in SmallContents.RegNo.
      MachineOperand **Prev;  // Access list for register.
      MachineOperand *Next;
    } Reg;

    /// OffsetedInfo - This struct contains the offset and an object identifier.
    /// this represent the object as with an optional offset from it.
    struct {
      union {
        int Index;                // For MO_*Index - The index itself.
        const char *SymbolName;   // For MO_ExternalSymbol.
        const GlobalValue *GV;    // For MO_GlobalAddress.
        const BlockAddress *BA;   // For MO_BlockAddress.
      } Val;
      // Low bits of offset are in SmallContents.OffsetLo.
      int OffsetHi;               // An offset from the object, high 32 bits.
    } OffsetedInfo;
  } Contents;

  explicit MachineOperand(MachineOperandType K) : OpKind(K), ParentMI(0) {
    TargetFlags = 0;
  }
public:
  /// getType - Returns the MachineOperandType for this operand.
  ///
  MachineOperandType getType() const { return (MachineOperandType)OpKind; }

  unsigned char getTargetFlags() const { return TargetFlags; }
  void setTargetFlags(unsigned char F) { TargetFlags = F; }
  void addTargetFlag(unsigned char F) { TargetFlags |= F; }


  /// getParent - Return the instruction that this operand belongs to.
  ///
  MachineInstr *getParent() { return ParentMI; }
  const MachineInstr *getParent() const { return ParentMI; }

  /// clearParent - Reset the parent pointer.
  ///
  /// The MachineOperand copy constructor also copies ParentMI, expecting the
  /// original to be deleted. If a MachineOperand is ever stored outside a
  /// MachineInstr, the parent pointer must be cleared.
  ///
  /// Never call clearParent() on an operand in a MachineInstr.
  ///
  void clearParent() { ParentMI = 0; }

  void print(raw_ostream &os, const TargetMachine *TM = 0) const;

  //===--------------------------------------------------------------------===//
  // Accessors that tell you what kind of MachineOperand you're looking at.
  //===--------------------------------------------------------------------===//

  /// isReg - Tests if this is a MO_Register operand.
  bool isReg() const { return OpKind == MO_Register; }
  /// isImm - Tests if this is a MO_Immediate operand.
  bool isImm() const { return OpKind == MO_Immediate; }
  /// isCImm - Test if t his is a MO_CImmediate operand.
  bool isCImm() const { return OpKind == MO_CImmediate; }
  /// isFPImm - Tests if this is a MO_FPImmediate operand.
  bool isFPImm() const { return OpKind == MO_FPImmediate; }
  /// isMBB - Tests if this is a MO_MachineBasicBlock operand.
  bool isMBB() const { return OpKind == MO_MachineBasicBlock; }
  /// isFI - Tests if this is a MO_FrameIndex operand.
  bool isFI() const { return OpKind == MO_FrameIndex; }
  /// isCPI - Tests if this is a MO_ConstantPoolIndex operand.
  bool isCPI() const { return OpKind == MO_ConstantPoolIndex; }
  /// isJTI - Tests if this is a MO_JumpTableIndex operand.
  bool isJTI() const { return OpKind == MO_JumpTableIndex; }
  /// isGlobal - Tests if this is a MO_GlobalAddress operand.
  bool isGlobal() const { return OpKind == MO_GlobalAddress; }
  /// isSymbol - Tests if this is a MO_ExternalSymbol operand.
  bool isSymbol() const { return OpKind == MO_ExternalSymbol; }
  /// isBlockAddress - Tests if this is a MO_BlockAddress operand.
  bool isBlockAddress() const { return OpKind == MO_BlockAddress; }
  /// isMetadata - Tests if this is a MO_Metadata operand.
  bool isMetadata() const { return OpKind == MO_Metadata; }
  bool isMCSymbol() const { return OpKind == MO_MCSymbol; }

  //===--------------------------------------------------------------------===//
  // Accessors for Register Operands
  //===--------------------------------------------------------------------===//

  /// getReg - Returns the register number.
  unsigned getReg() const {
    assert(isReg() && "This is not a register operand!");
    return SmallContents.RegNo;
  }

  unsigned getSubReg() const {
    assert(isReg() && "Wrong MachineOperand accessor");
    return (unsigned)SubReg;
  }

  bool isUse() const {
    assert(isReg() && "Wrong MachineOperand accessor");
    return !IsDef;
  }

  bool isDef() const {
    assert(isReg() && "Wrong MachineOperand accessor");
    return IsDef;
  }

  bool isImplicit() const {
    assert(isReg() && "Wrong MachineOperand accessor");
    return IsImp;
  }

  bool isDead() const {
    assert(isReg() && "Wrong MachineOperand accessor");
    return IsDead;
  }

  bool isKill() const {
    assert(isReg() && "Wrong MachineOperand accessor");
    return IsKill;
  }

  bool isUndef() const {
    assert(isReg() && "Wrong MachineOperand accessor");
    return IsUndef;
  }

  bool isEarlyClobber() const {
    assert(isReg() && "Wrong MachineOperand accessor");
    return IsEarlyClobber;
  }

  bool isDebug() const {
    assert(isReg() && "Wrong MachineOperand accessor");
    return IsDebug;
  }

  /// readsReg - Returns true if this operand reads the previous value of its
  /// register.  A use operand with the <undef> flag set doesn't read its
  /// register.  A sub-register def implicitly reads the other parts of the
  /// register being redefined unless the <undef> flag is set.
  bool readsReg() const {
    assert(isReg() && "Wrong MachineOperand accessor");
    return !isUndef() && (isUse() || getSubReg());
  }

  /// getNextOperandForReg - Return the next MachineOperand in the function that
  /// uses or defines this register.
  MachineOperand *getNextOperandForReg() const {
    assert(isReg() && "This is not a register operand!");
    return Contents.Reg.Next;
  }

  //===--------------------------------------------------------------------===//
  // Mutators for Register Operands
  //===--------------------------------------------------------------------===//

  /// Change the register this operand corresponds to.
  ///
  void setReg(unsigned Reg);

  void setSubReg(unsigned subReg) {
    assert(isReg() && "Wrong MachineOperand accessor");
    SubReg = (unsigned char)subReg;
  }

  /// substVirtReg - Substitute the current register with the virtual
  /// subregister Reg:SubReg. Take any existing SubReg index into account,
  /// using TargetRegisterInfo to compose the subreg indices if necessary.
  /// Reg must be a virtual register, SubIdx can be 0.
  ///
  void substVirtReg(unsigned Reg, unsigned SubIdx, const TargetRegisterInfo&);

  /// substPhysReg - Substitute the current register with the physical register
  /// Reg, taking any existing SubReg into account. For instance,
  /// substPhysReg(%EAX) will change %reg1024:sub_8bit to %AL.
  ///
  void substPhysReg(unsigned Reg, const TargetRegisterInfo&);

  void setIsUse(bool Val = true) {
    assert(isReg() && "Wrong MachineOperand accessor");
    assert((Val || !isDebug()) && "Marking a debug operation as def");
    IsDef = !Val;
  }

  void setIsDef(bool Val = true) {
    assert(isReg() && "Wrong MachineOperand accessor");
    assert((!Val || !isDebug()) && "Marking a debug operation as def");
    IsDef = Val;
  }

  void setImplicit(bool Val = true) {
    assert(isReg() && "Wrong MachineOperand accessor");
    IsImp = Val;
  }

  void setIsKill(bool Val = true) {
    assert(isReg() && !IsDef && "Wrong MachineOperand accessor");
    assert((!Val || !isDebug()) && "Marking a debug operation as kill");
    IsKill = Val;
  }

  void setIsDead(bool Val = true) {
    assert(isReg() && IsDef && "Wrong MachineOperand accessor");
    IsDead = Val;
  }

  void setIsUndef(bool Val = true) {
    assert(isReg() && "Wrong MachineOperand accessor");
    IsUndef = Val;
  }

  void setIsEarlyClobber(bool Val = true) {
    assert(isReg() && IsDef && "Wrong MachineOperand accessor");
    IsEarlyClobber = Val;
  }

  void setIsDebug(bool Val = true) {
    assert(isReg() && IsDef && "Wrong MachineOperand accessor");
    IsDebug = Val;
  }

  //===--------------------------------------------------------------------===//
  // Accessors for various operand types.
  //===--------------------------------------------------------------------===//

  int64_t getImm() const {
    assert(isImm() && "Wrong MachineOperand accessor");
    return Contents.ImmVal;
  }

  const ConstantInt *getCImm() const {
    assert(isCImm() && "Wrong MachineOperand accessor");
    return Contents.CI;
  }

  const ConstantFP *getFPImm() const {
    assert(isFPImm() && "Wrong MachineOperand accessor");
    return Contents.CFP;
  }

  MachineBasicBlock *getMBB() const {
    assert(isMBB() && "Wrong MachineOperand accessor");
    return Contents.MBB;
  }

  int getIndex() const {
    assert((isFI() || isCPI() || isJTI()) &&
           "Wrong MachineOperand accessor");
    return Contents.OffsetedInfo.Val.Index;
  }

  const GlobalValue *getGlobal() const {
    assert(isGlobal() && "Wrong MachineOperand accessor");
    return Contents.OffsetedInfo.Val.GV;
  }

  const BlockAddress *getBlockAddress() const {
    assert(isBlockAddress() && "Wrong MachineOperand accessor");
    return Contents.OffsetedInfo.Val.BA;
  }

  MCSymbol *getMCSymbol() const {
    assert(isMCSymbol() && "Wrong MachineOperand accessor");
    return Contents.Sym;
  }

  /// getOffset - Return the offset from the symbol in this operand. This always
  /// returns 0 for ExternalSymbol operands.
  int64_t getOffset() const {
    assert((isGlobal() || isSymbol() || isCPI() || isBlockAddress()) &&
           "Wrong MachineOperand accessor");
    return (int64_t(Contents.OffsetedInfo.OffsetHi) << 32) |
           SmallContents.OffsetLo;
  }

  const char *getSymbolName() const {
    assert(isSymbol() && "Wrong MachineOperand accessor");
    return Contents.OffsetedInfo.Val.SymbolName;
  }

  const MDNode *getMetadata() const {
    assert(isMetadata() && "Wrong MachineOperand accessor");
    return Contents.MD;
  }

  //===--------------------------------------------------------------------===//
  // Mutators for various operand types.
  //===--------------------------------------------------------------------===//

  void setImm(int64_t immVal) {
    assert(isImm() && "Wrong MachineOperand mutator");
    Contents.ImmVal = immVal;
  }

  void setOffset(int64_t Offset) {
    assert((isGlobal() || isSymbol() || isCPI() || isBlockAddress()) &&
        "Wrong MachineOperand accessor");
    SmallContents.OffsetLo = unsigned(Offset);
    Contents.OffsetedInfo.OffsetHi = int(Offset >> 32);
  }

  void setIndex(int Idx) {
    assert((isFI() || isCPI() || isJTI()) &&
           "Wrong MachineOperand accessor");
    Contents.OffsetedInfo.Val.Index = Idx;
  }

  void setMBB(MachineBasicBlock *MBB) {
    assert(isMBB() && "Wrong MachineOperand accessor");
    Contents.MBB = MBB;
  }

  //===--------------------------------------------------------------------===//
  // Other methods.
  //===--------------------------------------------------------------------===//

  /// isIdenticalTo - Return true if this operand is identical to the specified
  /// operand. Note: This method ignores isKill and isDead properties.
  bool isIdenticalTo(const MachineOperand &Other) const;

  /// ChangeToImmediate - Replace this operand with a new immediate operand of
  /// the specified value.  If an operand is known to be an immediate already,
  /// the setImm method should be used.
  void ChangeToImmediate(int64_t ImmVal);

  /// ChangeToRegister - Replace this operand with a new register operand of
  /// the specified value.  If an operand is known to be an register already,
  /// the setReg method should be used.
  void ChangeToRegister(unsigned Reg, bool isDef, bool isImp = false,
                        bool isKill = false, bool isDead = false,
                        bool isUndef = false, bool isDebug = false);

  //===--------------------------------------------------------------------===//
  // Construction methods.
  //===--------------------------------------------------------------------===//

  static MachineOperand CreateImm(int64_t Val) {
    MachineOperand Op(MachineOperand::MO_Immediate);
    Op.setImm(Val);
    return Op;
  }

  static MachineOperand CreateCImm(const ConstantInt *CI) {
    MachineOperand Op(MachineOperand::MO_CImmediate);
    Op.Contents.CI = CI;
    return Op;
  }

  static MachineOperand CreateFPImm(const ConstantFP *CFP) {
    MachineOperand Op(MachineOperand::MO_FPImmediate);
    Op.Contents.CFP = CFP;
    return Op;
  }

  static MachineOperand CreateReg(unsigned Reg, bool isDef, bool isImp = false,
                                  bool isKill = false, bool isDead = false,
                                  bool isUndef = false,
                                  bool isEarlyClobber = false,
                                  unsigned SubReg = 0,
                                  bool isDebug = false) {
    MachineOperand Op(MachineOperand::MO_Register);
    Op.IsDef = isDef;
    Op.IsImp = isImp;
    Op.IsKill = isKill;
    Op.IsDead = isDead;
    Op.IsUndef = isUndef;
    Op.IsEarlyClobber = isEarlyClobber;
    Op.IsDebug = isDebug;
    Op.SmallContents.RegNo = Reg;
    Op.Contents.Reg.Prev = 0;
    Op.Contents.Reg.Next = 0;
    Op.SubReg = SubReg;
    return Op;
  }
  static MachineOperand CreateMBB(MachineBasicBlock *MBB,
                                  unsigned char TargetFlags = 0) {
    MachineOperand Op(MachineOperand::MO_MachineBasicBlock);
    Op.setMBB(MBB);
    Op.setTargetFlags(TargetFlags);
    return Op;
  }
  static MachineOperand CreateFI(int Idx) {
    MachineOperand Op(MachineOperand::MO_FrameIndex);
    Op.setIndex(Idx);
    return Op;
  }
  static MachineOperand CreateCPI(unsigned Idx, int Offset,
                                  unsigned char TargetFlags = 0) {
    MachineOperand Op(MachineOperand::MO_ConstantPoolIndex);
    Op.setIndex(Idx);
    Op.setOffset(Offset);
    Op.setTargetFlags(TargetFlags);
    return Op;
  }
  static MachineOperand CreateJTI(unsigned Idx,
                                  unsigned char TargetFlags = 0) {
    MachineOperand Op(MachineOperand::MO_JumpTableIndex);
    Op.setIndex(Idx);
    Op.setTargetFlags(TargetFlags);
    return Op;
  }
  static MachineOperand CreateGA(const GlobalValue *GV, int64_t Offset,
                                 unsigned char TargetFlags = 0) {
    MachineOperand Op(MachineOperand::MO_GlobalAddress);
    Op.Contents.OffsetedInfo.Val.GV = GV;
    Op.setOffset(Offset);
    Op.setTargetFlags(TargetFlags);
    return Op;
  }
  static MachineOperand CreateES(const char *SymName,
                                 unsigned char TargetFlags = 0) {
    MachineOperand Op(MachineOperand::MO_ExternalSymbol);
    Op.Contents.OffsetedInfo.Val.SymbolName = SymName;
    Op.setOffset(0); // Offset is always 0.
    Op.setTargetFlags(TargetFlags);
    return Op;
  }
  static MachineOperand CreateBA(const BlockAddress *BA,
                                 unsigned char TargetFlags = 0) {
    MachineOperand Op(MachineOperand::MO_BlockAddress);
    Op.Contents.OffsetedInfo.Val.BA = BA;
    Op.setOffset(0); // Offset is always 0.
    Op.setTargetFlags(TargetFlags);
    return Op;
  }
  static MachineOperand CreateMetadata(const MDNode *Meta) {
    MachineOperand Op(MachineOperand::MO_Metadata);
    Op.Contents.MD = Meta;
    return Op;
  }

  static MachineOperand CreateMCSymbol(MCSymbol *Sym) {
    MachineOperand Op(MachineOperand::MO_MCSymbol);
    Op.Contents.Sym = Sym;
    return Op;
  }

  friend class MachineInstr;
  friend class MachineRegisterInfo;
private:
  //===--------------------------------------------------------------------===//
  // Methods for handling register use/def lists.
  //===--------------------------------------------------------------------===//

  /// isOnRegUseList - Return true if this operand is on a register use/def list
  /// or false if not.  This can only be called for register operands that are
  /// part of a machine instruction.
  bool isOnRegUseList() const {
    assert(isReg() && "Can only add reg operand to use lists");
    return Contents.Reg.Prev != 0;
  }

  /// AddRegOperandToRegInfo - Add this register operand to the specified
  /// MachineRegisterInfo.  If it is null, then the next/prev fields should be
  /// explicitly nulled out.
  void AddRegOperandToRegInfo(MachineRegisterInfo *RegInfo);

  /// RemoveRegOperandFromRegInfo - Remove this register operand from the
  /// MachineRegisterInfo it is linked with.
  void RemoveRegOperandFromRegInfo();
};

inline raw_ostream &operator<<(raw_ostream &OS, const MachineOperand& MO) {
  MO.print(OS, 0);
  return OS;
}

} // End llvm namespace

#endif
