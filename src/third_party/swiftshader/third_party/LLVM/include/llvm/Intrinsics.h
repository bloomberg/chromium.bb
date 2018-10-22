//===-- llvm/Instrinsics.h - LLVM Intrinsic Function Handling ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines a set of enums which allow processing of intrinsic
// functions.  Values of these enum types are returned by
// Function::getIntrinsicID.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_INTRINSICS_H
#define LLVM_INTRINSICS_H

#include "llvm/ADT/ArrayRef.h"
#include <string>

namespace llvm {

class Type;
class FunctionType;
class Function;
class LLVMContext;
class Module;
class AttrListPtr;

/// Intrinsic Namespace - This namespace contains an enum with a value for
/// every intrinsic/builtin function known by LLVM.  These enum values are
/// returned by Function::getIntrinsicID().
///
namespace Intrinsic {
  enum ID {
    not_intrinsic = 0,   // Must be zero

    // Get the intrinsic enums generated from Intrinsics.td
#define GET_INTRINSIC_ENUM_VALUES
#include "llvm/Intrinsics.gen"    
#undef GET_INTRINSIC_ENUM_VALUES
    , num_intrinsics
  };
  
  /// Intrinsic::getName(ID) - Return the LLVM name for an intrinsic, such as
  /// "llvm.ppc.altivec.lvx".
  std::string getName(ID id, ArrayRef<Type*> Tys = ArrayRef<Type*>());
  
  /// Intrinsic::getType(ID) - Return the function type for an intrinsic.
  ///
  FunctionType *getType(LLVMContext &Context, ID id,
                              ArrayRef<Type*> Tys = ArrayRef<Type*>());

  /// Intrinsic::isOverloaded(ID) - Returns true if the intrinsic can be
  /// overloaded.
  bool isOverloaded(ID id);

  /// Intrinsic::getAttributes(ID) - Return the attributes for an intrinsic.
  ///
  AttrListPtr getAttributes(ID id);

  /// Intrinsic::getDeclaration(M, ID) - Create or insert an LLVM Function
  /// declaration for an intrinsic, and return it.
  ///
  /// The Tys and numTys parameters are for intrinsics with overloaded types
  /// (e.g., those using iAny, fAny, vAny, or iPTRAny). For a declaration for an
  /// overloaded intrinsic, Tys should point to an array of numTys pointers to
  /// Type, and must provide exactly one type for each overloaded type in the
  /// intrinsic.
  Function *getDeclaration(Module *M, ID id,
                           ArrayRef<Type*> Tys = ArrayRef<Type*>());
                           
  /// Map a GCC builtin name to an intrinsic ID.
  ID getIntrinsicForGCCBuiltin(const char *Prefix, const char *BuiltinName);
  
} // End Intrinsic namespace

} // End llvm namespace

#endif
