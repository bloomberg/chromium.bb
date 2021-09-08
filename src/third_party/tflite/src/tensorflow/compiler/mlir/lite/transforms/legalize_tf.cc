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

// This transformation pass converts operations in TensorFlow dialect into
// operations that are legal in the TensorFlow Lite dialect.  Operations that
// can be legalized to TensorFlow Lite dialect with simple replacements are part
// of this pass and other operations that may create extra ops should be part of
// the PrepareTF pass which should be run before this pass.  That way any
// constant folding opportunities from the extra ops can be exploited by the
// constant folding support for the TensorFlow ops.

#include <climits>
#include <complex>
#include <cstdint>

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/Threading.h"
#include "mlir/Dialect/Quant/FakeQuantSupport.h"  // from @llvm-project
#include "mlir/Dialect/Quant/UniformSupport.h"  // from @llvm-project
#include "mlir/IR/Attributes.h"  // from @llvm-project
#include "mlir/IR/Diagnostics.h"  // from @llvm-project
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "mlir/IR/Operation.h"  // from @llvm-project
#include "mlir/IR/PatternMatch.h"  // from @llvm-project
#include "mlir/IR/StandardTypes.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "mlir/Support/LLVM.h"  // from @llvm-project
#include "mlir/Support/LogicalResult.h"  // from @llvm-project
#include "mlir/Transforms/DialectConversion.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/lite/ir/tfl_ops.h"
#include "tensorflow/compiler/mlir/lite/quantization/quantization_utils.h"
#include "tensorflow/compiler/mlir/lite/transforms/passes.h"
#include "tensorflow/compiler/mlir/lite/utils/attribute_utils.h"
#include "tensorflow/compiler/mlir/lite/utils/validators.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.h"
#include "tensorflow/compiler/mlir/tensorflow/utils/mangling_util.h"
#include "tensorflow/compiler/xla/status.h"
#include "tensorflow/compiler/xla/statusor.h"
#include "tensorflow/core/framework/tensor.pb.h"
#include "tensorflow/core/framework/tensor_shape.pb.h"
#include "tensorflow/core/framework/types.pb.h"
#include "tensorflow/core/lib/random/philox_random.h"
#include "tensorflow/core/lib/random/random_distributions.h"
#include "tensorflow/core/protobuf/error_codes.pb.h"

namespace mlir {
namespace TFL {

//===----------------------------------------------------------------------===//
// The actual LegalizeTF Pass.
namespace {

using xla::Status;
using xla::StatusOr;

constexpr char kUnidirectionalSequenceLstm[] = "tf.UnidirectionalSequenceLstm";
constexpr char kUnidirectionalSequenceRnn[] = "tf.UnidirectionalSequenceRnn";
constexpr char kTfLiteInputIndices[] = "_tflite_input_indices";

// Legalize operations in functions.
class LegalizeTF : public PassWrapper<LegalizeTF, FunctionPass> {
 public:
  LegalizeTF() = default;
  LegalizeTF(const LegalizeTF&) {}
  explicit LegalizeTF(bool run_tfl_runtime_verification) {
    run_tfl_runtime_verification_ = run_tfl_runtime_verification;
  }

  /// Performs the lowering to TFLite dialect.
  void runOnFunction() override;

