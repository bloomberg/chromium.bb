/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef MLIR_HLO_DIALECT_MHLO_TRANSFORMS_MAP_MHLO_TO_SCALAR_OP_H
#define MLIR_HLO_DIALECT_MHLO_TRANSFORMS_MAP_MHLO_TO_SCALAR_OP_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/iterator_range.h"
#include "mlir-hlo/Dialect/mhlo/IR/hlo_ops.h"
#include "mlir/Dialect/Arithmetic/IR/Arithmetic.h"
#include "mlir/Dialect/Complex/IR/Complex.h"
#include "mlir/Dialect/Math/IR/Math.h"
#include "mlir/Dialect/SCF/SCF.h"
#include "mlir/Dialect/Vector/IR/VectorOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/ImplicitLocOpBuilder.h"
#include "mlir/IR/TypeUtilities.h"

namespace mlir {
namespace mhlo {
namespace impl {

// A struct to map MhloBinaryOpTy type to the corresponding floating-point and
// integer scalar operation types.
template <typename MhloBinaryOpTy>
struct MhloToScalarOp {
  using FOp = void;
  using IOp = void;
  using UOp = void;
  using COp = void;
};

template <>
struct MhloToScalarOp<mhlo::AddOp> {
  using FOp = ::mlir::arith::AddFOp;
  using IOp = ::mlir::arith::AddIOp;
  using UOp = ::mlir::arith::AddIOp;
  using COp = ::mlir::complex::AddOp;
};
template <>
struct MhloToScalarOp<mhlo::AndOp> {
  using IOp = ::mlir::arith::AndIOp;
  using UOp = ::mlir::arith::AndIOp;
};
template <>
struct MhloToScalarOp<mhlo::CompareOp> {
  using FOp = ::mlir::arith::CmpFOp;
  using IOp = ::mlir::arith::CmpIOp;
  using UOp = ::mlir::arith::CmpIOp;
};
template <>
struct MhloToScalarOp<mhlo::CeilOp> {
  using FOp = ::mlir::math::CeilOp;
};
template <>
struct MhloToScalarOp<mhlo::CosOp> {
  using FOp = ::mlir::math::CosOp;
};
template <>
struct MhloToScalarOp<mhlo::DivOp> {
  using FOp = ::mlir::arith::DivFOp;
  using IOp = ::mlir::arith::DivSIOp;
  using UOp = ::mlir::arith::DivUIOp;
  using COp = ::mlir::complex::DivOp;
};
template <>
struct MhloToScalarOp<mhlo::ExpOp> {
  using FOp = ::mlir::math::ExpOp;
  using COp = ::mlir::complex::ExpOp;
};
template <>
struct MhloToScalarOp<mhlo::Expm1Op> {
  using FOp = ::mlir::math::ExpM1Op;
};
template <>
struct MhloToScalarOp<mhlo::FloorOp> {
  using FOp = ::mlir::math::FloorOp;
};
template <>
struct MhloToScalarOp<mhlo::MaxOp> {
  using FOp = ::mlir::arith::MaxFOp;
  using IOp = ::mlir::arith::MaxSIOp;
  using UOp = ::mlir::arith::MaxUIOp;
};
template <>
struct MhloToScalarOp<mhlo::MinOp> {
  using FOp = ::mlir::arith::MinFOp;
  using IOp = ::mlir::arith::MinSIOp;
  using UOp = ::mlir::arith::MinUIOp;
};
template <>
struct MhloToScalarOp<mhlo::LogOp> {
  using FOp = ::mlir::math::LogOp;
  using COp = ::mlir::complex::LogOp;
};
template <>
struct MhloToScalarOp<mhlo::Log1pOp> {
  using FOp = ::mlir::math::Log1pOp;
  using COp = ::mlir::complex::Log1pOp;
};
template <>
struct MhloToScalarOp<mhlo::MulOp> {
  using FOp = ::mlir::arith::MulFOp;
  using IOp = ::mlir::arith::MulIOp;
  using UOp = ::mlir::arith::MulIOp;
  using COp = ::mlir::complex::MulOp;
};
template <>
struct MhloToScalarOp<mhlo::OrOp> {
  using IOp = ::mlir::arith::OrIOp;
  using UOp = ::mlir::arith::OrIOp;
};
template <>
struct MhloToScalarOp<mhlo::PopulationCountOp> {
  using IOp = ::mlir::math::CtPopOp;
  using UOp = ::mlir::math::CtPopOp;
};
template <>
struct MhloToScalarOp<mhlo::RemOp> {
  using FOp = ::mlir::arith::RemFOp;
  using IOp = ::mlir::arith::RemSIOp;
  using UOp = ::mlir::arith::RemUIOp;
};
template <>
struct MhloToScalarOp<mhlo::RsqrtOp> {
  using FOp = ::mlir::math::RsqrtOp;
};
template <>
struct MhloToScalarOp<mhlo::SubOp> {
  using FOp = ::mlir::arith::SubFOp;
  using IOp = ::mlir::arith::SubIOp;
  using UOp = ::mlir::arith::SubIOp;
  using COp = ::mlir::complex::SubOp;
};
template <>
struct MhloToScalarOp<mhlo::SqrtOp> {
  using FOp = ::mlir::math::SqrtOp;
};
template <>
struct MhloToScalarOp<mhlo::SinOp> {
  using FOp = ::mlir::math::SinOp;
};
template <>
struct MhloToScalarOp<mhlo::ShiftLeftOp> {
  using IOp = ::mlir::arith::ShLIOp;
  using UOp = ::mlir::arith::ShLIOp;
};
template <>
struct MhloToScalarOp<mhlo::ShiftRightArithmeticOp> {
  using IOp = ::mlir::arith::ShRSIOp;
  using UOp = ::mlir::arith::ShRSIOp;
};
template <>
struct MhloToScalarOp<mhlo::ShiftRightLogicalOp> {
  using IOp = ::mlir::arith::ShRUIOp;
  using UOp = ::mlir::arith::ShRUIOp;
};
template <>
struct MhloToScalarOp<mhlo::Atan2Op> {
  using FOp = ::mlir::math::Atan2Op;
};
template <>
struct MhloToScalarOp<mhlo::TanhOp> {
  using FOp = ::mlir::math::TanhOp;
};
template <>
struct MhloToScalarOp<mhlo::XorOp> {
  using IOp = ::mlir::arith::XOrIOp;
  using UOp = ::mlir::arith::XOrIOp;
};

// Alias for the map from MHLO binary op type to STD floating-point op type.
template <typename MhloOp>
using ScalarFOp = typename MhloToScalarOp<MhloOp>::FOp;
// Alias for the map from MHLO binary op type to STD signed integer op type.
template <typename MhloOp>
using ScalarIOp = typename MhloToScalarOp<MhloOp>::IOp;
// Alias for the map from MHLO binary op type to STD unsigned integer op type.
template <typename MhloOp>
using ScalarUOp = typename MhloToScalarOp<MhloOp>::UOp;
// Alias for the map from MHLO binary op type to STD complex op type.
template <typename MhloOp>
using ScalarCOp = typename MhloToScalarOp<MhloOp>::COp;

template <typename... Args>
struct MapMhloOpToScalarOpImpl {
  Value operator()(Location /*loc*/, ArrayRef<Type> /*result_types*/,
                   ArrayRef<Type> /*arg_types*/, ValueRange /*args*/,
                   OpBuilder* /*b*/) {
    return nullptr;
  }
};

template <typename StdScalarOp>
struct MapMhloOpToScalarOpImpl<StdScalarOp> {
  Value operator()(Location loc, ArrayRef<Type> result_types,
                   ArrayRef<Type> /*arg_types*/, ValueRange args,
                   OpBuilder* b) {
    return b->template create<StdScalarOp>(loc, result_types, args, mlir::None);
  }
};

template <typename SupportedType, typename StdScalarOp, typename... Args>
struct MapMhloOpToScalarOpImpl<SupportedType, StdScalarOp, Args...> {
  Value operator()(Location loc, ArrayRef<Type> result_types,
                   ArrayRef<Type> arg_types, ValueRange args, OpBuilder* b) {
    Type element_type = getElementTypeOrSelf(arg_types.front());
    if (SupportedType{}(element_type)) {
      return b->template create<StdScalarOp>(loc, result_types, args,
                                             mlir::None);
    }
    return MapMhloOpToScalarOpImpl<Args...>{}(loc, result_types, arg_types,
                                              args, b);
  }
};

template <typename SupportedType, typename... Args>
struct MapMhloOpToScalarOpImpl<SupportedType, void, Args...> {
  Value operator()(Location loc, ArrayRef<Type> result_types,
                   ArrayRef<Type> arg_types, ValueRange args, OpBuilder* b) {
    return MapMhloOpToScalarOpImpl<Args...>{}(loc, result_types, arg_types,
                                              args, b);
  }
};

struct isAnyIntegerType {
  bool operator()(Type t) { return t.isa<IntegerType>(); }
};

struct isSignedIntegerType {
  bool operator()(Type t) {
    // Pretend that signless is signed. This will change eventually.
    return t.isa<IntegerType>() && !t.isUnsignedInteger();
  }
};

struct isUnsignedIntegerType {
  bool operator()(Type t) { return t.isUnsignedInteger(); }
};

struct isFloatType {
  bool operator()(Type t) { return t.isa<FloatType>(); }
};

struct isComplexType {
  bool operator()(Type t) { return t.isa<ComplexType>(); }
};

template <template <typename T> class MapTy, typename OpTy,
          typename PredTy = llvm::is_detected<MapTy, OpTy>>
struct MapableIf {
  using type = void;
};
template <template <typename T> class MapTy, typename OpTy>
struct MapableIf<MapTy, OpTy, std::true_type> {
  using type = MapTy<OpTy>;
};

// Inserts the computation that corresponds to the body of the loop for lowered
// MHLO unary/binary op. Returns the value for the result.
template <typename MhloOpTy>
inline Value MapMhloOpToStdScalarOp(Location loc, ArrayRef<Type> result_types,
                                    ArrayRef<Type> arg_types, ValueRange args,
                                    OpBuilder* b) {
  using ScalarIOpOrVoid = typename MapableIf<ScalarIOp, MhloOpTy>::type;
  using ScalarUOpOrVoid = typename MapableIf<ScalarUOp, MhloOpTy>::type;
  using ScalarFOpOrVoid = typename MapableIf<ScalarFOp, MhloOpTy>::type;
  using ScalarCOpOrVoid = typename MapableIf<ScalarCOp, MhloOpTy>::type;
  return MapMhloOpToScalarOpImpl<isSignedIntegerType, ScalarIOpOrVoid,
                                 isUnsignedIntegerType, ScalarUOpOrVoid,
                                 isFloatType, ScalarFOpOrVoid, isComplexType,
                                 ScalarCOpOrVoid>{}(loc, result_types,
                                                    arg_types, args, b);
}

template <>
inline Value MapMhloOpToStdScalarOp<mhlo::AbsOp>(Location loc,
                                                 ArrayRef<Type> result_types,
                                                 ArrayRef<Type> arg_types,
                                                 ValueRange args,
                                                 OpBuilder* b) {
  Type element_type = getElementTypeOrSelf(arg_types.front());
  if (element_type.isa<FloatType>()) {
    return MapMhloOpToScalarOpImpl<isFloatType, ::mlir::math::AbsOp>{}(
        loc, result_types, arg_types, args, b);
  }
  if (element_type.isa<ComplexType>()) {
    return MapMhloOpToScalarOpImpl<isComplexType, ::mlir::complex::AbsOp>{}(
        loc, result_types, arg_types, args, b);
  }
  if (element_type.isSignlessInteger() || element_type.isSignedInteger()) {
    // lmhlo.abs(x, result) ->  result = select((x > 0), x, sub(0, x))
    Value lhs = args[0];
    auto integer_type = element_type.dyn_cast<IntegerType>();

    Value zero_intval = b->create<::mlir::arith::ConstantIntOp>(
        loc, 0, integer_type.getWidth());
    if (VectorType vec_type = args.front().getType().dyn_cast<VectorType>()) {
      zero_intval =
          b->create<::mlir::vector::SplatOp>(loc, vec_type, zero_intval);
    }
    auto lhs_gt_zero = b->create<ScalarIOp<CompareOp>>(
        loc, arith::CmpIPredicate::sge, lhs, zero_intval);
    auto neg_val = b->create<ScalarIOp<mhlo::SubOp>>(loc, zero_intval, lhs);
    return b->create<::mlir::arith::SelectOp>(loc, lhs_gt_zero, lhs, neg_val);
  }
  return nullptr;
}

template <>
inline Value MapMhloOpToStdScalarOp<mhlo::CbrtOp>(Location loc,
                                                  ArrayRef<Type> result_types,
                                                  ArrayRef<Type> arg_types,
                                                  ValueRange args,
                                                  OpBuilder* b) {
  mhlo::CbrtOp::Adaptor adaptor(args);
  Type element_type = getElementTypeOrSelf(arg_types.front());
  if (auto float_type = element_type.dyn_cast<FloatType>()) {
    // Convert cbrt(x) to copysign(cbrt(abs(x), 1.0 / 3.0), x).
    // This is to allow cbrt using pow while still handling negative numbers. It
    // should match most cbrt intrinsics.
    Value abs = b->create<mlir::math::AbsOp>(loc, adaptor.operand());
    Value third = b->create<arith::ConstantOp>(
        loc, b->getFloatAttr(float_type, 1.0 / 3.0));
    Value pow = b->create<mlir::math::PowFOp>(loc, result_types[0], abs, third);
    return b->create<mlir::math::CopySignOp>(loc, float_type, pow,
                                             adaptor.operand());
  }
  return nullptr;
}

template <typename PredicateType>
inline Optional<PredicateType> getCmpPredicate(StringRef, bool) {
  return llvm::None;
}

template <>
inline Optional<arith::CmpFPredicate> getCmpPredicate<arith::CmpFPredicate>(
    StringRef comparison_direction, bool is_signed) {
  assert(is_signed && "cannot have an unsigned float!");
  return llvm::StringSwitch<Optional<arith::CmpFPredicate>>(
             comparison_direction)
      .Case("EQ", arith::CmpFPredicate::OEQ)
      .Case("NE", arith::CmpFPredicate::UNE)
      .Case("GE", arith::CmpFPredicate::OGE)
      .Case("GT", arith::CmpFPredicate::OGT)
      .Case("LE", arith::CmpFPredicate::OLE)
      .Case("LT", arith::CmpFPredicate::OLT)
      .Default(llvm::None);
}

template <>
inline Optional<arith::CmpIPredicate> getCmpPredicate<arith::CmpIPredicate>(
    StringRef comparison_direction, bool is_signed) {
  return llvm::StringSwitch<Optional<arith::CmpIPredicate>>(
             comparison_direction)
      .Case("EQ", arith::CmpIPredicate::eq)
      .Case("NE", arith::CmpIPredicate::ne)
      .Case("GE",
            is_signed ? arith::CmpIPredicate::sge : arith::CmpIPredicate::uge)
      .Case("GT",
            is_signed ? arith::CmpIPredicate::sgt : arith::CmpIPredicate::ugt)
      .Case("LE",
            is_signed ? arith::CmpIPredicate::sle : arith::CmpIPredicate::ule)
      .Case("LT",
            is_signed ? arith::CmpIPredicate::slt : arith::CmpIPredicate::ult)
      .Default(llvm::None);
}

template <typename CompareOpTy>
inline Value MapCompareOpToStdScalarOp(Location loc,
                                       StringRef comparison_direction,
                                       ArrayRef<Type> /*result_types*/,
                                       ArrayRef<Type> arg_types,
                                       ValueRange args, OpBuilder* b) {
  const auto& lhs = args[0];
  const auto& rhs = args[1];
  Type element_type = getElementTypeOrSelf(arg_types.front());
  if (element_type.isa<IntegerType>()) {
    Optional<arith::CmpIPredicate> predicate =
        getCmpPredicate<arith::CmpIPredicate>(
            comparison_direction, !element_type.isUnsignedInteger());
    assert(predicate.hasValue() && "expected valid comparison direction");
    return b->create<ScalarIOp<CompareOpTy>>(loc, predicate.getValue(), lhs,
                                             rhs);
  }
  if (element_type.isa<FloatType>()) {
    Optional<arith::CmpFPredicate> predicate =
        getCmpPredicate<arith::CmpFPredicate>(comparison_direction,
                                              /*is_signed=*/true);
    assert(predicate.hasValue() && "expected valid comparison direction");
    return b->create<ScalarFOp<CompareOpTy>>(loc, predicate.getValue(), lhs,
                                             rhs);
  }
  if (auto complex_type = element_type.dyn_cast<ComplexType>()) {
    if (complex_type.getElementType().isa<FloatType>()) {
      if (comparison_direction == "EQ") {
        return b->create<complex::EqualOp>(loc, lhs, rhs);
      }
      if (comparison_direction == "NE") {
        return b->create<complex::NotEqualOp>(loc, lhs, rhs);
      }
    }
  }
  return nullptr;
}

template <>
inline Value MapMhloOpToStdScalarOp<mhlo::CopyOp>(
    Location /*loc*/, ArrayRef<Type> /*result_types*/,
    ArrayRef<Type> /*arg_types*/, ValueRange args, OpBuilder* /*b*/) {
  return args.front();
}

template <>
inline Value MapMhloOpToStdScalarOp<mhlo::ComplexOp>(
    Location loc, ArrayRef<Type> result_types, ArrayRef<Type> arg_types,
    ValueRange args, OpBuilder* b) {
  return MapMhloOpToScalarOpImpl<complex::CreateOp>{}(loc, result_types,
                                                      arg_types, args, b);
}

template <>
inline Value MapMhloOpToStdScalarOp<mhlo::RealOp>(Location loc,
                                                  ArrayRef<Type> result_types,
                                                  ArrayRef<Type> arg_types,
                                                  ValueRange args,
                                                  OpBuilder* b) {
  return MapMhloOpToScalarOpImpl<complex::ReOp>{}(loc, result_types, arg_types,
                                                  args, b);
}

template <>
inline Value MapMhloOpToStdScalarOp<mhlo::ImagOp>(Location loc,
                                                  ArrayRef<Type> result_types,
                                                  ArrayRef<Type> arg_types,
                                                  ValueRange args,
                                                  OpBuilder* b) {
  return MapMhloOpToScalarOpImpl<complex::ImOp>{}(loc, result_types, arg_types,
                                                  args, b);
}

template <>
inline Value MapMhloOpToStdScalarOp<mhlo::ConvertOp>(
    Location loc, ArrayRef<Type> result_types, ArrayRef<Type> arg_types,
    ValueRange args, OpBuilder* b) {
  Type sourceType = getElementTypeOrSelf(arg_types.front());
  Type targetType = getElementTypeOrSelf(result_types.front());
  Type convertedSourceType = getElementTypeOrSelf(args.front());

  // A boolean value is considered to be unsigned when converting to
  // floating-point. Otherwise, it will become `-1`.
  if ((sourceType.isInteger(/*width=*/1) || sourceType.isUnsignedInteger()) &&
      mlir::arith::UIToFPOp::areCastCompatible(convertedSourceType,
                                               targetType)) {
    return b->create<mlir::arith::UIToFPOp>(loc, result_types, args,
                                            mlir::None);
  }
  if (mlir::arith::SIToFPOp::areCastCompatible(convertedSourceType,
                                               targetType)) {
    return b->create<mlir::arith::SIToFPOp>(loc, result_types, args,
                                            mlir::None);
  } else if (sourceType.isa<FloatType>() && targetType.isa<FloatType>()) {
    FloatType src = sourceType.cast<FloatType>();
    FloatType res = targetType.cast<FloatType>();
    if (src.getWidth() > res.getWidth()) {
      return b->create<mlir::arith::TruncFOp>(loc, result_types, args,
                                              mlir::None);
    } else if (src.getWidth() < res.getWidth()) {
      return b->create<mlir::arith::ExtFOp>(loc, result_types, args,
                                            mlir::None);
    }
    // No conversion is needed for the same width floats
    return args.front();
  }
  if (targetType.isInteger(/*width=*/1)) {
    // When casting to bool, we need to compare whether the value is equal to
    // zero.
    if (sourceType.isSignlessInteger() || sourceType.isUnsignedInteger()) {
      Value zero_intval = b->create<::mlir::arith::ConstantIntOp>(
          loc, 0, sourceType.cast<IntegerType>().getWidth());
      if (VectorType vec_type = args.front().getType().dyn_cast<VectorType>()) {
        zero_intval =
            b->create<::mlir::vector::SplatOp>(loc, vec_type, zero_intval);
      }
      return b->create<mlir::arith::CmpIOp>(loc, arith::CmpIPredicate::ne,
                                            args.front(), zero_intval);
    }
    if (sourceType.isa<FloatType>()) {
      Value zero =
          b->create<arith::ConstantOp>(loc, b->getFloatAttr(sourceType, 0.0));
      if (VectorType vec_type = args.front().getType().dyn_cast<VectorType>()) {
        zero = b->create<::mlir::vector::SplatOp>(loc, vec_type, zero);
      }
      return b->create<mlir::arith::CmpFOp>(loc, arith::CmpFPredicate::UNE,
                                            args.front(), zero);
    }
  }
  if (sourceType.isa<IntegerType>() && targetType.isa<IntegerType>()) {
    IntegerType src = sourceType.cast<IntegerType>();
    IntegerType res = targetType.cast<IntegerType>();
    if (src.getWidth() > res.getWidth()) {
      return b->create<mlir::arith::TruncIOp>(loc, result_types, args,
                                              mlir::None);
    }
    if (src.getWidth() < res.getWidth()) {
      // Special case boolean values, so they get casted to `1` instead of `-1`.
      if (src.isUnsignedInteger() || src.getWidth() == 1) {
        return b->create<mlir::arith::ExtUIOp>(loc, result_types, args,
                                               mlir::None);
      }
      return b->create<mlir::arith::ExtSIOp>(loc, result_types, args,
                                             mlir::None);
    }
    // No conversion is needed for the same width integers
    return args.front();
  }
  if (mlir::arith::FPToSIOp::areCastCompatible(convertedSourceType,
                                               targetType)) {
    return b->create<mlir::arith::FPToSIOp>(loc, result_types, args,
                                            mlir::None);
  }
  return nullptr;
}

template <>
inline Value MapMhloOpToStdScalarOp<mhlo::BitcastConvertOp>(
    Location loc, ArrayRef<Type> result_types, ArrayRef<Type>, ValueRange args,
    OpBuilder* b) {
  return b->create<mlir::arith::BitcastOp>(loc, result_types, args);
}

template <>
inline Value MapMhloOpToStdScalarOp<mhlo::DotOp>(Location loc,
                                                 ArrayRef<Type> result_types,
                                                 ArrayRef<Type> arg_types,
                                                 ValueRange args,
                                                 OpBuilder* b) {
  // Dot Op converter from lhlo to affine only accepts float and integer types.
  const auto& lhs = args[0];
  const auto& rhs = args[1];
  const auto& result = args[2];
  Type element_type = lhs.getType();
  if (element_type.isa<FloatType>()) {
    Value float_mul =
        MapMhloOpToScalarOpImpl<isFloatType, ::mlir::arith::MulFOp>{}(
            loc, result_types, arg_types, {lhs, rhs}, b);
    return MapMhloOpToScalarOpImpl<isFloatType, ::mlir::arith::AddFOp>{}(
        loc, result_types, arg_types, {float_mul, result}, b);
  }
  if (element_type.isa<IntegerType>()) {
    Value int_mul =
        MapMhloOpToScalarOpImpl<isAnyIntegerType, ::mlir::arith::MulIOp>{}(
            loc, result_types, arg_types, {lhs, rhs}, b);
    return MapMhloOpToScalarOpImpl<isAnyIntegerType, ::mlir::arith::AddIOp>{}(
        loc, result_types, arg_types, {int_mul, result}, b);
  }
  return nullptr;
}

template <>
inline Value MapMhloOpToStdScalarOp<mhlo::IsFiniteOp>(
    Location loc, ArrayRef<Type> /*result_types*/, ArrayRef<Type> /*arg_types*/,
    ValueRange args, OpBuilder* b) {
  if (args[0].getType().isa<FloatType>()) {
    auto pos_inf = APFloat::getInf(
        args[0].getType().cast<FloatType>().getFloatSemantics());
    auto const_pos_inf = b->create<arith::ConstantOp>(
        loc, b->getFloatAttr(args[0].getType(), pos_inf));
    Value abs_x = b->create<::mlir::math::AbsOp>(loc, args[0]);
    return b->create<::mlir::arith::CmpFOp>(loc, arith::CmpFPredicate::ONE,
                                            abs_x, const_pos_inf);
  }
  return nullptr;
}

/// Implements the conversion of HLO op to scalar op (to use within region of a
/// linalg.generic op) for compare-select style operations like min/max.
template <typename... Args>
struct CompareSelectOpToStdScalarOp {
  static Value map(Location /*loc*/, StringRef /*comparison_direction*/,
                   ArrayRef<Type> /*result_types*/,
                   ArrayRef<Type> /*arg_types*/, ValueRange /*args*/,
                   OpBuilder* /*b*/) {
    return nullptr;
  }
};

/// Specialization which allows converting to a comparison operation in standard
/// dialect with a given predicate based on the element type of the operand.
template <typename SupportedType, typename StdCompareOp, typename Predicate,
          typename... Args>
struct CompareSelectOpToStdScalarOp<SupportedType, StdCompareOp, Predicate,
                                    Args...> {
  static Value map(Location loc, StringRef comparison_direction,
                   ArrayRef<Type> result_types, ArrayRef<Type> arg_types,
                   ValueRange args, OpBuilder* b) {
    Type element_type = getElementTypeOrSelf(arg_types.front());
    if (element_type.isa<SupportedType>()) {
      auto predicate = getCmpPredicate<Predicate>(
          comparison_direction, !element_type.isUnsignedInteger());
      assert(predicate.hasValue() && "expected valid comparison direction");
      auto cmp = b->template create<StdCompareOp>(loc, predicate.getValue(),
                                                  args[0], args[1]);
      return b->create<::mlir::arith::SelectOp>(loc, cmp, args[0], args[1]);
    }
    return CompareSelectOpToStdScalarOp<Args...>::map(
        loc, comparison_direction, result_types, arg_types, args, b);
  }
};

inline Value MhloAlwaysPropagateNaN(Value v, ValueRange args, Location loc,
                                    OpBuilder* b) {
  Type element_type = getElementTypeOrSelf(args.front().getType());
  if (auto float_type = element_type.dyn_cast<FloatType>()) {
    Value isnan = b->create<mlir::arith::CmpFOp>(loc, arith::CmpFPredicate::UNO,
                                                 args[0], args[1]);

    auto nan_apfloat = APFloat::getQNaN(float_type.getFloatSemantics());
    Value nan =
        b->create<mlir::arith::ConstantFloatOp>(loc, nan_apfloat, float_type);
    if (VectorType vec_type = args[0].getType().dyn_cast<VectorType>()) {
      nan = b->create<::mlir::vector::SplatOp>(loc, vec_type, nan);
    }
    v = b->create<mlir::arith::SelectOp>(loc, isnan, nan, v);
  }
  return v;
}

template <>
inline Value MapMhloOpToStdScalarOp<mhlo::LogisticOp>(
    Location loc, ArrayRef<Type> result_types, ArrayRef<Type> /*arg_types*/,
    ValueRange args, OpBuilder* b) {
  auto ty = result_types.front().cast<FloatType>();
  Value one = b->create<arith::ConstantOp>(loc, b->getFloatAttr(ty, 1.0));
  Value x = args.front();
  Value neg_x = b->create<arith::NegFOp>(loc, x);
  Value exp_neg_x = b->create<::mlir::math::ExpOp>(loc, neg_x);
  Value one_add_exp_neg_x = b->create<arith::AddFOp>(loc, one, exp_neg_x);
  return b->create<arith::DivFOp>(loc, one, one_add_exp_neg_x);
}

template <>
inline Value MapMhloOpToStdScalarOp<mhlo::ClampOp>(Location loc,
                                                   ArrayRef<Type> result_types,
                                                   ArrayRef<Type> arg_types,
                                                   ValueRange args,
                                                   OpBuilder* b) {
  assert(args.size() == 3 && "expected 3 arguments");
  Value lb = args[0];
  Value x = args[1];
  Value ub = args[2];

  // clamp(lb, x, ub) = max(min(x, ub), lb)
  Value min_x_ub = MapMhloOpToStdScalarOp<mhlo::MinOp>(loc, result_types,
                                                       arg_types, {x, ub}, b);
  return MapMhloOpToStdScalarOp<mhlo::MaxOp>(loc, result_types, arg_types,
                                             {min_x_ub, lb}, b);
}

template <>
inline Value MapMhloOpToStdScalarOp<mhlo::NegOp>(Location loc,
                                                 ArrayRef<Type> result_types,
                                                 ArrayRef<Type> arg_types,
                                                 ValueRange args,
                                                 OpBuilder* b) {
  Type element_type = getElementTypeOrSelf(args.front().getType());
  if (element_type.isa<ComplexType, FloatType>()) {
    return MapMhloOpToScalarOpImpl<isFloatType, ::mlir::arith::NegFOp,
                                   isComplexType, ::mlir::complex::NegOp>{}(
        loc, result_types, arg_types, args, b);
  }
  if (element_type.isa<IntegerType>()) {
    // lmhlo.neg(x, result) -> result = sub(0, x)
    Value lhs = args[0];
    auto integer_type = element_type.dyn_cast<IntegerType>();

    Value zero_intval = b->create<::mlir::arith::ConstantIntOp>(
        loc, 0, integer_type.getWidth());
    if (VectorType vec_type = args.front().getType().dyn_cast<VectorType>()) {
      zero_intval =
          b->create<::mlir::vector::SplatOp>(loc, vec_type, zero_intval);
    }
    return b->create<ScalarIOp<mhlo::SubOp>>(loc, zero_intval, lhs);
  }
  return nullptr;
}

template <>
inline Value MapMhloOpToStdScalarOp<mhlo::NotOp>(
    Location loc, ArrayRef<Type> /*result_types*/, ArrayRef<Type> /*arg_types*/,
    ValueRange args, OpBuilder* b) {
  Type element_type = getElementTypeOrSelf(args.front().getType());
  if (auto integer_type = element_type.dyn_cast<IntegerType>()) {
    // lmhlo.not(x) -> x ^ -1
    Value all_ones = b->create<::mlir::arith::ConstantIntOp>(
        loc, -1, integer_type.getWidth());
    if (VectorType vec_type = args.front().getType().dyn_cast<VectorType>()) {
      all_ones = b->create<::mlir::vector::SplatOp>(loc, vec_type, all_ones);
    }
    return b->create<::mlir::arith::XOrIOp>(loc, all_ones, args[0]);
  }
  return nullptr;
}

template <>
inline Value MapMhloOpToStdScalarOp<mhlo::PowOp>(Location loc,
                                                 ArrayRef<Type> result_types,
                                                 ArrayRef<Type> arg_types,
                                                 ValueRange args,
                                                 OpBuilder* b) {
  mhlo::PowOp::Adaptor adaptor(args);
  auto lb = ImplicitLocOpBuilder(loc, *b);
  // Floating point can use std::powf
  auto result_type = result_types.front();
  if (result_type.isa<::mlir::FloatType>())
    return MapMhloOpToScalarOpImpl<::mlir::math::PowFOp>{}(loc, result_types,
                                                           arg_types, args, b);

  assert(result_type.isa<::mlir::IntegerType>() &&
         "only float and integer `pow` is supported right now");

  // Exponentiation by squaring:
  // https://en.wikipedia.org/wiki/Exponentiation_by_squaring;
  Value neg_one =
      lb.create<arith::ConstantOp>(lb.getIntegerAttr(result_type, -1));
  Value zero = lb.create<arith::ConstantOp>(lb.getIntegerAttr(result_type, 0));
  Value one = lb.create<arith::ConstantOp>(lb.getIntegerAttr(result_type, 1));
  Value two = lb.create<arith::ConstantOp>(lb.getIntegerAttr(result_type, 2));
  Value step = lb.create<arith::ConstantIndexOp>(1);
  Value lowerBound = lb.create<arith::ConstantIndexOp>(0);
  // Everything else would overflow for any exponent > 1, as 2^64
  // is the larget possible exponent for a 64-bit integer, and
  // that's 1 << 6.
  Value upperBound = lb.create<arith::ConstantIndexOp>(6);
  auto original_base = adaptor.lhs();
  auto original_exponent = adaptor.rhs();

  Value accum =
      lb.create<scf::ForOp>(
            lowerBound, upperBound, step,
            SmallVector<Value>({one, original_base, original_exponent}),
            [&](OpBuilder& b, Location, Value /*v*/, ValueRange iters) {
              Value accum = iters[0];
              Value base = iters[1];
              Value exponent = iters[2];

              Value condition = b.create<arith::CmpIOp>(
                  loc, arith::CmpIPredicate::eq,
                  b.create<::mlir::arith::AndIOp>(loc, exponent, one), one);
              Value multiplied =
                  b.create<::mlir::arith::MulIOp>(loc, accum, base);
              accum = b.create<::mlir::arith::SelectOp>(loc, condition,
                                                        multiplied, accum);
              base = b.create<::mlir::arith::MulIOp>(loc, base, base);
              exponent = b.create<::mlir::arith::ShRUIOp>(loc, exponent, one);
              b.create<scf::YieldOp>(
                  loc, SmallVector<Value>({accum, base, exponent}));
            })
          .getResult(0);

  Value rhs_is_even = lb.create<arith::CmpIOp>(
      arith::CmpIPredicate::eq, lb.create<arith::RemSIOp>(adaptor.rhs(), two),
      zero);
  Value rhs_is_negative =
      lb.create<arith::CmpIOp>(arith::CmpIPredicate::slt, adaptor.rhs(), zero);
  Value lhs_is_one =
      lb.create<arith::CmpIOp>(arith::CmpIPredicate::eq, adaptor.lhs(), one);
  Value lhs_is_neg_one = lb.create<arith::CmpIOp>(arith::CmpIPredicate::eq,
                                                  adaptor.lhs(), neg_one);

  // The accum is correct when the rhs is non-negative. When rhs is
  // negative, we return 0 for integer, with the exception of lhs values of 1
  // and -1 which have integer results for negative exponents. Specifically, the
  // calulation is the following:
  //
  // - Return accum if the rhs is not negative.
  // - Return 1 or -1 depending on the parity of rhs when the lhs is -1.
  // - Return 1 if lhs is 1.
  // - Else return 0.
  Value if_lhs_is_one =
      lb.create<::mlir::arith::SelectOp>(lhs_is_one, one, zero);
  Value if_lhs_is_neg_one = lb.create<::mlir::arith::SelectOp>(
      lhs_is_neg_one,
      lb.create<::mlir::arith::SelectOp>(rhs_is_even, one, neg_one),
      if_lhs_is_one);
  return lb.create<::mlir::arith::SelectOp>(rhs_is_negative, if_lhs_is_neg_one,
                                            accum);
}

template <>
inline Value MapMhloOpToStdScalarOp<mhlo::RoundOp>(
    Location loc, ArrayRef<Type> /*result_types*/, ArrayRef<Type> /*arg_types*/,
    ValueRange args, OpBuilder* b) {
  mhlo::RoundOp::Adaptor adaptor(args);
  auto lb = ImplicitLocOpBuilder(loc, *b);
  auto operand = adaptor.operand();
  auto operand_ty = operand.getType();
  auto element_ty = getElementTypeOrSelf(operand_ty);

  if (auto float_type = element_ty.dyn_cast<FloatType>()) {
    Value half =
        b->create<arith::ConstantOp>(loc, b->getFloatAttr(float_type, 0.5));
    auto abs = lb.create<math::AbsOp>(operand_ty, operand);
    auto add = lb.create<arith::AddFOp>(abs, half);
    auto floor = lb.create<math::FloorOp>(add);
    return lb.create<mlir::math::CopySignOp>(floor, operand);
  }

  return nullptr;
}

template <>
inline Value MapMhloOpToStdScalarOp<mhlo::SelectOp>(Location loc,
                                                    ArrayRef<Type> result_types,
                                                    ArrayRef<Type> arg_types,
                                                    ValueRange args,
                                                    OpBuilder* b) {
  return MapMhloOpToScalarOpImpl<::mlir::arith::SelectOp>{}(loc, result_types,
                                                            arg_types, args, b);
}

template <>
inline Value MapMhloOpToStdScalarOp<mhlo::SignOp>(Location loc,
                                                  ArrayRef<Type> result_types,
                                                  ArrayRef<Type> /*arg_types*/,
                                                  ValueRange args,
                                                  OpBuilder* b) {
  Type element_type = getElementTypeOrSelf(args.front().getType());
  if (auto float_type = element_type.dyn_cast<FloatType>()) {
    bool ignored;
    APFloat zero_apfloat(0.0f);
    zero_apfloat.convert(float_type.getFloatSemantics(),
                         APFloat::rmNearestTiesToEven, &ignored);
    Value zero =
        b->create<mlir::arith::ConstantFloatOp>(loc, zero_apfloat, float_type);
    if (VectorType vec_type = args.front().getType().dyn_cast<VectorType>()) {
      zero = b->create<::mlir::vector::SplatOp>(loc, vec_type, zero);
    }
    Value ne0_i1 = b->create<::mlir::arith::CmpFOp>(
        loc, arith::CmpFPredicate::ONE, args[0], zero);
    Value ne0_float =
        b->create<::mlir::arith::UIToFPOp>(loc, zero.getType(), ne0_i1);
    Value copy_sign = b->create<::mlir::math::CopySignOp>(loc, result_types,
                                                          ne0_float, args[0]);
    auto is_nan = b->create<::mlir::arith::CmpFOp>(
        loc, arith::CmpFPredicate::UNO, args[0], args[0]);
    return b->create<::mlir::arith::SelectOp>(loc, is_nan, args[0], copy_sign);
  }
  if (auto integer_type = element_type.dyn_cast<IntegerType>()) {
    // sign(x) = x == 0 ? 0 : ((x s>> 31) | 1)
    Value zero = b->create<::mlir::arith::ConstantIntOp>(
        loc, 0, integer_type.getWidth());
    Value bitwidth_minus_one = b->create<::mlir::arith::ConstantIntOp>(
        loc, integer_type.getWidth() - 1, integer_type.getWidth());
    Value one = b->create<::mlir::arith::ConstantIntOp>(
        loc, 1, integer_type.getWidth());
    if (VectorType vec_type = args.front().getType().dyn_cast<VectorType>()) {
      zero = b->create<::mlir::vector::SplatOp>(loc, vec_type, zero);
      bitwidth_minus_one =
          b->create<::mlir::vector::SplatOp>(loc, vec_type, bitwidth_minus_one);
      one = b->create<::mlir::vector::SplatOp>(loc, vec_type, one);
    }
    Value cmp = b->create<::mlir::arith::CmpIOp>(loc, arith::CmpIPredicate::eq,
                                                 args[0], zero);
    Value ashr =
        b->create<::mlir::arith::ShRSIOp>(loc, args[0], bitwidth_minus_one);
    Value or_op = b->create<::mlir::arith::OrIOp>(loc, ashr, one);
    return b->create<::mlir::arith::SelectOp>(loc, cmp, zero, or_op);
  } else if (element_type.isa<ComplexType>()) {
    return b->create<::mlir::complex::SignOp>(loc, element_type, args.front());
  }
  return nullptr;
}

}  // namespace impl

struct MhloOpToStdScalarOp {
  // Implementation for HLO ops except mhlo::CompareOp.
  template <typename MhloOpTy, typename = std::enable_if_t<!std::is_same<
                                   MhloOpTy, mhlo::CompareOp>::value>>
  static Value map(MhloOpTy op, ArrayRef<Type> result_types, ValueRange args,
                   OpBuilder* b) {
    return impl::MapMhloOpToStdScalarOp<MhloOpTy>(
        op.getLoc(), result_types, llvm::to_vector<4>(op->getOperandTypes()),
        args, b);
  }

