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
#include <utility>

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/Threading.h"
#include "mlir/Dialect/Quant/FakeQuantSupport.h"  // from @llvm-project
#include "mlir/Dialect/Quant/QuantOps.h"  // from @llvm-project
#include "mlir/Dialect/Quant/UniformSupport.h"  // from @llvm-project
#include "mlir/IR/Attributes.h"  // from @llvm-project
#include "mlir/IR/BuiltinTypes.h"  // from @llvm-project
#include "mlir/IR/Diagnostics.h"  // from @llvm-project
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "mlir/IR/Operation.h"  // from @llvm-project
#include "mlir/IR/PatternMatch.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "mlir/Support/LLVM.h"  // from @llvm-project
#include "mlir/Support/LogicalResult.h"  // from @llvm-project
#include "mlir/Transforms/DialectConversion.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/lite/ir/tfl_ops.h"
#include "tensorflow/compiler/mlir/lite/quantization/quantization_utils.h"
#include "tensorflow/compiler/mlir/lite/transforms/passes.h"
#include "tensorflow/compiler/mlir/lite/utils/attribute_utils.h"
#include "tensorflow/compiler/mlir/lite/utils/constant_utils.h"
#include "tensorflow/compiler/mlir/lite/utils/validators.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops_a_m.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/lower_tf.h"
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

constexpr char kUnidirectionalSequenceLstm[] = "tf.UnidirectionalSequenceLstm";
constexpr char kUnidirectionalSequenceRnn[] = "tf.UnidirectionalSequenceRnn";
constexpr char kTfLiteInputIndices[] = "_tflite_input_indices";

// Legalize operations in functions.
class LegalizeTF : public PassWrapper<LegalizeTF, FunctionPass> {
  void getDependentDialects(DialectRegistry& registry) const override {
    registry.insert<quant::QuantizationDialect, TFL::TensorFlowLiteDialect>();
  }

 public:
  LegalizeTF() = default;
  LegalizeTF(const LegalizeTF&) {}
  explicit LegalizeTF(bool run_tfl_runtime_verification) {
    run_tfl_runtime_verification_ = run_tfl_runtime_verification;
  }