 private:
  Option<bool> run_tfl_runtime_verification_{
      *this, "run-tfl-runtime-verification",
      llvm::cl::desc("Allow tfl runtime verification."), llvm::cl::init(true)};
};

// Returns true if all tensor value in `values` has static shape and same shape.
bool HasSameStaticShapes(Operation* op) {
  auto values = op->getOperands();
  int index = 0;
  ArrayRef<int64_t> shape;
  for (Value value : values) {
    auto shaped_type = value.getType().dyn_cast<ShapedType>();
    if (!shaped_type || !shaped_type.hasStaticShape()) {
      return false;
    }
    if (index == 0) {
      shape = shaped_type.getShape();
    } else {
      if (shape != shaped_type.getShape()) {
        return false;
      }
    }
    ++index;
  }
  return true;
}

#include "tensorflow/compiler/mlir/lite/transforms/generated_legalize_tf.inc"

#define DECL_CONVERT_OP(tf_op)                                               \
  struct ConvertTF##tf_op##Op : public RewritePattern {                      \
    explicit ConvertTF##tf_op##Op(MLIRContext* context)                      \
        : RewritePattern(TF::tf_op##Op::getOperationName(), 1, context) {}   \
    LogicalResult matchAndRewrite(Operation* op,                             \
                                  PatternRewriter& rewriter) const override; \
  }

// TODO(antiagainst): Define this pattern in a table-driven manner once variadic
// operands are properly supported in declarative rewrite rule specification.

DECL_CONVERT_OP(Assert);
DECL_CONVERT_OP(ConcatV2);
DECL_CONVERT_OP(MatMul);
DECL_CONVERT_OP(MatrixDiagV2);
DECL_CONVERT_OP(MatrixDiagV3);
DECL_CONVERT_OP(Pack);
DECL_CONVERT_OP(Reshape);
DECL_CONVERT_OP(Split);
DECL_CONVERT_OP(SplitV);
DECL_CONVERT_OP(StridedSlice);
DECL_CONVERT_OP(Unpack);
DECL_CONVERT_OP(Reciprocal);
DECL_CONVERT_OP(RandomUniform);
DECL_CONVERT_OP(BroadcastTo);

#undef DECL_CONVERT_OP

LogicalResult ConvertTFRandomUniformOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto random_uniform_op = cast<TF::RandomUniformOp>(op);
  if (random_uniform_op.seed() == 0 && random_uniform_op.seed2() == 0) {
    return failure();
  }
  if (!random_uniform_op.dtype().isF32()) {
    return failure();
  }
  typedef tensorflow::random::UniformDistribution<
      tensorflow::random::PhiloxRandom, float>
      Distribution;

  tensorflow::random::PhiloxRandom generator(
      random_uniform_op.seed().getSExtValue(),
      random_uniform_op.seed2().getSExtValue());
  Distribution dist;
  int num_elements = 0;
  if (auto output_type =
          random_uniform_op.output().getType().dyn_cast_or_null<ShapedType>()) {
    if (auto ranked_output = output_type.dyn_cast_or_null<RankedTensorType>()) {
      if (!ranked_output.hasRank() || ranked_output.getNumDynamicDims() != 0) {
        return failure();
      }
      num_elements = output_type.getNumElements();
      size_t offset = 0;
      size_t num_samples = Distribution::kResultElementCount;
      llvm::SmallVector<float, 32> data;
      data.resize(num_elements);
      while (offset < num_elements) {
        const typename Distribution::ResultType samples = dist(&generator);
        std::copy(&samples[0],
                  &samples[0] + std::min(num_samples, data.size() - offset),
                  &data[0] + offset);
        offset += num_samples;
      }
      auto output_data = DenseFPElementsAttr::get(output_type, data);
      rewriter.replaceOpWithNewOp<ConstantOp>(op, output_type, output_data);
      return success();
    }
  }
  return failure();
}

// Converts any IntegerAttr to an IntegerAttr of an i32 type.
// The value won't change in the new attribute, but if the value is out of
// the bound of i32, the function returns a failure.
LogicalResult ConvertToI32Attr(IntegerAttr attr, IntegerAttr* attr_i32) {
  if (attr.getType().isInteger(/*width=*/32)) {
    *attr_i32 = attr;
    return success();
  }

  int64_t value = attr.getInt();
  if (value > std::numeric_limits<int>::max() ||
      value < std::numeric_limits<int>::min()) {
    return failure();
  }

  *attr_i32 = IntegerAttr::get(
      IntegerType::get(/*width=*/32, attr.getContext()), value);
  return success();
}

LogicalResult ConvertTFConcatV2Op::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tf_concat_op = cast<TF::ConcatV2Op>(op);

  auto values = tf_concat_op.values();
  auto output_type = tf_concat_op.output().getType();
  // Extract axis attribute from constant axis tensor
  ElementsAttr axis;
  if (!matchPattern(tf_concat_op.axis(), m_Constant(&axis))) return failure();
  IntegerAttr axis_int = ExtractSingleElementAsInteger(axis);

  // "axis" operand could be a i64 tensor. Resolve it here.
  IntegerAttr axis_i32;
  if (failed(ConvertToI32Attr(axis_int, &axis_i32))) return failure();

