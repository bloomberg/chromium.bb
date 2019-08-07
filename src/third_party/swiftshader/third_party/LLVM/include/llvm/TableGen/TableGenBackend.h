//===- llvm/TableGen/TableGenBackend.h - Backend base class -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// The TableGenBackend class is provided as a common interface for all TableGen
// backends.  It provides useful services and an standardized interface.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TABLEGEN_TABLEGENBACKEND_H
#define LLVM_TABLEGEN_TABLEGENBACKEND_H

#include "llvm/Support/raw_ostream.h"
#include <string>

namespace llvm {

class Record;
class RecordKeeper;

struct TableGenBackend {
  virtual ~TableGenBackend() {}

  // run - All TableGen backends should implement the run method, which should
  // be the main entry point.
  virtual void run(raw_ostream &OS) = 0;


public:   // Useful helper routines...
  /// EmitSourceFileHeader - Output a LLVM style file header to the specified
  /// ostream.
  void EmitSourceFileHeader(const std::string &Desc, raw_ostream &OS) const;

};

} // End llvm namespace

#endif
