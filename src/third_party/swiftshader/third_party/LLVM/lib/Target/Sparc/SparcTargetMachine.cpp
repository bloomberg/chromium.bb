//===-- SparcTargetMachine.cpp - Define TargetMachine for Sparc -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "Sparc.h"
#include "SparcTargetMachine.h"
#include "llvm/PassManager.h"
#include "llvm/Support/TargetRegistry.h"
using namespace llvm;

extern "C" void LLVMInitializeSparcTarget() {
  // Register the target.
  RegisterTargetMachine<SparcV8TargetMachine> X(TheSparcTarget);
  RegisterTargetMachine<SparcV9TargetMachine> Y(TheSparcV9Target);
}

/// SparcTargetMachine ctor - Create an ILP32 architecture model
///
SparcTargetMachine::SparcTargetMachine(const Target &T, StringRef TT, 
                                       StringRef CPU, StringRef FS,
                                       Reloc::Model RM, CodeModel::Model CM,
                                       bool is64bit)
  : LLVMTargetMachine(T, TT, CPU, FS, RM, CM),
    Subtarget(TT, CPU, FS, is64bit),
    DataLayout(Subtarget.getDataLayout()),
    TLInfo(*this), TSInfo(*this), InstrInfo(Subtarget),
    FrameLowering(Subtarget) {
}

bool SparcTargetMachine::addInstSelector(PassManagerBase &PM,
                                         CodeGenOpt::Level OptLevel) {
  PM.add(createSparcISelDag(*this));
  return false;
}

/// addPreEmitPass - This pass may be implemented by targets that want to run
/// passes immediately before machine code is emitted.  This should return
/// true if -print-machineinstrs should print out the code after the passes.
bool SparcTargetMachine::addPreEmitPass(PassManagerBase &PM,
                                        CodeGenOpt::Level OptLevel){
  PM.add(createSparcFPMoverPass(*this));
  PM.add(createSparcDelaySlotFillerPass(*this));
  return true;
}

SparcV8TargetMachine::SparcV8TargetMachine(const Target &T,
                                           StringRef TT, StringRef CPU,
                                           StringRef FS, Reloc::Model RM,
                                           CodeModel::Model CM)
  : SparcTargetMachine(T, TT, CPU, FS, RM, CM, false) {
}

SparcV9TargetMachine::SparcV9TargetMachine(const Target &T, 
                                           StringRef TT,  StringRef CPU,
                                           StringRef FS, Reloc::Model RM,
                                           CodeModel::Model CM)
  : SparcTargetMachine(T, TT, CPU, FS, RM, CM, true) {
}
