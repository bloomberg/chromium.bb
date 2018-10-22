//===----------- ValueTypes.cpp - Implementation of EVT methods -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements methods in the CodeGen/ValueTypes.h header.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/StringExtras.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/LLVMContext.h"
#include "llvm/Type.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Support/ErrorHandling.h"
using namespace llvm;

EVT EVT::changeExtendedVectorElementTypeToInteger() const {
  LLVMContext &Context = LLVMTy->getContext();
  EVT IntTy = getIntegerVT(Context, getVectorElementType().getSizeInBits());
  return getVectorVT(Context, IntTy, getVectorNumElements());
}

EVT EVT::getExtendedIntegerVT(LLVMContext &Context, unsigned BitWidth) {
  EVT VT;
  VT.LLVMTy = IntegerType::get(Context, BitWidth);
  assert(VT.isExtended() && "Type is not extended!");
  return VT;
}

EVT EVT::getExtendedVectorVT(LLVMContext &Context, EVT VT,
                             unsigned NumElements) {
  EVT ResultVT;
  ResultVT.LLVMTy = VectorType::get(VT.getTypeForEVT(Context), NumElements);
  assert(ResultVT.isExtended() && "Type is not extended!");
  return ResultVT;
}

bool EVT::isExtendedFloatingPoint() const {
  assert(isExtended() && "Type is not extended!");
  return LLVMTy->isFPOrFPVectorTy();
}

bool EVT::isExtendedInteger() const {
  assert(isExtended() && "Type is not extended!");
  return LLVMTy->isIntOrIntVectorTy();
}

bool EVT::isExtendedVector() const {
  assert(isExtended() && "Type is not extended!");
  return LLVMTy->isVectorTy();
}

bool EVT::isExtended64BitVector() const {
  return isExtendedVector() && getSizeInBits() == 64;
}

bool EVT::isExtended128BitVector() const {
  return isExtendedVector() && getSizeInBits() == 128;
}

bool EVT::isExtended256BitVector() const {
  return isExtendedVector() && getSizeInBits() == 256;
}

bool EVT::isExtended512BitVector() const {
  return isExtendedVector() && getSizeInBits() == 512;
}

EVT EVT::getExtendedVectorElementType() const {
  assert(isExtended() && "Type is not extended!");
  return EVT::getEVT(cast<VectorType>(LLVMTy)->getElementType());
}

unsigned EVT::getExtendedVectorNumElements() const {
  assert(isExtended() && "Type is not extended!");
  return cast<VectorType>(LLVMTy)->getNumElements();
}

unsigned EVT::getExtendedSizeInBits() const {
  assert(isExtended() && "Type is not extended!");
  if (IntegerType *ITy = dyn_cast<IntegerType>(LLVMTy))
    return ITy->getBitWidth();
  if (VectorType *VTy = dyn_cast<VectorType>(LLVMTy))
    return VTy->getBitWidth();
  assert(false && "Unrecognized extended type!");
  return 0; // Suppress warnings.
}

/// getEVTString - This function returns value type as a string, e.g. "i32".
std::string EVT::getEVTString() const {
  switch (V.SimpleTy) {
  default:
    if (isVector())
      return "v" + utostr(getVectorNumElements()) +
             getVectorElementType().getEVTString();
    if (isInteger())
      return "i" + utostr(getSizeInBits());
    llvm_unreachable("Invalid EVT!");
    return "?";
  case MVT::i1:      return "i1";
  case MVT::i8:      return "i8";
  case MVT::i16:     return "i16";
  case MVT::i32:     return "i32";
  case MVT::i64:     return "i64";
  case MVT::i128:    return "i128";
  case MVT::f32:     return "f32";
  case MVT::f64:     return "f64";
  case MVT::f80:     return "f80";
  case MVT::f128:    return "f128";
  case MVT::ppcf128: return "ppcf128";
  case MVT::isVoid:  return "isVoid";
  case MVT::Other:   return "ch";
  case MVT::Glue:    return "glue";
  case MVT::x86mmx:  return "x86mmx";
  case MVT::v2i8:    return "v2i8";
  case MVT::v4i8:    return "v4i8";
  case MVT::v8i8:    return "v8i8";
  case MVT::v16i8:   return "v16i8";
  case MVT::v32i8:   return "v32i8";
  case MVT::v2i16:   return "v2i16";
  case MVT::v4i16:   return "v4i16";
  case MVT::v8i16:   return "v8i16";
  case MVT::v16i16:  return "v16i16";
  case MVT::v2i32:   return "v2i32";
  case MVT::v4i32:   return "v4i32";
  case MVT::v8i32:   return "v8i32";
  case MVT::v1i64:   return "v1i64";
  case MVT::v2i64:   return "v2i64";
  case MVT::v4i64:   return "v4i64";
  case MVT::v8i64:   return "v8i64";
  case MVT::v2f32:   return "v2f32";
  case MVT::v4f32:   return "v4f32";
  case MVT::v8f32:   return "v8f32";
  case MVT::v2f64:   return "v2f64";
  case MVT::v4f64:   return "v4f64";
  case MVT::Metadata:return "Metadata";
  case MVT::untyped: return "untyped";
  }
}