  StringRef getArgument() const final {
    // This is the argument used to refer to the pass in
    // the textual format (on the commandline for example).
    return "tfl-legalize-tf";
  }
  StringRef getDescription() const final {
    // This is a brief description of the pass.
    return "Legalize from TensorFlow to TensorFlow Lite dialect";
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

// Util that casts 'val' to Int32 by adding a cast Op.
Value CreateCastToInt32(Value val, Location loc, PatternRewriter& rewriter) {
  IntegerType new_ele_type = rewriter.getIntegerType(32);
  if (auto shaped_type = val.getType().dyn_cast<RankedTensorType>()) {
    ShapedType new_type =
        RankedTensorType::get(shaped_type.getShape(), new_ele_type);
    return rewriter.createOrFold<TF::CastOp>(loc, new_type, val,
                                             rewriter.getBoolAttr(false));
  }
  return rewriter.createOrFold<TF::CastOp>(
      loc, UnrankedTensorType::get(new_ele_type), val,
      rewriter.getBoolAttr(false));
}

// Get shape of an operand or result, support both dynamic and static shape.
Value GetShape(Value input, Location loc, PatternRewriter& rewriter) {
  auto shaped_type = input.getType().cast<ShapedType>();
  if (shaped_type.hasStaticShape()) {
    auto static_shape = shaped_type.getShape();
    auto static_shape_type =
        RankedTensorType::get(static_shape.size(), rewriter.getIntegerType(64));
    auto static_shape_attr =
        mlir::DenseIntElementsAttr::get(static_shape_type, static_shape);
    return rewriter.create<TF::ConstOp>(loc, static_shape_attr).output();
  }

  // If the shape is not static, create a new ShapeOp.
  BoolAttr false_attr = rewriter.getBoolAttr(false);
  return rewriter
      .create<TF::ShapeOp>(loc, input,
                           /*use_32bit=*/false_attr)
      .output();
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
DECL_CONVERT_OP(Split);
DECL_CONVERT_OP(SplitV);
DECL_CONVERT_OP(Unpack);
DECL_CONVERT_OP(RandomUniform);
DECL_CONVERT_OP(Conv3D);
DECL_CONVERT_OP(Conv3DBackpropInputV2);

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

  tensorflow::random::PhiloxRandom generator(random_uniform_op.seed(),
                                             random_uniform_op.seed2());
  Distribution dist;
  size_t num_elements = 0;
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
      rewriter.replaceOpWithNewOp<arith::ConstantOp>(op, output_type,
                                                     output_data);
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
      IntegerType::get(attr.getContext(), /*width=*/32), value);
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
      StringAttr::get(rewriter.getContext(), "NONE");
  rewriter.replaceOpWithNewOp<ConcatenationOp>(
      op, output_type, values, axis_i32, fused_activation_function);
  return success();
}

LogicalResult ConvertTFMatMulOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tf_matmul_op = cast<TF::MatMulOp>(op);
  auto lhs = op->getOperand(0);
  auto rhs = op->getOperand(1);
  auto transpose = [&](Value input) -> std::pair<LogicalResult, Value> {
    RankedTensorType type =
        input.getType().dyn_cast_or_null<RankedTensorType>();
    if (!type || type.getRank() != 2) return {failure(), nullptr};

    auto permute_attr = DenseIntElementsAttr::get(
        RankedTensorType::get({2}, rewriter.getI32Type()), {1, 0});
    auto permute = rewriter.create<arith::ConstantOp>(
        op->getLoc(), permute_attr.getType(), permute_attr);
    llvm::SmallVector<int64_t, 2> new_shape{type.getShape()[1],
                                            type.getShape()[0]};
    auto output = rewriter.create<TFL::TransposeOp>(
        op->getLoc(), RankedTensorType::get(new_shape, type.getElementType()),
        input, permute);
    return {success(), output};
  };

  // TODO(jpienaar): Remove once handled via dailect conversion.
  if (tf_matmul_op.transpose_a()) {
    LogicalResult result = success();
    std::tie(result, lhs) = transpose(lhs);
    if (failed(result)) return failure();
  }
  if (!tf_matmul_op.transpose_b()) {
    LogicalResult result = success();
    std::tie(result, rhs) = transpose(rhs);
    if (failed(result)) return failure();
  }

  Type output_type = tf_matmul_op.getResult().getType();
  auto no_input = rewriter.create<ConstantOp>(
      op->getLoc(), rewriter.getNoneType(), rewriter.getUnitAttr());
  auto fc_op = rewriter.create<FullyConnectedOp>(
      op->getLoc(), ArrayRef<Type>{output_type}, lhs, rhs, no_input,
      rewriter.getStringAttr("NONE"), rewriter.getStringAttr("DEFAULT"),
      rewriter.getBoolAttr(false));
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
  auto axis = rewriter.getI32IntegerAttr(tf_pack_op.axis());

  rewriter.replaceOpWithNewOp<PackOp>(op, output_type, values, values_count,
                                      axis);
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

LogicalResult ConvertTFUnpackOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tf_unpack_op = cast<TF::UnpackOp>(op);

  auto input = tf_unpack_op.value();
  auto num = rewriter.getI32IntegerAttr(tf_unpack_op.num());
  // Axis can be negative.
  auto axis = rewriter.getI32IntegerAttr(tf_unpack_op.axis());

  rewriter.replaceOpWithNewOp<UnpackOp>(op, tf_unpack_op.output().getTypes(),
                                        input, num, axis);
  return success();
}