  StringAttr fused_activation_function =
      StringAttr::get("NONE", rewriter.getContext());
  rewriter.replaceOpWithNewOp<ConcatenationOp>(
      op, output_type, values, axis_i32, fused_activation_function);
  return success();
}

// The following is effectively:
// def : Pat<
//   (TF_MatMulOp $a, $b, ConstBoolAttrFalse:$transpose_a,
//      ConstBoolAttrTrue:$transpose_b),
//   (TFL_FullyConnectedOp:$__0 $a, $b,
//     NoInput.pattern, TFL_AF_None, TFL_FCWO_Default, ConstBoolAttrFalse)>;
LogicalResult ConvertTFMatMulOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tf_matmul_op = cast<TF::MatMulOp>(op);
  if (tf_matmul_op.transpose_a()) return failure();
  if (!tf_matmul_op.transpose_b()) return failure();

  Type output_type = tf_matmul_op.getResult().getType();
  // TODO(jpienaar): Follow up post shuffle discussion.
  auto no_input = rewriter.create<ConstantOp>(
      op->getLoc(), rewriter.getNoneType(), rewriter.getUnitAttr());
  auto fc_op = rewriter.create<FullyConnectedOp>(
      op->getLoc(), ArrayRef<Type>{output_type}, op->getOperand(0),
      op->getOperand(1), no_input, rewriter.getStringAttr("NONE"),
      rewriter.getStringAttr("DEFAULT"), rewriter.getBoolAttr(false));
  rewriter.replaceOp(op, {fc_op.getResult(0)});
  return success();
}

LogicalResult ConvertTFPackOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tf_pack_op = cast<TF::PackOp>(op);

  SmallVector<Value, 4> values(tf_pack_op.values());
  auto output_type = tf_pack_op.output().getType();
  auto values_count = rewriter.getI32IntegerAttr(tf_pack_op.N());
  // Axis can be negative.
  auto axis = rewriter.getI32IntegerAttr(tf_pack_op.axis().getSExtValue());

  rewriter.replaceOpWithNewOp<PackOp>(op, output_type, values, values_count,
                                      axis);
  return success();
}

LogicalResult ConvertTFReshapeOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tf_reshape_op = cast<TF::ReshapeOp>(op);

  auto input = tf_reshape_op.tensor();
  auto shape = tf_reshape_op.shape();

  ShapedType shape_type = shape.getType().cast<ShapedType>();
  // The tfl reshape's #2 operand needs to i32 tensor type, so we have to cast.
  if (!shape_type.getElementType().isSignlessInteger(32)) {
    auto new_shape = shape_type.getShape();
    IntegerType new_ele_type = rewriter.getIntegerType(32);
    ShapedType new_type = RankedTensorType::get(new_shape, new_ele_type);
    // Uses TF::CastOp to be folded if the shape input is a constant.
    shape = rewriter
                .create<TF::CastOp>(op->getLoc(), new_type, shape,
                                    rewriter.getBoolAttr(false))
                .y();
  }
  rewriter.replaceOpWithNewOp<ReshapeOp>(op, tf_reshape_op.output().getType(),
                                         input, shape);
  return success();
}

LogicalResult ConvertTFSplitOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tf_split_op = cast<TF::SplitOp>(op);

  // Number of splits cannot be negative.
  auto num_split = rewriter.getI32IntegerAttr(tf_split_op.num_split());

  rewriter.replaceOpWithNewOp<TFL::SplitOp>(op, tf_split_op.output().getTypes(),
                                            tf_split_op.split_dim(),
                                            tf_split_op.value(), num_split);
  return success();
}

LogicalResult ConvertTFSplitVOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tf_splitv_op = cast<TF::SplitVOp>(op);

  // Number of splits cannot be negative.
  auto num_split = rewriter.getI32IntegerAttr(tf_splitv_op.num_split());

  rewriter.replaceOpWithNewOp<TFL::SplitVOp>(
      op, tf_splitv_op.output().getTypes(), tf_splitv_op.value(),
      tf_splitv_op.size_splits(), tf_splitv_op.split_dim(), num_split);
  return success();
}

