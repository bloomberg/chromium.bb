//===- SystemZInstrInfo.h - SystemZ Instruction Information -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the SystemZ implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TARGET_SYSTEMZINSTRINFO_H
#define LLVM_TARGET_SYSTEMZINSTRINFO_H

#include "SystemZ.h"
#include "SystemZRegisterInfo.h"
#include "llvm/ADT/IndexedMap.h"
#include "llvm/Target/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "SystemZGenInstrInfo.inc"

namespace llvm {

class SystemZTargetMachine;

/// SystemZII - This namespace holds all of the target specific flags that
/// instruction info tracks.
///
namespace SystemZII {
  enum {
    //===------------------------------------------------------------------===//
    // SystemZ Specific MachineOperand flags.

    MO_NO_FLAG = 0,

    /// MO_GOTENT - On a symbol operand this indicates that the immediate is
    /// the offset to the location of the symbol name from the base of the GOT.
    ///
    ///    SYMBOL_LABEL @GOTENT
    MO_GOTENT = 1,

    /// MO_PLT - On a symbol operand this indicates that the immediate is
    /// offset to the PLT entry of symbol name from the current code location.
    ///
    ///    SYMBOL_LABEL @PLT
    MO_PLT = 2
  };
}

class SystemZInstrInfo : public SystemZGenInstrInfo {
  const SystemZRegisterInfo RI;
  SystemZTargetMachine &TM;
public:
  explicit SystemZInstrInfo(SystemZTargetMachine &TM);

  /// getRegisterInfo - TargetInstrInfo is a superset of MRegister info.  As
  /// such, whenever a client has an instance of instruction info, it should
  /// always be able to get register info as well (through this method).
  ///
  virtual const SystemZRegisterInfo &getRegisterInfo() const { return RI; }

  virtual void copyPhysReg(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator I, DebugLoc DL,
                           unsigned DestReg, unsigned SrcReg,
                           bool KillSrc) const;

  unsigned isLoadFromStackSlot(const MachineInstr *MI, int &FrameIndex) const;
  unsigned isStoreToStackSlot(const MachineInstr *MI, int &FrameIndex) const;

  virtual void storeRegToStackSlot(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator MI,
                                   unsigned SrcReg, bool isKill,
                                   int FrameIndex,
                                   const TargetRegisterClass *RC,
                                   const TargetRegisterInfo *TRI) const;
  virtual void loadRegFromStackSlot(MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator MI,
                                    unsigned DestReg, int FrameIdx,
                                    const TargetRegisterClass *RC,
                                    const TargetRegisterInfo *TRI) const;

  bool ReverseBranchCondition(SmallVectorImpl<MachineOperand> &Cond) const;
  virtual bool isUnpredicatedTerminator(const MachineInstr *MI) const;
  virtual bool AnalyzeBranch(MachineBasicBlock &MBB,
                             MachineBasicBlock *&TBB,
                             MachineBasicBlock *&FBB,
                             SmallVectorImpl<MachineOperand> &Cond,
                             bool AllowModify) const;
  virtual unsigned InsertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB,
                                MachineBasicBlock *FBB,
                                const SmallVectorImpl<MachineOperand> &Cond,
                                DebugLoc DL) const;
  virtual unsigned RemoveBranch(MachineBasicBlock &MBB) const;

  SystemZCC::CondCodes getOppositeCondition(SystemZCC::CondCodes CC) const;
  SystemZCC::CondCodes getCondFromBranchOpc(unsigned Opc) const;
  const MCInstrDesc& getBrCond(SystemZCC::CondCodes CC) const;
  const MCInstrDesc& getLongDispOpc(unsigned Opc) const;

  const MCInstrDesc& getMemoryInstr(unsigned Opc, int64_t Offset = 0) const {
    if (Offset < 0 || Offset >= 4096)
      return getLongDispOpc(Opc);
    else
      return get(Opc);
  }
};

}

#endif