  // Implementation for mhlo::CompareOp.
  template <typename MhloOpTy, typename = std::enable_if_t<std::is_same<
                                   MhloOpTy, mhlo::CompareOp>::value>>
  static Value map(mhlo::CompareOp op, ArrayRef<Type> result_types,
                   ValueRange args, OpBuilder* b) {
    auto comparison_direction = op.comparison_direction();
    return impl::MapCompareOpToStdScalarOp<mhlo::CompareOp>(
        op.getLoc(), comparison_direction, result_types,
        llvm::to_vector<4>(op->getOperandTypes()), args, b);
  }

  // Implementation for HLO ops except mhlo::CompareOp.
  template <typename MhloOpTy, typename = std::enable_if_t<!std::is_same<
                                   MhloOpTy, mhlo::CompareOp>::value>>
  static Value map(Location loc, ArrayRef<Type> result_types,
                   ArrayRef<Type> arg_types, ValueRange args, OpBuilder* b) {
    return impl::MapMhloOpToStdScalarOp<MhloOpTy>(loc, result_types, arg_types,
                                                  args, b);
  }

  // Implementation for lmhlo::CompareOp.
  template <typename MhloOpTy, typename = std::enable_if_t<std::is_same<
                                   MhloOpTy, mhlo::CompareOp>::value>>
  static Value map(Location loc, StringRef comparison_direction,
                   ArrayRef<Type> result_types, ArrayRef<Type> arg_types,
                   ValueRange args, OpBuilder* b) {
    return impl::MapCompareOpToStdScalarOp<mhlo::CompareOp>(
        loc, comparison_direction, result_types, arg_types, args, b);
  }
};

}  // namespace mhlo
}  // namespace mlir

#endif  // MLIR_HLO_DIALECT_MHLO_TRANSFORMS_MAP_MHLO_TO_SCALAR_OP_H