Value PadStridedSliceAttributeArray(Operation* op, PatternRewriter& rewriter,
                                    Value attribute,
                                    ArrayRef<int32_t> padding_val, int* mask) {
  DenseIntElementsAttr dense_elem_attr;
  SmallVector<int32_t, 8> padded_val;

  auto ranked_attr_type = attribute.getType().dyn_cast<RankedTensorType>();
  if (!ranked_attr_type ||
      !matchPattern(attribute, m_Constant(&dense_elem_attr))) {
    // If the input attribute is neither ranked type nor constant, we
    // can't do any padding. Instead we just return it.
    return attribute;
  }
  for (const auto& idx : dense_elem_attr.getIntValues()) {
    padded_val.push_back(idx.getSExtValue());
  }
  auto attr_dim_count = ranked_attr_type.getShape()[0];
  int full_dim_count = padding_val.size();
  for (int i = attr_dim_count; i < full_dim_count; ++i) {
    padded_val.push_back(padding_val[i]);
    if (mask) *mask |= 1 << i;
  }
  auto type =
      RankedTensorType::get({full_dim_count}, rewriter.getIntegerType(32));
  auto attr = DenseElementsAttr::get<int32_t>(type, padded_val);
  return rewriter.create<ConstantOp>(op->getLoc(), type, attr);
}

LogicalResult ConvertTFStridedSliceOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tf_strided_slice_op = cast<TF::StridedSliceOp>(op);
  auto ranked_input_type =
      tf_strided_slice_op.input().getType().dyn_cast<RankedTensorType>();
  if (!ranked_input_type) {
    // If input is not a ranked tensor, we can't deduce the padding dimensions
    // from it, so we just do a plain conversion here.
    rewriter.replaceOpWithNewOp<TFL::StridedSliceOp>(
        op, tf_strided_slice_op.output().getType(), tf_strided_slice_op.input(),
        tf_strided_slice_op.begin(), tf_strided_slice_op.end(),
        tf_strided_slice_op.strides(),
        rewriter.getI32IntegerAttr(
            tf_strided_slice_op.begin_mask().getSExtValue()),
        rewriter.getI32IntegerAttr(
            tf_strided_slice_op.end_mask().getSExtValue()),
        rewriter.getI32IntegerAttr(
            tf_strided_slice_op.ellipsis_mask().getSExtValue()),
        rewriter.getI32IntegerAttr(
            tf_strided_slice_op.new_axis_mask().getSExtValue()),
        rewriter.getI32IntegerAttr(
            tf_strided_slice_op.shrink_axis_mask().getSExtValue()));
    return success();
  }

  int num_input_dims = ranked_input_type.getRank();
  // Pad `begin` array with zero values and update the `begin_mask`.
  SmallVector<int32_t, 8> begin_pad_val(num_input_dims, 0);
  int begin_mask = tf_strided_slice_op.begin_mask().getSExtValue();
  Value padded_begin = PadStridedSliceAttributeArray(
      op, rewriter, tf_strided_slice_op.begin(), begin_pad_val, &begin_mask);
  // Pad `end` array with `input_shape` and update the `end_mask`.
  int end_mask = tf_strided_slice_op.end_mask().getSExtValue();
  auto input_shape = ranked_input_type.getShape();
  SmallVector<int32_t, 8> end_pad_val(input_shape.begin(), input_shape.end());
  Value padded_end = PadStridedSliceAttributeArray(
      op, rewriter, tf_strided_slice_op.end(), end_pad_val, &end_mask);
  // Pad `strides` array with ones.
  SmallVector<int32_t, 8> strides_pad_val(num_input_dims, 1);
  Value padded_strides = PadStridedSliceAttributeArray(
      op, rewriter, tf_strided_slice_op.strides(), strides_pad_val, nullptr);
  rewriter.replaceOpWithNewOp<TFL::StridedSliceOp>(
      op, tf_strided_slice_op.output().getType(), tf_strided_slice_op.input(),
      padded_begin, padded_end, padded_strides,
      rewriter.getI32IntegerAttr(begin_mask),
      rewriter.getI32IntegerAttr(end_mask),
      rewriter.getI32IntegerAttr(
          tf_strided_slice_op.ellipsis_mask().getSExtValue()),
      rewriter.getI32IntegerAttr(
          tf_strided_slice_op.new_axis_mask().getSExtValue()),
      rewriter.getI32IntegerAttr(
          tf_strided_slice_op.shrink_axis_mask().getSExtValue()));
  return success();
}