LogicalResult ConvertTFConv3DOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  if (!TFDataFormatIsNDHWC(op)) return failure();

  auto tf_op = cast<TF::Conv3DOp>(op);

  IntegerAttr stride_depth, stride_height, stride_width;
  if (!TFIntListIs1XYZ1(op, "strides", &stride_depth, &stride_height,
                        &stride_width))
    return failure();

  IntegerAttr dilation_depth_factor, dilation_height_factor,
      dilation_width_factor;
  if (!TFIntListIs1XYZ1(op, "dilations", &dilation_depth_factor,
                        &dilation_height_factor, &dilation_width_factor)) {
    // If the 'dilations' attribute is missing, we use the default value (1)
    // for all dilation depth, height and width factor.
    dilation_depth_factor = rewriter.getI32IntegerAttr(1);
    dilation_height_factor = rewriter.getI32IntegerAttr(1);
    dilation_width_factor = rewriter.getI32IntegerAttr(1);
  }

  StringAttr padding;
  if (!TFPaddingIsSameOrValid(op, &padding)) return failure();

  // TensorFlow Conv3D has no bias, optimization patterns will fuse Conv3D
  // with other ops can fill the bias.
  Value none = rewriter.create<mlir::ConstantOp>(
      op->getLoc(), rewriter.getNoneType(), rewriter.getUnitAttr());

  rewriter.replaceOpWithNewOp<TFL::Conv3DOp>(
      op, tf_op.getType(), tf_op.input(), tf_op.filter(),
      /*bias=*/none, dilation_depth_factor, dilation_height_factor,
      dilation_width_factor,
      /*fused_activation_function=*/rewriter.getStringAttr("NONE"), padding,
      stride_depth, stride_height, stride_width);

  return success();
}

LogicalResult ConvertTFConv3DBackpropInputV2Op::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  if (!TFDataFormatIsNDHWC(op)) return failure();

  auto tf_op = cast<TF::Conv3DBackpropInputV2Op>(op);

  IntegerAttr stride_depth, stride_height, stride_width;
  if (!TFIntListIs1XYZ1(op, "strides", &stride_depth, &stride_height,
                        &stride_width))
    return failure();

  IntegerAttr dilation_depth_factor, dilation_height_factor,
      dilation_width_factor;
  if (!TFIntListIs1XYZ1(op, "dilations", &dilation_depth_factor,
                        &dilation_height_factor, &dilation_width_factor)) {
    // If the 'dilations' attribute is missing, we use the default value (1)
    // for all dilation depth, height and width factor.
    dilation_depth_factor = rewriter.getI32IntegerAttr(1);
    dilation_height_factor = rewriter.getI32IntegerAttr(1);
    dilation_width_factor = rewriter.getI32IntegerAttr(1);
  }

  StringAttr padding;
  if (!TFPaddingIsSameOrValid(op, &padding)) return failure();

  // TensorFlow Conv3D has no bias, optimization patterns will fuse Conv3D
  // with other ops can fill the bias.
  Value none = rewriter.create<mlir::ConstantOp>(
      op->getLoc(), rewriter.getNoneType(), rewriter.getUnitAttr());

  Value output_shape =
      CreateCastToInt32(tf_op.input_sizes(), op->getLoc(), rewriter);

  rewriter.replaceOpWithNewOp<TFL::Conv3DTransposeOp>(
      op, tf_op.getType(), output_shape, tf_op.filter(), tf_op.out_backprop(),
      /*bias=*/none, dilation_depth_factor, dilation_height_factor,
      dilation_width_factor,
      /*fused_activation_function=*/rewriter.getStringAttr("NONE"), padding,
      stride_depth, stride_height, stride_width);

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

    Value lstm_result = rewriter.create<TFL::UnidirectionalSequenceLSTMOp>(
        op->getLoc(), result_types, inputs, attributes);

    // Rewire the output.
    rewriter.replaceOp(op, {nullptr, nullptr, lstm_result});
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

    Value rnn_result = rewriter.create<TFL::UnidirectionalSequenceRNNOp>(
        op->getLoc(), result_types, inputs, attributes);

    // Rewire the output.
    rewriter.replaceOp(op, {nullptr, rnn_result});

    return success();
  }
};

