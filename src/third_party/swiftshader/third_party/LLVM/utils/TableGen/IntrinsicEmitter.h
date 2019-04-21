//===- IntrinsicEmitter.h - Generate intrinsic information ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This tablegen backend emits information about intrinsic functions.
//
//===----------------------------------------------------------------------===//

#ifndef INTRINSIC_EMITTER_H
#define INTRINSIC_EMITTER_H

#include "CodeGenIntrinsics.h"
#include "llvm/TableGen/TableGenBackend.h"

namespace llvm {
  class IntrinsicEmitter : public TableGenBackend {
    RecordKeeper &Records;
    bool TargetOnly;
    std::string TargetPrefix;
    
  public:
    IntrinsicEmitter(RecordKeeper &R, bool T = false) 
      : Records(R), TargetOnly(T) {}

    void run(raw_ostream &OS);

    void EmitPrefix(raw_ostream &OS);
    
    void EmitEnumInfo(const std::vector<CodeGenIntrinsic> &Ints, 
                      raw_ostream &OS);

    void EmitFnNameRecognizer(const std::vector<CodeGenIntrinsic> &Ints, 
                              raw_ostream &OS);
    void EmitIntrinsicToNameTable(const std::vector<CodeGenIntrinsic> &Ints, 
                                  raw_ostream &OS);
    void EmitIntrinsicToOverloadTable(const std::vector<CodeGenIntrinsic> &Ints, 
                                      raw_ostream &OS);
    void EmitVerifier(const std::vector<CodeGenIntrinsic> &Ints, 
                      raw_ostream &OS);
    void EmitGenerator(const std::vector<CodeGenIntrinsic> &Ints, 
                       raw_ostream &OS);
    void EmitAttributes(const std::vector<CodeGenIntrinsic> &Ints,
                        raw_ostream &OS);
    void EmitModRefBehavior(const std::vector<CodeGenIntrinsic> &Ints,
                            raw_ostream &OS);
    void EmitGCCBuiltinList(const std::vector<CodeGenIntrinsic> &Ints, 
                            raw_ostream &OS);
    void EmitIntrinsicToGCCBuiltinMap(const std::vector<CodeGenIntrinsic> &Ints, 
                                      raw_ostream &OS);
    void EmitSuffix(raw_ostream &OS);
  };

} // End llvm namespace

#endif