LogicalResult ConvertTFUnpackOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tf_unpack_op = cast<TF::UnpackOp>(op);

  auto input = tf_unpack_op.value();
  auto num = rewriter.getI32IntegerAttr(tf_unpack_op.num());
  // Axis can be negative.
  auto axis = rewriter.getI32IntegerAttr(tf_unpack_op.axis().getSExtValue());

  rewriter.replaceOpWithNewOp<UnpackOp>(op, tf_unpack_op.output().getTypes(),
                                        input, num, axis);
  return success();
}

// MatrixDiagV3 is MatrixDiagV2 with an alignment attribute. This attribute
// only has effects when processing multiple diagonals. Since TFLite converts
// MatrixDiagV{2,3} to MatrixDiag, which only takes single-diagonal inputs, we
// can safely ignore this V3 attribute.
// We can't pass `rewriter` by reference because clang-tidy will want it to be
// constant (`const PatternRewriter& rewriter`). If we do that, we won't be able
// to call `rewriter::replaceOpWihNewOp`, which is not a const member function.
template <typename MatrixDiagV2OrV3Op>
bool ConvertTFMatrixDiagV2orV3(Operation* op, PatternRewriter* rewriter) {
  auto tf_matrix_diag_v2_or_v3_op = cast<MatrixDiagV2OrV3Op>(op);

  if (tf_matrix_diag_v2_or_v3_op.getNumOperands() != 5) return false;

  auto input = tf_matrix_diag_v2_or_v3_op.diagonal();
  auto output_type = tf_matrix_diag_v2_or_v3_op.output().getType();

  // Extract k constant tensor and check value = 0.
  ElementsAttr k;
  if (!matchPattern(tf_matrix_diag_v2_or_v3_op.k(), m_Constant(&k)))
    return false;
  if (ExtractSingleElementAsInteger(k).getInt() != 0) return false;

  // Extract num_rows constant tensor and check value = -1.
  ElementsAttr num_rows;
  if (!matchPattern(tf_matrix_diag_v2_or_v3_op.num_rows(),
                    m_Constant(&num_rows)))
    return false;
  if (ExtractSingleElementAsInteger(num_rows).getInt() != -1) return false;

  // Extract num_cols constant tensor and check value = -1.
  ElementsAttr num_cols;
  if (!matchPattern(tf_matrix_diag_v2_or_v3_op.num_cols(),
                    m_Constant(&num_cols)))
    return false;
  if (ExtractSingleElementAsInteger(num_cols).getInt() != -1) return false;

  // Verify padding_value is an integer tensor with all 0s.
  ElementsAttr padding_value;
  if (!matchPattern(tf_matrix_diag_v2_or_v3_op.padding_value(),
                    m_Constant(&padding_value)))
    return false;
  for (const auto& value : padding_value.getValues<APInt>()) {
    if (value != 0) return false;
  }

  rewriter->replaceOpWithNewOp<MatrixDiagOp>(op, output_type, input);
  return true;
}

LogicalResult ConvertTFMatrixDiagV2Op::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  if (ConvertTFMatrixDiagV2orV3<TF::MatrixDiagV2Op>(op, &rewriter))
    return success();
  return failure();
}

LogicalResult ConvertTFMatrixDiagV3Op::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  if (ConvertTFMatrixDiagV2orV3<TF::MatrixDiagV3Op>(op, &rewriter))
    return success();
  return failure();
}

// TF Lite doesn't support Assert, we just drop the assert from the graph.
LogicalResult ConvertTFAssertOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  rewriter.eraseOp(op);
  return success();
}