// Put two TFL BroadcastTo ops in front of the given TF binary broadcast op to
// to make binary broadcast-able op conversion always successful and does not
// require flex delegate.
template <typename SourceOp>
class ApplyExplicitBroadcasting : public OpRewritePattern<SourceOp> {
 public:
  using OpRewritePattern<SourceOp>::OpRewritePattern;

  LogicalResult rewriteOpWithDynamicInput(Operation* op,
                                          PatternRewriter& rewriter) const {
    auto lhs = op->getOperand(0);
    auto rhs = op->getOperand(1);
    auto out = op->getResult(0);

    // Calculates symbolic broadcast shape that is only used in types.
    SmallVector<int64_t, 4> symbolic_broadcast_shape;
    if (!OpTrait::util::getBroadcastedShape(
            lhs.getType().cast<ShapedType>().getShape(),
            rhs.getType().cast<ShapedType>().getShape(),
            symbolic_broadcast_shape)) {
      return failure();
    }

    // Calculates the broadcast shape using BroadcastArgs op.
    Value lhs_shape = GetShape(lhs, op->getLoc(), rewriter);
    Value rhs_shape = GetShape(rhs, op->getLoc(), rewriter);
    auto broadcast_shape =
        rewriter
            .create<TF::BroadcastArgsOp>(
                op->getLoc(),
                RankedTensorType::get(symbolic_broadcast_shape.size(),
                                      rewriter.getIntegerType(64)),
                lhs_shape, rhs_shape)
            .r0();

    // Broadcasts inputs using BroadcastTo op.
    auto broadcast_type = RankedTensorType::get(
        symbolic_broadcast_shape, getElementTypeOrSelf(lhs.getType()));
    auto broadcasted_lhs =
        rewriter
            .create<TF::BroadcastToOp>(op->getLoc(), broadcast_type, lhs,
                                       broadcast_shape)
            .output();
    auto broadcasted_rhs =
        rewriter
            .create<TF::BroadcastToOp>(op->getLoc(), broadcast_type, rhs,
                                       broadcast_shape)
            .output();

    // Recreate an op with the above BroadcastTo op results.
    RankedTensorType result_type = RankedTensorType::get(
        symbolic_broadcast_shape, getElementTypeOrSelf(out.getType()));
    rewriter.replaceOpWithNewOp<SourceOp>(op, result_type, broadcasted_lhs,
                                          broadcasted_rhs);
    return success();
  }

  LogicalResult matchAndRewrite(SourceOp src_op,
                                PatternRewriter& rewriter) const override {
    Operation* op = static_cast<Operation*>(src_op);
    auto lhs = op->getOperand(0);
    auto rhs = op->getOperand(1);

    if (!lhs.getType().cast<ShapedType>().hasStaticShape() ||
        !rhs.getType().cast<ShapedType>().hasStaticShape()) {
      return rewriteOpWithDynamicInput(op, rewriter);
    }

    auto lhs_shape = lhs.getType().cast<ShapedType>().getShape();
    auto rhs_shape = rhs.getType().cast<ShapedType>().getShape();

    if (lhs_shape == rhs_shape) {
      return failure();
    }

    // Calculate the broadcasted shape.
    SmallVector<int64_t, 4> result_shape;
    if (!OpTrait::util::getBroadcastedShape(lhs_shape, rhs_shape,
                                            result_shape)) {
      return failure();
    }

    RankedTensorType result_type = RankedTensorType::get(
        result_shape, getElementTypeOrSelf(op->getResult(0).getType()));

    // Create a const op, that stores the above broadcasted shape.
    auto new_shape_attr = mlir::DenseIntElementsAttr::get(
        RankedTensorType::get(result_shape.size(), rewriter.getIntegerType(64)),
        result_shape);
    auto new_shape = rewriter.create<TF::ConstOp>(op->getLoc(), new_shape_attr);

    // Apply BroadcastTo ops to each input.
    auto broadcast_type = RankedTensorType::get(
        result_shape, getElementTypeOrSelf(lhs.getType()));

    if (result_type.getShape() != lhs_shape) {
      lhs = rewriter
                .create<TF::BroadcastToOp>(op->getLoc(), broadcast_type, lhs,
                                           new_shape)
                .output();
    }
    if (result_type.getShape() != rhs_shape) {
      rhs = rewriter
                .create<TF::BroadcastToOp>(op->getLoc(), broadcast_type, rhs,
                                           new_shape)
                .output();
    }

    // Recreate an op with the above Broadcast op results.
    rewriter.replaceOpWithNewOp<SourceOp>(op, result_type, lhs, rhs);
    return success();
  }
};

