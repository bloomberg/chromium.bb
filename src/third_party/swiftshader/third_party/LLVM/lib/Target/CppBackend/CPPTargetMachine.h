//===-- CPPTargetMachine.h - TargetMachine for the C++ backend --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file declares the TargetMachine that is used by the C++ backend.
//
//===----------------------------------------------------------------------===//

#ifndef CPPTARGETMACHINE_H
#define CPPTARGETMACHINE_H

#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetData.h"

namespace llvm {

class formatted_raw_ostream;

struct CPPTargetMachine : public TargetMachine {
  CPPTargetMachine(const Target &T, StringRef TT,
                   StringRef CPU, StringRef FS,
                   Reloc::Model RM, CodeModel::Model CM)
    : TargetMachine(T, TT, CPU, FS) {}

  virtual bool addPassesToEmitFile(PassManagerBase &PM,
                                   formatted_raw_ostream &Out,
                                   CodeGenFileType FileType,
                                   CodeGenOpt::Level OptLevel,
                                   bool DisableVerify);

  virtual const TargetData *getTargetData() const { return 0; }
};

extern Target TheCppBackendTarget;

} // End llvm namespace


#endif