StatusOr<ConstantOp> CreateConstOpWithSingleValue(PatternRewriter* rewriter,
                                                  Location loc,
                                                  ShapedType shaped_type,
                                                  int value) {
  Type element_type = shaped_type.getElementType();
  ShapedType scalar_type = RankedTensorType::get({}, element_type);
  Attribute attr;
  switch (element_type.getKind()) {
    case mlir::StandardTypes::F16: {
      auto floatType = mlir::FloatType::getF16(element_type.getContext());
      auto floatAttr =
          mlir::FloatAttr::get(floatType, static_cast<float>(value));
      std::vector<Attribute> floatValues({floatAttr});
      attr = DenseElementsAttr::get(scalar_type, floatValues);
      break;
    }
    case mlir::StandardTypes::BF16: {
      auto floatType = mlir::FloatType::getBF16(element_type.getContext());
      auto floatAttr =
          mlir::FloatAttr::get(floatType, static_cast<float>(value));
      std::vector<Attribute> floatValues({floatAttr});
      attr = DenseElementsAttr::get(scalar_type, floatValues);
      break;
    }
    case mlir::StandardTypes::F32: {
      attr =
          DenseElementsAttr::get<float>(scalar_type, static_cast<float>(value));
      break;
    }
    case mlir::StandardTypes::Complex: {
      auto etype = element_type.cast<mlir::ComplexType>().getElementType();
      if (etype.isF32()) {
        auto dialect = etype.getContext()->getRegisteredDialect("tf");
        tensorflow::TensorProto repr;
        repr.set_dtype(tensorflow::DT_COMPLEX64);

        tensorflow::TensorShapeProto* shape = repr.mutable_tensor_shape();
        shape->set_unknown_rank(false);
        shape->add_dim()->set_size(int64_t{1});
        std::string content;
        auto complex_value =
            std::complex<float>(static_cast<float>(value), 0.0f);
        content.assign(reinterpret_cast<const char*>(&complex_value),
                       sizeof(complex_value));
        repr.set_tensor_content(content);
        std::string mangled = tensorflow::mangling_util::MangleTensor(repr);

        attr = mlir::OpaqueElementsAttr::get(dialect, scalar_type, mangled);
        break;
      }
      return Status(tensorflow::error::INVALID_ARGUMENT, "Unsupported type");
    }
    case mlir::StandardTypes::Integer: {
      const auto& itype = element_type.cast<mlir::IntegerType>();
      switch (itype.getWidth()) {
        case 8:
          attr = DenseElementsAttr::get<int8_t>(scalar_type,
                                                static_cast<int8_t>(value));
          break;
        case 16:
          attr = DenseElementsAttr::get<int16_t>(scalar_type,
                                                 static_cast<int16_t>(value));
          break;
        case 32:
          attr = DenseElementsAttr::get<int32_t>(scalar_type,
                                                 static_cast<int32_t>(value));
          break;
        case 64:
          attr = DenseElementsAttr::get<int64_t>(scalar_type,
                                                 static_cast<int64_t>(value));
          break;
        default:
          return Status(tensorflow::error::INVALID_ARGUMENT,
                        "Unsupported type");
      }
      break;
    }
    default:
      return Status(tensorflow::error::INVALID_ARGUMENT, "Unsupported type");
  }
  return rewriter->create<ConstantOp>(loc, scalar_type, attr);
}

LogicalResult ConvertTFReciprocalOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tf_reciprocal_op = cast<TF::ReciprocalOp>(op);

  auto status_or_const_op = CreateConstOpWithSingleValue(
      &rewriter, op->getLoc(),
      tf_reciprocal_op.x().getType().cast<ShapedType>(), 1);
  if (!status_or_const_op.ok()) {
    return failure();
  }

  StringAttr fused_activation_function =
      StringAttr::get("NONE", rewriter.getContext());

  rewriter.replaceOpWithNewOp<TFL::DivOp>(op, status_or_const_op.ValueOrDie(),
                                          tf_reciprocal_op.x(),
                                          fused_activation_function);
  return success();
}

LogicalResult ConvertTFBroadcastToOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tf_broadcast_to_op = cast<TF::BroadcastToOp>(op);
  auto element_type = tf_broadcast_to_op.input().getType().cast<ShapedType>();
  auto output_type = tf_broadcast_to_op.output().getType();

  auto status_or_const_op =
      CreateConstOpWithSingleValue(&rewriter, op->getLoc(), element_type, 1);
  if (!status_or_const_op.ok()) {
    return failure();
  }

  auto tfl_fill_op = rewriter.create<TFL::FillOp>(
      op->getLoc(), output_type, tf_broadcast_to_op.shape(),
      status_or_const_op.ValueOrDie());

  StringAttr fused_activation_function =
      StringAttr::get("NONE", rewriter.getContext());

  rewriter.replaceOpWithNewOp<TFL::MulOp>(
      op, output_type, tf_broadcast_to_op.input(), tfl_fill_op,
      fused_activation_function);
  return success();
}