// This specialization is for TF SelectV2 op. SelectV2 op have three inputs and
// they should have broadcastable shapes.
template <>
class ApplyExplicitBroadcasting<TF::SelectV2Op>
    : public OpRewritePattern<TF::SelectV2Op> {
 public:
  using OpRewritePattern<TF::SelectV2Op>::OpRewritePattern;

  LogicalResult rewriteOpWithDynamicInput(Operation* op,
                                          PatternRewriter& rewriter) const {
    auto cond = op->getOperand(0);
    auto lhs = op->getOperand(1);
    auto rhs = op->getOperand(2);
    auto out = op->getResult(0);

    // Calculates symbolic broadcast shape that is only used in types.
    SmallVector<int64_t, 4> symbolic_broadcast_lhs_rhs_shape;
    if (!OpTrait::util::getBroadcastedShape(
            lhs.getType().cast<ShapedType>().getShape(),
            rhs.getType().cast<ShapedType>().getShape(),
            symbolic_broadcast_lhs_rhs_shape)) {
      return failure();
    }
    SmallVector<int64_t, 4> symbolic_broadcast_shape;
    if (!OpTrait::util::getBroadcastedShape(
            cond.getType().cast<ShapedType>().getShape(),
            symbolic_broadcast_lhs_rhs_shape, symbolic_broadcast_shape)) {
      return failure();
    }

    // Calculates the broadcast shape using BroadcastArgs op.
    Value cond_shape = GetShape(cond, op->getLoc(), rewriter);
    Value lhs_shape = GetShape(lhs, op->getLoc(), rewriter);
    Value rhs_shape = GetShape(rhs, op->getLoc(), rewriter);
    auto broadcast_shape_value =
        rewriter
            .create<TF::BroadcastArgsOp>(op->getLoc(), lhs_shape.getType(),
                                         lhs_shape, rhs_shape)
            .r0();
    broadcast_shape_value =
        rewriter
            .create<TF::BroadcastArgsOp>(op->getLoc(), lhs_shape.getType(),
                                         broadcast_shape_value, cond_shape)
            .r0();

    // Broadcasting inputs using BroadcastTo op.
    auto broadcast_type = RankedTensorType::get(
        symbolic_broadcast_shape, getElementTypeOrSelf(out.getType()));
    auto broadcasted_cond =
        rewriter
            .create<TF::BroadcastToOp>(
                op->getLoc(),
                RankedTensorType::get(symbolic_broadcast_shape,
                                      rewriter.getIntegerType(1)),
                cond, broadcast_shape_value)
            .output();
    auto broadcasted_lhs =
        rewriter
            .create<TF::BroadcastToOp>(op->getLoc(), broadcast_type, lhs,
                                       broadcast_shape_value)
            .output();
    auto broadcasted_rhs =
        rewriter
            .create<TF::BroadcastToOp>(op->getLoc(), broadcast_type, rhs,
                                       broadcast_shape_value)
            .output();

    // Recreate an op with the above BroadcastTo op results.
    rewriter.replaceOpWithNewOp<TF::SelectV2Op>(
        op, broadcast_type, broadcasted_cond, broadcasted_lhs, broadcasted_rhs);
    return success();
  }

  LogicalResult matchAndRewrite(TF::SelectV2Op src_op,
                                PatternRewriter& rewriter) const override {
    Operation* op = static_cast<Operation*>(src_op);
    auto cond = op->getOperand(0);
    auto lhs = op->getOperand(1);
    auto rhs = op->getOperand(2);

    // Should have static shapes to calculate the broadcasted shape.
    if (!lhs.getType().cast<ShapedType>().hasStaticShape() ||
        !rhs.getType().cast<ShapedType>().hasStaticShape() ||
        !cond.getType().cast<ShapedType>().hasStaticShape()) {
      return rewriteOpWithDynamicInput(op, rewriter);
    }

    auto lhs_shape = lhs.getType().cast<ShapedType>().getShape();
    auto rhs_shape = rhs.getType().cast<ShapedType>().getShape();
    auto cond_shape = cond.getType().cast<ShapedType>().getShape();

    if (lhs_shape == rhs_shape && cond_shape == lhs_shape) {
      return failure();
    }

    // Calculate the broadcasted shape.
    SmallVector<int64_t, 4> broadcasted_shape;
    if (!OpTrait::util::getBroadcastedShape(lhs_shape, rhs_shape,
                                            broadcasted_shape)) {
      return failure();
    }

    SmallVector<int64_t, 4> result_shape;
    if (!OpTrait::util::getBroadcastedShape(broadcasted_shape, cond_shape,
                                            result_shape)) {
      return failure();
    }

    // Create a const op, that stores the above broadcasted shape.
    auto shape_type =
        RankedTensorType::get(result_shape.size(), rewriter.getIntegerType(64));
    auto new_shape_attr =
        mlir::DenseIntElementsAttr::get(shape_type, result_shape);
    auto new_shape = rewriter.create<TF::ConstOp>(op->getLoc(), new_shape_attr);

    // Apply BroadcastTo ops to each input.
    auto cond_result_type =
        RankedTensorType::get(result_shape, rewriter.getIntegerType(1));
    auto result_type = RankedTensorType::get(
        result_shape, getElementTypeOrSelf(lhs.getType()));

    if (result_shape != cond_shape) {
      cond = rewriter
                 .create<TF::BroadcastToOp>(op->getLoc(), cond_result_type,
                                            cond, new_shape)
                 .output();
    }
    if (result_shape != lhs_shape) {
      lhs = rewriter
                .create<TF::BroadcastToOp>(op->getLoc(), result_type, lhs,
                                           new_shape)
                .output();
    }
    if (result_shape != rhs_shape) {
      rhs = rewriter
                .create<TF::BroadcastToOp>(op->getLoc(), result_type, rhs,
                                           new_shape)
                .output();
    }

    // Recreate an op with the above Broadcast op results.
    rewriter.replaceOpWithNewOp<TF::SelectV2Op>(op, result_type, cond, lhs,
                                                rhs);
    return success();
  }
};

