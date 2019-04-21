//===-- AlphaTargetMachine.h - Define TargetMachine for Alpha ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file declares the Alpha-specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

#ifndef ALPHA_TARGETMACHINE_H
#define ALPHA_TARGETMACHINE_H

#include "AlphaInstrInfo.h"
#include "AlphaISelLowering.h"
#include "AlphaFrameLowering.h"
#include "AlphaSelectionDAGInfo.h"
#include "AlphaSubtarget.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetFrameLowering.h"

namespace llvm {

class GlobalValue;

class AlphaTargetMachine : public LLVMTargetMachine {
  const TargetData DataLayout;       // Calculates type size & alignment
  AlphaInstrInfo InstrInfo;
  AlphaFrameLowering FrameLowering;
  AlphaSubtarget Subtarget;
  AlphaTargetLowering TLInfo;
  AlphaSelectionDAGInfo TSInfo;

public:
  AlphaTargetMachine(const Target &T, StringRef TT,
                     StringRef CPU, StringRef FS,
                     Reloc::Model RM, CodeModel::Model CM);

  virtual const AlphaInstrInfo *getInstrInfo() const { return &InstrInfo; }
  virtual const TargetFrameLowering  *getFrameLowering() const {
    return &FrameLowering;
  }
  virtual const AlphaSubtarget   *getSubtargetImpl() const{ return &Subtarget; }
  virtual const AlphaRegisterInfo *getRegisterInfo() const {
    return &InstrInfo.getRegisterInfo();
  }
  virtual const AlphaTargetLowering* getTargetLowering() const {
    return &TLInfo;
  }
  virtual const AlphaSelectionDAGInfo* getSelectionDAGInfo() const {
    return &TSInfo;
  }
  virtual const TargetData       *getTargetData() const { return &DataLayout; }

  // Pass Pipeline Configuration
  virtual bool addInstSelector(PassManagerBase &PM, CodeGenOpt::Level OptLevel);
  virtual bool addPreEmitPass(PassManagerBase &PM, CodeGenOpt::Level OptLevel);
};

} // end namespace llvm

#endif