// Legalize unidirectional sequence lstm.
struct LegalizeUnidirectionalSequenceLstm : public RewritePattern {
  explicit LegalizeUnidirectionalSequenceLstm(MLIRContext* context)
      : RewritePattern(kUnidirectionalSequenceLstm, 1, context) {}

  LogicalResult matchAndRewrite(Operation* op,
                                PatternRewriter& rewriter) const override {
    auto tflite_indices_attr =
        op->getAttrOfType<ArrayAttr>(kTfLiteInputIndices);
    if (!tflite_indices_attr) return failure();

    SmallVector<int64_t, 20> tflite_indices;
    for (auto index_attr : tflite_indices_attr.getValue()) {
      IntegerAttr index = index_attr.cast<IntegerAttr>();
      tflite_indices.push_back(index.getInt());
    }

    // Optional input placeholder.
    Value none = rewriter.create<mlir::ConstantOp>(
        op->getLoc(), rewriter.getNoneType(), rewriter.getUnitAttr());

    // Populate inputs.
    // UnidirectionalSequenceLstm is expected to have 24 inputs.
    SmallVector<Value, 24> inputs;
    int count = 0;
    int total_ophint_converted_inputs = tflite_indices.size();
    for (int i = 0; i < 24; ++i) {
      if (count < total_ophint_converted_inputs && tflite_indices[count] == i) {
        // specified input.
        inputs.push_back(op->getOperand(i));
        count++;
      } else {
        // Non specified input.
        inputs.push_back(none);
      }
    }

    // Populate outputs.
    // UnidirectionalSequenceLstm should only have 1 output, and that is the
    // original ophint converted node's 3rd output.
    SmallVector<Type, 4> result_types;
    result_types.push_back(op->getOpResult(2).getType());

    // Populate attributes.
    SmallVector<NamedAttribute, 4> attributes;
    // Activation will always be tanh.
    attributes.push_back(rewriter.getNamedAttr("fused_activation_function",
                                               rewriter.getStringAttr("TANH")));
    // cell_clip.
    attributes.push_back(
        rewriter.getNamedAttr("cell_clip", rewriter.getF32FloatAttr(0.0)));
    // proj_clip.
    attributes.push_back(
        rewriter.getNamedAttr("proj_clip", rewriter.getF32FloatAttr(0.0)));
    // will always be time_majored.
    attributes.push_back(
        rewriter.getNamedAttr("time_major", rewriter.getBoolAttr(true)));

    auto lstm_op = rewriter.create<TFL::UnidirectionalSequenceLSTMOp>(
        op->getLoc(), result_types, inputs, attributes);

    // Rewire the output.
    op->getResult(2).replaceAllUsesWith(lstm_op.getResult());
    rewriter.eraseOp(op);
    return success();
  }
};

// Legalize unidirectional seqeucen rnn.
struct LegalizeUnidirectionalSequenceRnn : public RewritePattern {
  explicit LegalizeUnidirectionalSequenceRnn(MLIRContext* context)
      : RewritePattern(kUnidirectionalSequenceRnn, 1, context) {}

  LogicalResult matchAndRewrite(Operation* op,
                                PatternRewriter& rewriter) const override {
    auto tflite_indices_attr =
        op->getAttrOfType<ArrayAttr>(kTfLiteInputIndices);
    if (!tflite_indices_attr) return failure();

    if (op->getNumOperands() != 5) {
      op->emitError()
          << "We're expecting 5 inputs for UnidirectionalSequenceRNN, only "
          << op->getNumOperands() << " provided";
      return failure();
    }

    if (op->getNumResults() != 2) {
      op->emitError()
          << "We're expecting 2 inputs for UnidirectionalSequenceRNN, only "
          << op->getNumResults() << " found";
      return failure();
    }

    // Populate inputs.
    // UnidirectionalSequenceRnn is expected to have 5 inputs, and none of them
    // are optional inputs.
    SmallVector<Value, 5> inputs;
    for (int i = 0; i < 5; ++i) {
      inputs.push_back(op->getOperand(i));
    }

    // Populate outputs.
    // UnidirectionalSequenceRnn should only have 1 output, and that is the
    // original ophint converted node's 2nd output.
    SmallVector<Type, 4> result_types;
    result_types.push_back(op->getOpResult(1).getType());

    // Populate attributes.
    SmallVector<NamedAttribute, 2> attributes;
    // Activation will always be tanh.
    attributes.push_back(rewriter.getNamedAttr("fused_activation_function",
                                               rewriter.getStringAttr("TANH")));

    // will always be time_majored.
    attributes.push_back(
        rewriter.getNamedAttr("time_major", rewriter.getBoolAttr(true)));

    auto rnn_op = rewriter.create<TFL::UnidirectionalSequenceRNNOp>(
        op->getLoc(), result_types, inputs, attributes);

    // Rewire the output.
    op->getResult(1).replaceAllUsesWith(rnn_op.getResult());
    rewriter.eraseOp(op);

    return success();
  }
};