void addPatterns(MLIRContext* context, OwningRewritePatternList& patterns) {
  // Add TF->TF lowering patterns.
  TF::PopulateLoweringTFPatterns(context, &patterns);

  // Add the generated patterns to the list.
  populateWithGenerated(patterns);
  patterns
      .insert<ConvertTFConcatV2Op, ConvertTFMatMulOp, ConvertTFMatrixDiagV2Op,
              ConvertTFMatrixDiagV3Op, ConvertTFPackOp, ConvertTFSplitOp,
              ConvertTFSplitVOp, ConvertTFUnpackOp, ConvertTFAssertOp,
              ConvertTFRandomUniformOp, ConvertTFConv3DOp,
              ConvertTFConv3DBackpropInputV2Op>(context);

  // Ophint python converter converted tf node pattern.
  patterns.insert<LegalizeUnidirectionalSequenceLstm,
                  LegalizeUnidirectionalSequenceRnn>(context);
}

void applyPatterns(FuncOp func, ConversionTarget& target,
                   FrozenRewritePatternSet& frozenPatterns) {
  // Keep trying to convert.
  // TODO(karimnosseir): This is similar to what apply greedy patterns does.
  // Look if there is a function that tries until it converge.
  // Currently unit-test doesn't do multiple tries, so we need this.
  const int max_iterations = 15;
  for (int i = 0; i < max_iterations; ++i) {
    if (failed(applyPartialConversion(func, target, frozenPatterns))) {
      return;
    }
  }
}