/// getTypeForEVT - This method returns an LLVM type corresponding to the
/// specified EVT.  For integer types, this returns an unsigned type.  Note
/// that this will abort for types that cannot be represented.
Type *EVT::getTypeForEVT(LLVMContext &Context) const {
  switch (V.SimpleTy) {
  default:
    assert(isExtended() && "Type is not extended!");
    return LLVMTy;
  case MVT::isVoid:  return Type::getVoidTy(Context);
  case MVT::i1:      return Type::getInt1Ty(Context);
  case MVT::i8:      return Type::getInt8Ty(Context);
  case MVT::i16:     return Type::getInt16Ty(Context);
  case MVT::i32:     return Type::getInt32Ty(Context);
  case MVT::i64:     return Type::getInt64Ty(Context);
  case MVT::i128:    return IntegerType::get(Context, 128);
  case MVT::f32:     return Type::getFloatTy(Context);
  case MVT::f64:     return Type::getDoubleTy(Context);
  case MVT::f80:     return Type::getX86_FP80Ty(Context);
  case MVT::f128:    return Type::getFP128Ty(Context);
  case MVT::ppcf128: return Type::getPPC_FP128Ty(Context);
  case MVT::x86mmx:  return Type::getX86_MMXTy(Context);
  case MVT::v2i8:    return VectorType::get(Type::getInt8Ty(Context), 2);
  case MVT::v4i8:    return VectorType::get(Type::getInt8Ty(Context), 4);
  case MVT::v8i8:    return VectorType::get(Type::getInt8Ty(Context), 8);
  case MVT::v16i8:   return VectorType::get(Type::getInt8Ty(Context), 16);
  case MVT::v32i8:   return VectorType::get(Type::getInt8Ty(Context), 32);
  case MVT::v2i16:   return VectorType::get(Type::getInt16Ty(Context), 2);
  case MVT::v4i16:   return VectorType::get(Type::getInt16Ty(Context), 4);
  case MVT::v8i16:   return VectorType::get(Type::getInt16Ty(Context), 8);
  case MVT::v16i16:  return VectorType::get(Type::getInt16Ty(Context), 16);
  case MVT::v2i32:   return VectorType::get(Type::getInt32Ty(Context), 2);
  case MVT::v4i32:   return VectorType::get(Type::getInt32Ty(Context), 4);
  case MVT::v8i32:   return VectorType::get(Type::getInt32Ty(Context), 8);
  case MVT::v1i64:   return VectorType::get(Type::getInt64Ty(Context), 1);
  case MVT::v2i64:   return VectorType::get(Type::getInt64Ty(Context), 2);
  case MVT::v4i64:   return VectorType::get(Type::getInt64Ty(Context), 4);
  case MVT::v8i64:   return VectorType::get(Type::getInt64Ty(Context), 8);
  case MVT::v2f32:   return VectorType::get(Type::getFloatTy(Context), 2);
  case MVT::v4f32:   return VectorType::get(Type::getFloatTy(Context), 4);
  case MVT::v8f32:   return VectorType::get(Type::getFloatTy(Context), 8);
  case MVT::v2f64:   return VectorType::get(Type::getDoubleTy(Context), 2);
  case MVT::v4f64:   return VectorType::get(Type::getDoubleTy(Context), 4); 
  case MVT::Metadata: return Type::getMetadataTy(Context);
 }
}

/// getEVT - Return the value type corresponding to the specified type.  This
/// returns all pointers as MVT::iPTR.  If HandleUnknown is true, unknown types
/// are returned as Other, otherwise they are invalid.
EVT EVT::getEVT(Type *Ty, bool HandleUnknown){
  switch (Ty->getTypeID()) {
  default:
    if (HandleUnknown) return MVT(MVT::Other);
    llvm_unreachable("Unknown type!");
    return MVT::isVoid;
  case Type::VoidTyID:
    return MVT::isVoid;
  case Type::IntegerTyID:
    return getIntegerVT(Ty->getContext(), cast<IntegerType>(Ty)->getBitWidth());
  case Type::FloatTyID:     return MVT(MVT::f32);
  case Type::DoubleTyID:    return MVT(MVT::f64);
  case Type::X86_FP80TyID:  return MVT(MVT::f80);
  case Type::X86_MMXTyID:   return MVT(MVT::x86mmx);
  case Type::FP128TyID:     return MVT(MVT::f128);
  case Type::PPC_FP128TyID: return MVT(MVT::ppcf128);
  case Type::PointerTyID:   return MVT(MVT::iPTR);
  case Type::VectorTyID: {
    VectorType *VTy = cast<VectorType>(Ty);
    return getVectorVT(Ty->getContext(), getEVT(VTy->getElementType(), false),
                       VTy->getNumElements());
  }
  }
}