void LegalizeTF::runOnFunction() {
  OwningRewritePatternList patterns;
  auto* context = &getContext();
  auto func = getFunction();

  // Add the generated patterns to the list.
  populateWithGenerated(context, &patterns);
  patterns
      .insert<ConvertTFConcatV2Op, ConvertTFMatMulOp, ConvertTFMatrixDiagV2Op,
              ConvertTFMatrixDiagV3Op, ConvertTFPackOp, ConvertTFReshapeOp,
              ConvertTFSplitOp, ConvertTFSplitVOp, ConvertTFStridedSliceOp,
              ConvertTFUnpackOp, ConvertTFAssertOp, ConvertTFReciprocalOp,
              ConvertTFRandomUniformOp, ConvertTFBroadcastToOp>(context);

  // Ophint python converter converted tf node pattern.
  patterns.insert<LegalizeUnidirectionalSequenceLstm,
                  LegalizeUnidirectionalSequenceRnn>(context);

  ConversionTarget target(*context);
  // It is legal to have TF ops in the graph still which can be
  // used later or in the case of SELECT were we allow TF ops in the final
  // graph.
  target.addLegalOp<mlir::ConstantOp>();
  target.addLegalOp<ConstOp>();
  if (run_tfl_runtime_verification_) {
    target.addDynamicallyLegalDialect<TensorFlowLiteDialect>(
        Optional<ConversionTarget::DynamicLegalityCallbackFn>(
            [](Operation* op) {
              auto tfl_op = dyn_cast_or_null<TflRuntimeVerifyOpInterface>(op);
              if (!tfl_op) return false;
              return succeeded(tfl_op.VerifyTflRuntimeConstraints(op));
            }));
  } else {
    target.addLegalDialect<TensorFlowLiteDialect>();
  }

  // Ignore transient errors by registering an no-op handler.
  // Applying legalization patterns will emit unwanted, transient errors when
  // the replaced TFLite ops do not meet the sanity checks. In order to ignore
  // the transient errors, the following lines override a diagnostic handler
  // with an no-op handler only while this pass runs.
  uint64_t current_thread_id = llvm::get_threadid();
  ScopedDiagnosticHandler scoped_diag_handler(
      context, [&current_thread_id](Diagnostic&) -> LogicalResult {
        // Consume only errors that are coming from the same thread in order not
        // to ignore errors from other passes that are running. Things running
        // in the pass manager can be multi-threaded.
        return success(current_thread_id == llvm::get_threadid());
      });

  // Keep trying to convert.
  // TODO(karimnosseir): This is similar to what apply greedy patterns does.
  // Look if there is a function that tries until it converge.
  // Currently unit-test doesn't do multiple tries, so we need this.
  const int max_iterations = 15;
  for (int i = 0; i < max_iterations; ++i) {
    if (failed(applyPartialConversion(func, target, patterns))) {
      return;
    }
  }
}

}  // namespace

// Creates an instance of the TensorFlow Lite dialect LegalizeTF pass.
std::unique_ptr<OperationPass<FuncOp>> CreateLegalizeTFPass(
    bool run_tfl_runtime_verification) {
  return std::make_unique<LegalizeTF>(run_tfl_runtime_verification);
}

static PassRegistration<LegalizeTF> pass(
    "tfl-legalize-tf", "Legalize from TensorFlow to TensorFlow Lite dialect");

}  // namespace TFL
}  // namespace mlir
