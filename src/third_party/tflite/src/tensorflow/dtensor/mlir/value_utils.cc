/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/dtensor/mlir/value_utils.h"

#include "mlir/IR/BuiltinAttributes.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/transforms/collection_ops_util.h"
#include "tensorflow/dtensor/mlir/ir/tf_dtensor.h"
#include "tensorflow/dtensor/mlir/op_utils.h"

namespace tensorflow {
namespace dtensor {
namespace {

// Given a mlir::Value will trace the value back through
// DTensorLayout and basic blocks of while loops.
// This is like a reverse version of TraceUseToNextTFOp.
mlir::Value GetForwardedInput(mlir::Value value) {
  bool value_updated;
  do {
    value_updated = false;
    if (mlir::BlockArgument argument = value.dyn_cast<mlir::BlockArgument>()) {
      mlir::Region* region = argument.getParentRegion();
      if (region == nullptr) break;
      mlir::Operation* parent_op = region->getParentOp();
      // TODO(bfontain): handle if and other control flow blocks.
      if (mlir::TF::WhileRegionOp while_op =
              mlir::dyn_cast<mlir::TF::WhileRegionOp>(parent_op)) {
        value = while_op.getOperand(argument.getArgNumber());
        value_updated = true;
      }
    } else {
      mlir::Operation* op = value.getDefiningOp();
      // TODO(bfontain): Add cases for identity and control flow return values.
      if (mlir::TF::DTensorLayout layout_op =
              mlir::dyn_cast<mlir::TF::DTensorLayout>(op)) {
        value = layout_op.input();
        value_updated = true;
      }
    }
  } while (value_updated);

  return value;
}
}  // namespace

namespace ops_util = ::mlir::TF::collection_ops_util;

int ValueRank(mlir::Value operand_value) {
  mlir::Type type = GetSubtypeOrSelf(operand_value);
  const auto operand_type = type.cast<mlir::TensorType>();
  if (!operand_type.hasRank()) return -1;
  return operand_type.getRank();
}

mlir::RankedTensorType EffectivelyScalarR1Type(mlir::Type element_type) {
  return mlir::RankedTensorType::get({1}, element_type);
}

mlir::Value ReshapeSizeTypeToScalar(mlir::OpBuilder builder, mlir::Location loc,
                                    mlir::Value tensor) {
  auto scalar_type =
      mlir::RankedTensorType::get({}, builder.getIntegerType(32));
  mlir::Value scalar_shape =
      ops_util::GetR1Const(scalar_type.getShape(), builder, loc);
  return builder.create<mlir::TF::ReshapeOp>(
      loc, mlir::ArrayRef<mlir::Type>{scalar_type},
      mlir::ArrayRef<mlir::Value>{tensor, scalar_shape});
}

mlir::Value IntConst(mlir::OpBuilder& builder, mlir::Location loc,
                     llvm::ArrayRef<int32> values) {
  auto const_type = mlir::RankedTensorType::get(
      {static_cast<int64>(values.size())}, builder.getIntegerType(32));
  mlir::Attribute const_attr =
      mlir::DenseIntElementsAttr::get(const_type, values);
  return builder.create<mlir::TF::ConstOp>(loc, const_attr).getResult();
}

mlir::Value Int64Const(mlir::OpBuilder& builder, mlir::Location loc,
                       llvm::ArrayRef<int64_t> values) {
  auto const_type = mlir::RankedTensorType::get(
      {static_cast<int64>(values.size())}, builder.getIntegerType(64));
  mlir::Attribute const_attr =
      mlir::DenseIntElementsAttr::get(const_type, values);
  return builder.create<mlir::TF::ConstOp>(loc, const_attr).getResult();
}

mlir::Value FloatConst(mlir::OpBuilder& builder, mlir::Location loc,
                       llvm::ArrayRef<float> values) {
  mlir::RankedTensorType const_type = mlir::RankedTensorType::get(
      {static_cast<int64>(values.size())}, builder.getF32Type());
  mlir::Attribute const_attr =
      mlir::DenseFPElementsAttr::get(const_type, values);
  return builder.create<mlir::TF::ConstOp>(loc, const_attr).getResult();
}

mlir::Value StringConst(mlir::OpBuilder& builder, mlir::Location loc,
                        llvm::ArrayRef<llvm::StringRef> values) {
  auto const_type =
      mlir::RankedTensorType::get({static_cast<int64>(values.size())},
                                  builder.getType<mlir::TF::StringType>());
  mlir::Attribute const_attr =
      mlir::DenseStringElementsAttr::get(const_type, values);
  return builder.create<mlir::TF::ConstOp>(loc, const_attr).getResult();
}

StatusOr<int64_t> ExtractConstIntFromValue(mlir::Value value) {
  value = GetForwardedInput(value);
  if (value.isa<mlir::BlockArgument>())
    return errors::Internal("unable get constant value from block argument");
  mlir::DenseIntElementsAttr attr;
  if (!matchPattern(value, m_Constant(&attr))) {
    return errors::Internal(absl::StrCat("required constant value for ",
                                         OpName(value.getDefiningOp())));
  }
  if (attr.size() != 1) {
    return errors::Internal(absl::StrCat("expected 1 element, got ",
                                         attr.size(), " for ",
                                         OpName(value.getDefiningOp())));
  }
  auto a = *attr.value_begin<llvm::APInt>();
  return a.getSExtValue();
}

Status ExtractConstVectorFromValue(mlir::Value value,
                                   llvm::SmallVector<int64_t, 4>* out_vector) {
  value = GetForwardedInput(value);
  if (value.isa<mlir::BlockArgument>())
    return errors::Internal("unable get constant value from block argument");
  mlir::DenseIntElementsAttr attr;
  if (!matchPattern(value, m_Constant(&attr))) {
    return errors::Internal(
        absl::StrCat("failed to extract constant value from ",
                     value.getDefiningOp()->getName().getStringRef().str()));
  }
  for (const mlir::APInt& index : attr)
    out_vector->emplace_back(index.getSExtValue());
  return Status::OK();
}

mlir::Value CreateIntScalarConst(const int64_t value, mlir::OpBuilder builder,
                                 mlir::Location loc, bool use_int64) {
  if (use_int64) {
    return builder.create<mlir::TF::ConstOp>(
        loc, mlir::DenseIntElementsAttr::get(
                 mlir::RankedTensorType::get({}, builder.getI64Type()), value));
  } else {
    return builder.create<mlir::TF::ConstOp>(
        loc, mlir::DenseIntElementsAttr::get(
                 mlir::RankedTensorType::get({}, builder.getI32Type()),
                 static_cast<int32_t>(value)));
  }
}

absl::optional<mlir::Value> CreateZeroScalarConst(mlir::OpBuilder& builder,
                                                  mlir::Location loc,
                                                  mlir::Type type) {
  if (type.isF64()) {
    return builder.create<mlir::TF::ConstOp>(
        loc, mlir::DenseFPElementsAttr::get(
                 mlir::RankedTensorType::get({}, builder.getF64Type()),
                 static_cast<double>(0.)));
  } else if (type.isF32()) {
    return builder.create<mlir::TF::ConstOp>(
        loc, mlir::DenseFPElementsAttr::get(
                 mlir::RankedTensorType::get({}, builder.getF32Type()),
                 static_cast<float>(0.f)));
  } else if (type.isInteger(32)) {
    return builder.create<mlir::TF::ConstOp>(
        loc, mlir::DenseIntElementsAttr::get(
                 mlir::RankedTensorType::get({}, builder.getI32Type()),
                 static_cast<int32_t>(0)));
  } else if (type.isInteger(64)) {
    return builder.create<mlir::TF::ConstOp>(
        loc, mlir::DenseIntElementsAttr::get(
                 mlir::RankedTensorType::get({}, builder.getI64Type()),
                 static_cast<int64_t>(0)));
  } else {
    return absl::nullopt;
  }
}

mlir::Type GetSubtypeOrSelf(mlir::Value val) {
  mlir::Type type = val.getType();
  if (auto type_with_subtype =
          mlir::getElementTypeOrSelf(val)
              .dyn_cast<mlir::TF::TensorFlowTypeWithSubtype>()) {
    if (type_with_subtype.GetSubtypes().size() == 1) {
      type = type_with_subtype.GetSubtypes().front();
    }
  }
  return type;
}

}  // namespace dtensor
}  // namespace tensorflow
