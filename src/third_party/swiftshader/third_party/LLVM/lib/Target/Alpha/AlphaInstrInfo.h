//===- AlphaInstrInfo.h - Alpha Instruction Information ---------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the Alpha implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef ALPHAINSTRUCTIONINFO_H
#define ALPHAINSTRUCTIONINFO_H

#include "llvm/Target/TargetInstrInfo.h"
#include "AlphaRegisterInfo.h"

#define GET_INSTRINFO_HEADER
#include "AlphaGenInstrInfo.inc"

namespace llvm {

class AlphaInstrInfo : public AlphaGenInstrInfo {
  const AlphaRegisterInfo RI;
public:
  AlphaInstrInfo();

  /// getRegisterInfo - TargetInstrInfo is a superset of MRegister info.  As
  /// such, whenever a client has an instance of instruction info, it should
  /// always be able to get register info as well (through this method).
  ///
  virtual const AlphaRegisterInfo &getRegisterInfo() const { return RI; }

  virtual unsigned isLoadFromStackSlot(const MachineInstr *MI,
                                       int &FrameIndex) const;
  virtual unsigned isStoreToStackSlot(const MachineInstr *MI,
                                      int &FrameIndex) const;
  
  virtual unsigned InsertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB,
                                MachineBasicBlock *FBB,
                                const SmallVectorImpl<MachineOperand> &Cond,
                                DebugLoc DL) const;
  virtual void copyPhysReg(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MI, DebugLoc DL,
                           unsigned DestReg, unsigned SrcReg,
                           bool KillSrc) const;
  virtual void storeRegToStackSlot(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator MBBI,
                                   unsigned SrcReg, bool isKill, int FrameIndex,
                                   const TargetRegisterClass *RC,
                                   const TargetRegisterInfo *TRI) const;

  virtual void loadRegFromStackSlot(MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator MBBI,
                                    unsigned DestReg, int FrameIndex,
                                    const TargetRegisterClass *RC,
                                    const TargetRegisterInfo *TRI) const;
  
  bool AnalyzeBranch(MachineBasicBlock &MBB,MachineBasicBlock *&TBB,
                     MachineBasicBlock *&FBB,
                     SmallVectorImpl<MachineOperand> &Cond,
                     bool AllowModify) const;
  unsigned RemoveBranch(MachineBasicBlock &MBB) const;
  void insertNoop(MachineBasicBlock &MBB, 
                  MachineBasicBlock::iterator MI) const;
  bool ReverseBranchCondition(SmallVectorImpl<MachineOperand> &Cond) const;

  /// getGlobalBaseReg - Return a virtual register initialized with the
  /// the global base register value. Output instructions required to
  /// initialize the register in the function entry block, if necessary.
  ///
  unsigned getGlobalBaseReg(MachineFunction *MF) const;

  /// getGlobalRetAddr - Return a virtual register initialized with the
  /// the global return address register value. Output instructions required to
  /// initialize the register in the function entry block, if necessary.
  ///
  unsigned getGlobalRetAddr(MachineFunction *MF) const;
};

}

#endif