void LegalizeTF::runOnFunction() {
  auto* context = &getContext();
  auto func = getFunction();

  ConversionTarget target(*context);
  // It is legal to have TF ops in the graph still which can be
  // used later or in the case of SELECT were we allow TF ops in the final
  // graph.
  target.addLegalOp<mlir::arith::ConstantOp>();
  target.addLegalOp<mlir::ConstantOp>();
  target.addLegalOp<ConstOp>();
  if (run_tfl_runtime_verification_) {
    target.addDynamicallyLegalDialect<TensorFlowLiteDialect>([](Operation* op) {
      auto tfl_op = dyn_cast_or_null<TflRuntimeVerifyOpInterface>(op);
      if (!tfl_op) return false;
      return succeeded(tfl_op.VerifyTflRuntimeConstraints(op));
    });
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

  OwningRewritePatternList stage1Patterns(&getContext());

  addPatterns(context, stage1Patterns);

  FrozenRewritePatternSet stage1FrozenPatterns(std::move(stage1Patterns));
  applyPatterns(func, target, stage1FrozenPatterns);

  // Explict BroadcastTo addition for left-over broadcast-able ops.
  // The following pattern matchings should be done after the other legalization
  // rules in order not to add unnecessary BroadcastTo ops.
  OwningRewritePatternList stage2Patterns(&getContext());

  addPatterns(context, stage2Patterns);

  stage2Patterns.insert<ApplyExplicitBroadcasting<TF::LessEqualOp>,
                        ApplyExplicitBroadcasting<TF::GreaterEqualOp>,
                        ApplyExplicitBroadcasting<TF::NotEqualOp>,
                        ApplyExplicitBroadcasting<TF::GreaterOp>,
                        ApplyExplicitBroadcasting<TF::LessOp>,
                        ApplyExplicitBroadcasting<TF::EqualOp>,
                        ApplyExplicitBroadcasting<TF::AddOp>,
                        ApplyExplicitBroadcasting<TF::AddV2Op>,
                        ApplyExplicitBroadcasting<TF::MulOp>,
                        ApplyExplicitBroadcasting<TF::DivOp>,
                        ApplyExplicitBroadcasting<TF::RealDivOp>,
                        ApplyExplicitBroadcasting<TF::SubOp>,
                        ApplyExplicitBroadcasting<TF::FloorDivOp>,
                        ApplyExplicitBroadcasting<TF::FloorModOp>,
                        ApplyExplicitBroadcasting<TF::PowOp>,
                        ApplyExplicitBroadcasting<TF::MaximumOp>,
                        ApplyExplicitBroadcasting<TF::MinimumOp>,
                        ApplyExplicitBroadcasting<TF::SquaredDifferenceOp>,
                        ApplyExplicitBroadcasting<TF::SelectV2Op>>(context);

  FrozenRewritePatternSet stage2FrozenPatterns(std::move(stage2Patterns));
  applyPatterns(func, target, stage2FrozenPatterns);
}

}  // namespace

// Creates an instance of the TensorFlow Lite dialect LegalizeTF pass.
std::unique_ptr<OperationPass<FuncOp>> CreateLegalizeTFPass(
    bool run_tfl_runtime_verification) {
  return std::make_unique<LegalizeTF>(run_tfl_runtime_verification);
}

static PassRegistration<LegalizeTF> pass;

}  // namespace TFL
}  // namespace mlir
