//===- InstrEnumEmitter.h - Generate Instruction Set Enums ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This tablegen backend is responsible for emitting enums for each machine
// instruction.
//
//===----------------------------------------------------------------------===//

#ifndef INSTRENUM_EMITTER_H
#define INSTRENUM_EMITTER_H

#include "llvm/TableGen/TableGenBackend.h"

namespace llvm {

class InstrEnumEmitter : public TableGenBackend {
  RecordKeeper &Records;
public:
  InstrEnumEmitter(RecordKeeper &R) : Records(R) {}

  // run - Output the instruction set description, returning true on failure.
  void run(raw_ostream &OS);
};

} // End llvm namespace

#endif
