/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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

// Legalize TensorFlow Lite to TOSA

#include <climits>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <numeric>
#include <unordered_set>

#include "mlir/Dialect/Quant/QuantOps.h"  // from @llvm-project
#include "mlir/Dialect/Quant/QuantTypes.h"  // from @llvm-project
#include "mlir/Dialect/Tosa/IR/TosaOps.h"  // from @llvm-project
#include "mlir/IR/BuiltinAttributes.h"  // from @llvm-project
#include "mlir/IR/BuiltinTypes.h"  // from @llvm-project
#include "mlir/IR/PatternMatch.h"  // from @llvm-project
#include "mlir/Support/LLVM.h"  // from @llvm-project
#include "mlir/Transforms/DialectConversion.h"  // from @llvm-project
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/lite/ir/tfl_ops.h"
#include "tensorflow/compiler/mlir/tosa/transforms/legalize_common.h"
#include "tensorflow/compiler/mlir/tosa/transforms/legalize_utils.h"
#include "tensorflow/compiler/mlir/tosa/transforms/passes.h"

#define PASS_NAME "tosa-legalize-tfl"
#define DEBUG_TYPE PASS_NAME
#define HARDSWISH_EXPLICIT_RESCALING false

// Conditionally avoid converting some TFLite ops to TOSA.
// By default, all conversions will be invoked.
//
// The denylist file lists patterns which are not legalized from TFLite to TOSA.
llvm::cl::opt<std::string> tfl_tosa_denylist(
    "tfl-tosa-denylist",
    llvm::cl::desc("<a list of patterns not legalized from TFLite to TOSA>"),
    llvm::cl::init("transforms/tfl_tosa_denylist.txt"),
    llvm::cl::value_desc("pattern name"));

namespace mlir {
namespace tosa {
namespace {
#define GEN_PASS_CLASSES
#include "tensorflow/compiler/mlir/tosa/transforms/passes.h.inc"

// Performs lowering to TOSA dialect.
class LegalizeTFL : public TosaLegalizeTFLPassBase<LegalizeTFL> {
 public:
  explicit LegalizeTFL() {}
  void runOnFunction() override;
};

#include "tensorflow/compiler/mlir/tosa/transforms/tfl_legalize_patterns.inc"

#define DECL_CONVERT_OP(tfl_op)                                              \
  struct ConvertTFL##tfl_op##Op : public RewritePattern {                    \
    explicit ConvertTFL##tfl_op##Op(MLIRContext* context)                    \
        : RewritePattern(TFL::tfl_op##Op::getOperationName(), 1, context) {} \
    LogicalResult matchAndRewrite(Operation* op,                             \
                                  PatternRewriter& rewriter) const override; \
  }
DECL_CONVERT_OP(Relu);
DECL_CONVERT_OP(Relu6);
DECL_CONVERT_OP(Equal);
DECL_CONVERT_OP(NotEqual);
DECL_CONVERT_OP(Greater);
DECL_CONVERT_OP(GreaterEqual);
DECL_CONVERT_OP(Add);
DECL_CONVERT_OP(Sub);
DECL_CONVERT_OP(Mul);
DECL_CONVERT_OP(Square);
DECL_CONVERT_OP(SquaredDifference);
DECL_CONVERT_OP(Round);
DECL_CONVERT_OP(Div);
DECL_CONVERT_OP(Maximum);
DECL_CONVERT_OP(Minimum);
DECL_CONVERT_OP(FloorMod);
DECL_CONVERT_OP(FloorDiv);
DECL_CONVERT_OP(AddN);
DECL_CONVERT_OP(AveragePool2D);
DECL_CONVERT_OP(MaxPool2D);
DECL_CONVERT_OP(Concatenation);
DECL_CONVERT_OP(Reshape);
DECL_CONVERT_OP(Rank);
DECL_CONVERT_OP(Shape);
DECL_CONVERT_OP(ExpandDims);
DECL_CONVERT_OP(Squeeze);
DECL_CONVERT_OP(Fill);
DECL_CONVERT_OP(Elu);
DECL_CONVERT_OP(Softmax);
DECL_CONVERT_OP(LogSoftmax);
DECL_CONVERT_OP(Sqrt);
DECL_CONVERT_OP(ReduceAny);
DECL_CONVERT_OP(ReduceMax);
DECL_CONVERT_OP(ReduceMin);
DECL_CONVERT_OP(Mean);
DECL_CONVERT_OP(ReduceProd);
DECL_CONVERT_OP(Sum);
DECL_CONVERT_OP(Conv2D);
DECL_CONVERT_OP(TransposeConv);
DECL_CONVERT_OP(DepthwiseConv2D);
DECL_CONVERT_OP(FullyConnected);
DECL_CONVERT_OP(Split);
DECL_CONVERT_OP(SplitV);
DECL_CONVERT_OP(Pack);
DECL_CONVERT_OP(Unpack);
DECL_CONVERT_OP(Transpose);
DECL_CONVERT_OP(Tile);
DECL_CONVERT_OP(Slice);
DECL_CONVERT_OP(StridedSlice);
DECL_CONVERT_OP(HardSwish);
DECL_CONVERT_OP(ZerosLike);
DECL_CONVERT_OP(Less);
DECL_CONVERT_OP(LessEqual);
DECL_CONVERT_OP(Pad);
DECL_CONVERT_OP(ResizeBilinear);
DECL_CONVERT_OP(ResizeNearestNeighbor);
DECL_CONVERT_OP(Select);
DECL_CONVERT_OP(SelectV2);
DECL_CONVERT_OP(SpaceToBatchNd);
DECL_CONVERT_OP(BatchToSpaceNd);
DECL_CONVERT_OP(SpaceToDepth);
DECL_CONVERT_OP(DepthToSpace);
DECL_CONVERT_OP(Logistic);
DECL_CONVERT_OP(Tanh);
DECL_CONVERT_OP(PRelu);
DECL_CONVERT_OP(LeakyRelu);
DECL_CONVERT_OP(Neg);
DECL_CONVERT_OP(Yield);
DECL_CONVERT_OP(Custom);
DECL_CONVERT_OP(ReverseV2);
DECL_CONVERT_OP(Quantize);
DECL_CONVERT_OP(Dequantize);
DECL_CONVERT_OP(Const);
DECL_CONVERT_OP(QConst);
DECL_CONVERT_OP(Gather);
DECL_CONVERT_OP(GatherNd);
DECL_CONVERT_OP(OneHot);
DECL_CONVERT_OP(ArgMax);
DECL_CONVERT_OP(FakeQuant);
#undef DECL_CONVERT_OP

// Input from tfl.conv2d takes 64 bits a bias, while tosa.conv2d expects 48
// bits. Need to do a customized truncate here instead of tablegen to handle
// attribute with negative value.

struct ConvertConstantOp : public RewritePattern {
  explicit ConvertConstantOp(MLIRContext* context)
      : RewritePattern(ConstantOp::getOperationName(), 1, context) {}
  LogicalResult matchAndRewrite(Operation* op,
                                PatternRewriter& rewriter) const override;
};

LogicalResult ConvertTFLReluOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_relu_op = cast<TFL::ReluOp>(op);

  ShapedType input_type = tfl_relu_op.x().getType().dyn_cast<ShapedType>();
  ShapedType output_type =
      tfl_relu_op.getResult().getType().dyn_cast<ShapedType>();
  // Not a ranked tensor output
  if (!input_type || !output_type) return failure();

  bool input_is_qtype =
      input_type.getElementType().isa<mlir::quant::UniformQuantizedType>();
  bool output_is_qtype =
      output_type.getElementType().isa<mlir::quant::UniformQuantizedType>();

  if (input_is_qtype != output_is_qtype) {
    return op->emitOpError(
        "ConvertTFLReluOp: input/output tensor should "
        "be all quantized or all floating-point.");
  }

  int64_t clamp_min = 0;
  Value clamp_in = tfl_relu_op.x();

  if (output_is_qtype) {
    UniformQuantizedType input_qtype =
        input_type.getElementType()
            .dyn_cast<mlir::quant::UniformQuantizedType>();
    UniformQuantizedType output_qtype =
        output_type.getElementType()
            .dyn_cast<mlir::quant::UniformQuantizedType>();

    clamp_min = output_qtype.getZeroPoint();
    clamp_in =
        buildRescale(rewriter, op, output_type, tfl_relu_op.x(),
                     input_qtype.getScale() / output_qtype.getScale(),
                     input_qtype.getZeroPoint(), output_qtype.getZeroPoint(),
                     /*double_round=*/false, /*scale32=*/true);
  }

  CreateReplaceOpAndInfer<tosa::ClampOp>(
      rewriter, op, output_type, clamp_in,
      rewriter.getI64IntegerAttr(clamp_min),
      rewriter.getI64IntegerAttr(std::numeric_limits<int32_t>::max()),
      rewriter.getF32FloatAttr(0.0f),
      rewriter.getF32FloatAttr(std::numeric_limits<float>::max()));

  return success();
}

LogicalResult ConvertTFLRelu6Op::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_relu6_op = cast<TFL::Relu6Op>(op);

  ShapedType input_type = tfl_relu6_op.x().getType().dyn_cast<ShapedType>();
  ShapedType output_type =
      tfl_relu6_op.getResult().getType().dyn_cast<ShapedType>();
  // Not a ranked tensor output
  if (!input_type || !output_type) return failure();

  bool input_is_qtype =
      input_type.getElementType().isa<mlir::quant::UniformQuantizedType>();
  bool output_is_qtype =
      output_type.getElementType().isa<mlir::quant::UniformQuantizedType>();

  if (input_is_qtype != output_is_qtype) {
    return op->emitOpError(
        "ConvertTFLRelu6Op: input/output tensor should "
        "be all quantized or all floating-point.");
  }

  int64_t clamp_min = 0;
  int64_t clamp_max = 6;
  Value clamp_in = tfl_relu6_op.x();

  if (output_is_qtype && input_is_qtype) {
    UniformQuantizedType input_qtype =
        input_type.getElementType()
            .dyn_cast<mlir::quant::UniformQuantizedType>();
    UniformQuantizedType output_qtype =
        output_type.getElementType()
            .dyn_cast<mlir::quant::UniformQuantizedType>();

    clamp_min = output_qtype.getZeroPoint();
    clamp_max = std::llround(6.0f / output_qtype.getScale()) +
                output_qtype.getZeroPoint();

    clamp_in =
        buildRescale(rewriter, op, output_type, tfl_relu6_op.x(),
                     input_qtype.getScale() / output_qtype.getScale(),
                     input_qtype.getZeroPoint(), output_qtype.getZeroPoint(),
                     /*double_round=*/false, /*scale32=*/true);
  }

  CreateReplaceOpAndInfer<tosa::ClampOp>(rewriter, op, output_type, clamp_in,
                                         rewriter.getI64IntegerAttr(clamp_min),
                                         rewriter.getI64IntegerAttr(clamp_max),
                                         rewriter.getF32FloatAttr(0.0f),
                                         rewriter.getF32FloatAttr(6.0f));

  return success();
}

static LogicalResult prepareMatchAndRewriteComparison(
    Operation* op, mlir::OperandRange operands, PatternRewriter& rewriter,
    llvm::SmallVectorImpl<Value>& newOperands) {
  Value x = operands[0];
  Value y = operands[1];
  Value result = op->getResult(0);

  ShapedType input_x_type = x.getType().dyn_cast<ShapedType>();
  ShapedType input_y_type = y.getType().dyn_cast<ShapedType>();
  ShapedType output_type = result.getType().dyn_cast<ShapedType>();
  // Not a shaped tensor output
  if (!input_x_type || !input_y_type || !output_type) return failure();

  bool input_x_is_qtype =
      input_x_type.getElementType().isa<mlir::quant::UniformQuantizedType>();
  bool input_y_is_qtype =
      input_y_type.getElementType().isa<mlir::quant::UniformQuantizedType>();
  bool output_is_qtype =
      output_type.getElementType().isa<mlir::quant::UniformQuantizedType>();

  if (input_x_is_qtype != input_y_is_qtype ||
      input_y_is_qtype != output_is_qtype) {
    return op->emitOpError(
        "ConvertTFLEqualOp: input/output tensor should "
        "be all quantized or all floating-point.");
  }

  if (!output_is_qtype && !input_x_is_qtype && !input_y_is_qtype) {
    newOperands.push_back(x);
    newOperands.push_back(y);
    return success();
  }

  UniformQuantizedType input_x_qtype =
      input_x_type.getElementType()
          .dyn_cast<mlir::quant::UniformQuantizedType>();
  UniformQuantizedType input_y_qtype =
      input_y_type.getElementType()
          .dyn_cast<mlir::quant::UniformQuantizedType>();

  if (input_x_qtype.getScale() != input_y_qtype.getScale() ||
      input_x_qtype.getZeroPoint() != input_y_qtype.getZeroPoint()) {
    return op->emitOpError(
        "ConvertTFLEqualOp: input_x and input_y scale/zp "
        "must be the same");
  }

  x = buildRescaleToInt32(rewriter, op, x, 1.0f, input_x_qtype.getZeroPoint());
  y = buildRescaleToInt32(rewriter, op, y, 1.0f, input_y_qtype.getZeroPoint());

  newOperands.push_back(x);
  newOperands.push_back(y);
  return success();
}

LogicalResult ConvertTFLEqualOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  llvm::SmallVector<Value, 2> newOperands;
  LogicalResult status = prepareMatchAndRewriteComparison(
      op, op->getOperands(), rewriter, newOperands);
  if (status.failed()) return failure();

  CreateReplaceOpAndInfer<tosa::EqualOp>(
      rewriter, op, op->getResult(0).getType(), newOperands[0], newOperands[1]);

  return success();
}

LogicalResult ConvertTFLNotEqualOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  llvm::SmallVector<Value, 2> newOperands;
  LogicalResult status = prepareMatchAndRewriteComparison(
      op, op->getOperands(), rewriter, newOperands);
  if (status.failed()) return failure();

  auto equal_op = CreateOpAndInfer<tosa::EqualOp>(
      rewriter, op->getLoc(), op->getResult(0).getType(), newOperands[0],
      newOperands[1]);

  CreateReplaceOpAndInfer<tosa::LogicalNotOp>(
      rewriter, op, op->getResult(0).getType(), equal_op);

  return success();
}

LogicalResult ConvertTFLGreaterOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  llvm::SmallVector<Value, 2> newOperands;
  LogicalResult status = prepareMatchAndRewriteComparison(
      op, op->getOperands(), rewriter, newOperands);
  if (status.failed()) return failure();

  CreateReplaceOpAndInfer<tosa::GreaterOp>(
      rewriter, op, op->getResult(0).getType(), newOperands[0], newOperands[1]);

  return success();
}

LogicalResult ConvertTFLGreaterEqualOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  llvm::SmallVector<Value, 2> newOperands;
  LogicalResult status = prepareMatchAndRewriteComparison(
      op, op->getOperands(), rewriter, newOperands);
  if (status.failed()) return failure();

  CreateReplaceOpAndInfer<tosa::GreaterEqualOp>(
      rewriter, op, op->getResult(0).getType(), newOperands[0], newOperands[1]);

  return success();
}

LogicalResult ConvertTFLLessOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  llvm::SmallVector<Value, 2> newOperands;
  LogicalResult status = prepareMatchAndRewriteComparison(
      op, op->getOperands(), rewriter, newOperands);
  if (status.failed()) return failure();

  CreateReplaceOpAndInfer<tosa::GreaterOp>(
      rewriter, op, op->getResult(0).getType(), newOperands[1], newOperands[0]);
  return success();
}

LogicalResult ConvertTFLLessEqualOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  llvm::SmallVector<Value, 2> newOperands;
  LogicalResult status = prepareMatchAndRewriteComparison(
      op, op->getOperands(), rewriter, newOperands);
  if (status.failed()) return failure();

  // Swapping the args handles the greater/less difference.
  CreateReplaceOpAndInfer<tosa::GreaterEqualOp>(
      rewriter, op, op->getResult(0).getType(), newOperands[1], newOperands[0]);

  return success();
}

template <typename TflOp, typename TosaOp>
static LogicalResult matchAndRewriteAddSub(Operation* op,
                                           mlir::OperandRange operands,
                                           PatternRewriter& rewriter) {
  auto tfl_add_op = cast<TflOp>(op);

  ShapedType input_lhs_type =
      tfl_add_op.lhs().getType().template dyn_cast<ShapedType>();
  ShapedType input_rhs_type =
      tfl_add_op.rhs().getType().template dyn_cast<ShapedType>();
  ShapedType output_type =
      tfl_add_op.getResult().getType().template dyn_cast<ShapedType>();
  // Not a ranked tensor output
  if (!input_lhs_type || !input_rhs_type || !output_type) return failure();

  bool input_lhs_is_qtype =
      input_lhs_type.getElementType().isa<mlir::quant::UniformQuantizedType>();
  bool input_rhs_is_qtype =
      input_rhs_type.getElementType().isa<mlir::quant::UniformQuantizedType>();
  bool output_is_qtype =
      output_type.getElementType().isa<mlir::quant::UniformQuantizedType>();

  if (input_lhs_is_qtype != output_is_qtype ||
      input_rhs_is_qtype != output_is_qtype) {
    return op->emitOpError(
        "ConvertTFLAddOp: input/output tensor should "
        "be all quantized or all floating-point.");
  }

  Value output;
  if (output_is_qtype && input_lhs_is_qtype && input_rhs_is_qtype) {
    ShapedType rescale_type = output_type.clone(rewriter.getI32Type());
    UniformQuantizedType input_lhs_qtype =
        input_lhs_type.getElementType()
            .dyn_cast<mlir::quant::UniformQuantizedType>();
    UniformQuantizedType input_rhs_qtype =
        input_rhs_type.getElementType()
            .dyn_cast<mlir::quant::UniformQuantizedType>();
    UniformQuantizedType output_qtype =
        output_type.getElementType()
            .dyn_cast<mlir::quant::UniformQuantizedType>();

    // Following quantization described in tensorflow/lite/kernels/add.cc
    // In details it does:
    // 1. Rescale inputs to scale = 2.0 x max(lhs.scale, rhs.scale)
    // 2. Extra left shift to input to increase precision
    // Where input_shift = 20 if input is 8-bit
    // input_shift = 15 if input is 16-bit
    double in_lhs_scale = input_lhs_qtype.getScale();
    double in_rhs_scale = input_rhs_qtype.getScale();
    double output_scale = output_qtype.getScale();
    double max_scale_2x = 2.0 * std::max(in_lhs_scale, in_rhs_scale);

    const int32_t SHIFT_8_BIT = 20;
    const int32_t SHIFT_16_BIT = 15;

    int32_t input_shift = (output_qtype.getStorageTypeIntegralWidth() == 16)
                              ? SHIFT_16_BIT
                              : SHIFT_8_BIT;

    double lhs_rescale_scale =
        static_cast<double>(1 << input_shift) * in_lhs_scale / max_scale_2x;
    double rhs_rescale_scale =
        static_cast<double>(1 << input_shift) * in_rhs_scale / max_scale_2x;
    double output_rescale_scale =
        max_scale_2x / (output_scale * static_cast<double>(1 << input_shift));

    Value op1_rescale_lhs =
        buildRescaleToInt32(rewriter, op, tfl_add_op.lhs(), lhs_rescale_scale,
                            input_lhs_qtype.getZeroPoint());
    Value op2_rescale_rhs =
        buildRescaleToInt32(rewriter, op, tfl_add_op.rhs(), rhs_rescale_scale,
                            input_rhs_qtype.getZeroPoint());
    auto op3_add_op1_op2 = CreateOpAndInfer<TosaOp>(
        rewriter, op->getLoc(), rescale_type, op1_rescale_lhs, op2_rescale_rhs);
    Value op4_rescale_op3 = buildRescaleFromInt32(
        rewriter, op, output_type, op3_add_op1_op2.getResult(),
        output_rescale_scale, output_qtype.getZeroPoint());
    output = op4_rescale_op3;
  } else {
    auto op1_add_in =
        CreateOpAndInfer<TosaOp>(rewriter, op->getLoc(), output_type,
                                 tfl_add_op.lhs(), tfl_add_op.rhs());

    output = op1_add_in.getResult();
  }

  auto fused_activation_fn = tfl_add_op.fused_activation_functionAttr();

  if (fused_activation_fn) {
    llvm::Optional<Value> fused_activation_val =
        convertFusedActivation(rewriter, op, output, fused_activation_fn);

    if (!fused_activation_val) return failure();

    rewriter.replaceOp(op, {fused_activation_val.getValue()});
    return success();
  }

  rewriter.replaceOp(op, {output});
  return success();
}

LogicalResult ConvertTFLAddOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  return matchAndRewriteAddSub<TFL::AddOp, tosa::AddOp>(op, op->getOperands(),
                                                        rewriter);
}

LogicalResult ConvertTFLSubOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  return matchAndRewriteAddSub<TFL::SubOp, tosa::SubOp>(op, op->getOperands(),
                                                        rewriter);
}

LogicalResult ConvertTFLMulOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_mul_op = cast<TFL::MulOp>(op);

  llvm::Optional<Value> result = convertMultiplyOp(
      rewriter, op, tfl_mul_op.getResult(), tfl_mul_op.lhs(), tfl_mul_op.rhs());

  if (!result) return failure();

  auto fused_activation_fn = tfl_mul_op.fused_activation_functionAttr();

  if (fused_activation_fn) {
    llvm::Optional<Value> fused_activation_val = convertFusedActivation(
        rewriter, op, result.getValue(), fused_activation_fn);

    if (!fused_activation_val) return failure();

    rewriter.replaceOp(op, {fused_activation_val.getValue()});
    return success();
  }

  rewriter.replaceOp(op, {result.getValue()});
  return success();
}

LogicalResult ConvertTFLSquareOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_square_op = cast<TFL::SquareOp>(op);

  llvm::Optional<Value> result =
      convertMultiplyOp(rewriter, op, tfl_square_op.getResult(),
                        tfl_square_op.x(), tfl_square_op.x());

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});
  return success();
}

LogicalResult ConvertTFLSquaredDifferenceOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_squared_op = cast<TFL::SquaredDifferenceOp>(op);

  llvm::Optional<Value> result =
      convertSquaredDifferenceOp(rewriter, op, tfl_squared_op.getResult(),
                                 tfl_squared_op.lhs(), tfl_squared_op.rhs());

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});
  return success();
}

LogicalResult ConvertTFLRoundOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_round_op = cast<TFL::RoundOp>(op);

  ShapedType input_type = tfl_round_op.x().getType().dyn_cast<ShapedType>();
  if (!input_type) {
    return op->emitOpError("Round: input not shaped tensor type");
  }

  if (input_type.getElementType().isa<FloatType>()) {
    llvm::Optional<Value> result = convertRoundOp(
        rewriter, op, tfl_round_op.getResult(), tfl_round_op.x());

    if (!result) return failure();

    rewriter.replaceOp(op, {result.getValue()});
    return success();

  } else {
    // Round on int is nonsensical. Instead, replace uses of result with the
    // input.
    tfl_round_op.replaceAllUsesWith(tfl_round_op.x());
    return success();
  }
}

LogicalResult ConvertTFLDivOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_div_op = cast<TFL::DivOp>(op);

  ShapedType output_type =
      tfl_div_op.getResult().getType().dyn_cast<ShapedType>();
  // Not a ranked tensor output
  if (!output_type) return failure();

  auto fused_activation_fn = tfl_div_op.fused_activation_functionAttr();

  Type element_type = output_type.getElementType();
  Value div_op;
  if (element_type.isa<IntegerType>()) {
    div_op = CreateOpAndInfer<tosa::DivOp>(rewriter, op->getLoc(), output_type,
                                           tfl_div_op.lhs(), tfl_div_op.rhs())
                 .getResult();
  } else {
    auto reciprocal_op = CreateOpAndInfer<tosa::ReciprocalOp>(
        rewriter, op->getLoc(), tfl_div_op.rhs().getType(), tfl_div_op.rhs());
    div_op = CreateOpAndInfer<tosa::MulOp>(rewriter, op->getLoc(), output_type,
                                           tfl_div_op.lhs(),
                                           reciprocal_op.getResult(), 0)
                 .getResult();
  }

  if (fused_activation_fn) {
    llvm::Optional<Value> fused_activation_val =
        convertFusedActivation(rewriter, op, div_op, fused_activation_fn);

    if (!fused_activation_val) return failure();

    rewriter.replaceOp(op, {fused_activation_val.getValue()});
    return success();
  }

  rewriter.replaceOp(op, {div_op});

  return success();
}

LogicalResult ConvertTFLMaximumOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_max_op = cast<TFL::MaximumOp>(op);

  ShapedType input_lhs_type = tfl_max_op.lhs().getType().dyn_cast<ShapedType>();
  ShapedType input_rhs_type = tfl_max_op.rhs().getType().dyn_cast<ShapedType>();
  ShapedType output_type =
      tfl_max_op.getResult().getType().dyn_cast<ShapedType>();

  // Not a shaped tensor output
  if (!input_lhs_type || !input_rhs_type || !output_type) return failure();

  bool input_lhs_is_qtype =
      input_lhs_type.getElementType().isa<mlir::quant::UniformQuantizedType>();
  bool input_rhs_is_qtype =
      input_rhs_type.getElementType().isa<mlir::quant::UniformQuantizedType>();
  bool output_is_qtype =
      output_type.getElementType().isa<mlir::quant::UniformQuantizedType>();

  if (input_lhs_is_qtype != output_is_qtype ||
      input_rhs_is_qtype != output_is_qtype) {
    return op->emitOpError(
        "ConvertTFLMaximumOp: input/output tensor should "
        "be all quantized or all floating-point.");
  }

  Value output;
  if (output_is_qtype) {
    ShapedType rescale_type = output_type.clone(rewriter.getI32Type());

    Value op1_rescale_lhs =
        buildRescaleToInt32(rewriter, op, tfl_max_op.lhs(), 1.0f, 0);
    Value op2_rescale_rhs =
        buildRescaleToInt32(rewriter, op, tfl_max_op.rhs(), 1.0f, 0);
    auto op3_max_op1_op2 = CreateOpAndInfer<tosa::MaximumOp>(
        rewriter, op->getLoc(), rescale_type, op1_rescale_lhs, op2_rescale_rhs);
    Value op4_rescale_op3 = buildRescaleFromInt32(
        rewriter, op, output_type, op3_max_op1_op2.getResult(), 1.0f, 0);

    output = op4_rescale_op3;
  } else {
    auto op1_max_in =
        CreateOpAndInfer<tosa::MaximumOp>(rewriter, op->getLoc(), output_type,
                                          tfl_max_op.lhs(), tfl_max_op.rhs());

    output = op1_max_in.getResult();
  }

  rewriter.replaceOp(op, {output});

  return success();
}

LogicalResult ConvertTFLMinimumOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_min_op = cast<TFL::MinimumOp>(op);

  ShapedType input_lhs_type = tfl_min_op.lhs().getType().dyn_cast<ShapedType>();
  ShapedType input_rhs_type = tfl_min_op.rhs().getType().dyn_cast<ShapedType>();
  ShapedType output_type =
      tfl_min_op.getResult().getType().dyn_cast<ShapedType>();
  // Not a shaped tensor output
  if (!input_lhs_type || !input_rhs_type || !output_type) return failure();

  bool input_lhs_is_qtype =
      input_lhs_type.getElementType().isa<mlir::quant::UniformQuantizedType>();
  bool input_rhs_is_qtype =
      input_rhs_type.getElementType().isa<mlir::quant::UniformQuantizedType>();
  bool output_is_qtype =
      output_type.getElementType().isa<mlir::quant::UniformQuantizedType>();

  if (input_lhs_is_qtype != output_is_qtype ||
      input_rhs_is_qtype != output_is_qtype) {
    return op->emitOpError(
        "ConvertTFLMinimumOp: input/output tensor should "
        "be all quantized or all floating-point.");
  }

  Value output;
  if (output_is_qtype) {
    ShapedType rescale_type = output_type.clone(rewriter.getI32Type());

    Value op1_rescale_lhs =
        buildRescaleToInt32(rewriter, op, tfl_min_op.lhs(), 1.0f, 0);
    Value op2_rescale_rhs =
        buildRescaleToInt32(rewriter, op, tfl_min_op.rhs(), 1.0f, 0);
    auto op3_min_op1_op2 = CreateOpAndInfer<tosa::MinimumOp>(
        rewriter, op->getLoc(), rescale_type, op1_rescale_lhs, op2_rescale_rhs);
    Value op4_rescale_op3 = buildRescaleFromInt32(
        rewriter, op, output_type, op3_min_op1_op2.getResult(), 1.0f, 0);

    output = op4_rescale_op3;
  } else {
    auto op1_min_in =
        CreateOpAndInfer<tosa::MinimumOp>(rewriter, op->getLoc(), output_type,
                                          tfl_min_op.lhs(), tfl_min_op.rhs());

    output = op1_min_in.getResult();
  }

  rewriter.replaceOp(op, {output});

  return success();
}

LogicalResult ConvertTFLFloorDivOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_floordiv_op = cast<TFL::FloorDivOp>(op);

  llvm::Optional<Value> result =
      convertFloorDivOp(rewriter, op, tfl_floordiv_op.getResult(),
                        tfl_floordiv_op.lhs(), tfl_floordiv_op.rhs());

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLFloorModOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_floormod_op = cast<TFL::FloorModOp>(op);

  llvm::Optional<Value> result =
      convertFloorModOp(rewriter, op, tfl_floormod_op.getResult(),
                        tfl_floormod_op.lhs(), tfl_floormod_op.rhs());

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLAddNOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_addn_op = cast<TFL::AddNOp>(op);

  ShapedType output_type =
      tfl_addn_op.getResult().getType().dyn_cast<ShapedType>();
  // Not a shaped output
  if (!output_type) return failure();

  SmallVector<Value> inputs(tfl_addn_op.inputs());

  assert(inputs.size() >= 2);

  auto newOp = CreateOpAndInfer<tosa::AddOp>(rewriter, op->getLoc(),
                                             output_type, inputs[0], inputs[1]);
  for (int i = 2; i < inputs.size(); i++) {
    newOp = CreateOpAndInfer<tosa::AddOp>(rewriter, op->getLoc(), output_type,
                                          inputs[i], newOp.getResult());
  }

  rewriter.replaceOp(op, {newOp.getResult()});

  return success();
}

LogicalResult ConvertTFLAveragePool2DOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_avgpool_op = cast<TFL::AveragePool2DOp>(op);

  ShapedType input_type =
      tfl_avgpool_op.input().getType().dyn_cast<ShapedType>();
  ShapedType output_type =
      tfl_avgpool_op.getResult().getType().dyn_cast<ShapedType>();
  // Not a shaped output
  if (!output_type) return failure();

  // Kernels and strides are dimensionally ordered
  SmallVector<int64_t, 4> i64array({1, 1, 1, 1});
  ArrayAttr kernel_size;
  ArrayAttr stride;
  ArrayAttr pad;
  {
    int64_t kernel_h = tfl_avgpool_op.filter_height();
    int64_t kernel_w = tfl_avgpool_op.filter_width();
    kernel_size = rewriter.getI64ArrayAttr({kernel_h, kernel_w});
    // i64array is formatted as NHWC now
    i64array[1] = kernel_h;
    i64array[2] = kernel_w;
  }
  {
    int64_t stride_h = tfl_avgpool_op.stride_h();
    int64_t stride_w = tfl_avgpool_op.stride_w();
    stride = rewriter.getI64ArrayAttr({stride_h, stride_w});
  }
  {
    tensorflow::Padding tf_pad;
    if (!GetPaddingFromString(tfl_avgpool_op.padding().str(), &tf_pad).ok())
      return failure();

    // Pooling has no non-unit dilation
    ArrayAttr dilation = rewriter.getI64ArrayAttr({1, 1});

    RankedTensorType filter_type = RankedTensorType::get(
        llvm::makeArrayRef(i64array), rewriter.getIntegerType(64));

    // TFLite doesn't support explicit padding
    if (!getPaddingValuesFromPadType(
            tf_pad,
            tensorflow::FORMAT_NHWC,  // TFLite only supports this
            1,                        // tensorflow::FORMAT_OHWI,
            input_type, filter_type, stride, dilation, rewriter, pad))
      return failure();
  }

  CreateReplaceOpAndInfer<tosa::AvgPool2dOp>(rewriter, op, output_type,
                                             tfl_avgpool_op.input(),
                                             kernel_size, stride, pad);
  return success();
}

LogicalResult ConvertTFLMaxPool2DOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_maxpool_op = cast<TFL::MaxPool2DOp>(op);

  ShapedType input_type =
      tfl_maxpool_op.input().getType().dyn_cast<ShapedType>();
  ShapedType output_type =
      tfl_maxpool_op.getResult().getType().dyn_cast<ShapedType>();
  // Not a shaped type
  if (!output_type) return failure();

  // Kernels and strides are dimensionally ordered
  SmallVector<int64_t, 4> i64array({1, 1, 1, 1});
  ArrayAttr kernel_size;
  ArrayAttr stride;
  ArrayAttr pad;
  {
    int64_t kernel_h = tfl_maxpool_op.filter_height();
    int64_t kernel_w = tfl_maxpool_op.filter_width();
    kernel_size = rewriter.getI64ArrayAttr({kernel_h, kernel_w});
    // i64array is formatted as NHWC now
    i64array[1] = kernel_h;
    i64array[2] = kernel_w;
  }
  {
    int64_t stride_h = tfl_maxpool_op.stride_h();
    int64_t stride_w = tfl_maxpool_op.stride_w();
    stride = rewriter.getI64ArrayAttr({stride_h, stride_w});
  }
  {
    tensorflow::Padding tf_pad;
    if (!GetPaddingFromString(tfl_maxpool_op.padding().str(), &tf_pad).ok())
      return failure();

    // Pooling has no non-unit dilation
    ArrayAttr dilation = rewriter.getI64ArrayAttr({1, 1});

    RankedTensorType filter_type =
        RankedTensorType::get(i64array, rewriter.getIntegerType(64));

    // TFLite doesn't support explicit padding
    if (!getPaddingValuesFromPadType(
            tf_pad,
            tensorflow::FORMAT_NHWC,  // TFLite only supports this
            1,                        // tensorflow::FORMAT_OHWI,
            input_type, filter_type, stride, dilation, rewriter, pad))
      return failure();
  }

  CreateReplaceOpAndInfer<tosa::MaxPool2dOp>(rewriter, op, output_type,
                                             tfl_maxpool_op.input(),
                                             kernel_size, stride, pad);
  return success();
}

LogicalResult ConvertTFLConv2DOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_conv2d_op = cast<TFL::Conv2DOp>(op);

  RankedTensorType input_type =
      tfl_conv2d_op.input().getType().dyn_cast<RankedTensorType>();
  RankedTensorType filter_type =
      tfl_conv2d_op.filter().getType().dyn_cast<RankedTensorType>();
  ShapedType output_type =
      tfl_conv2d_op.getResult().getType().dyn_cast<ShapedType>();
  // Not a ranked tensor output
  if (!input_type) return failure();
  if (!output_type) return failure();
  if (!filter_type) return failure();

  bool input_is_qtype =
      input_type.getElementType().isa<mlir::quant::QuantizedType>();
  bool filter_is_qtype =
      filter_type.getElementType().isa<mlir::quant::QuantizedType>();
  bool output_is_qtype =
      output_type.getElementType().isa<mlir::quant::QuantizedType>();

  if ((input_is_qtype != filter_is_qtype) ||
      (input_is_qtype != output_is_qtype)) {
    return op->emitOpError(
        "ConvertTFLConv2DOp: input/filter/output tensor should "
        "be all quantized or all floating-point.");
  }

  ArrayAttr pad;
  ArrayAttr stride;
  ArrayAttr dilation;
  {
    int64_t stride_h = tfl_conv2d_op.stride_h();
    int64_t stride_w = tfl_conv2d_op.stride_w();
    stride = rewriter.getI64ArrayAttr({stride_h, stride_w});
  }
  {
    int64_t dilation_h = tfl_conv2d_op.dilation_h_factor();
    int64_t dilation_w = tfl_conv2d_op.dilation_w_factor();
    dilation = rewriter.getI64ArrayAttr({dilation_h, dilation_w});
  }
  {
    tensorflow::Padding tf_pad;
    if (!GetPaddingFromString(tfl_conv2d_op.padding().str(), &tf_pad).ok())
      return failure();

    // TFLite doesn't support explicit padding
    if (!getPaddingValuesFromPadType(
            tf_pad,
            tensorflow::FORMAT_NHWC,  // TFLite only supports this
            1,                        // tensorflow::FORMAT_OHWI,
            input_type, filter_type, stride, dilation, rewriter, pad))
      return failure();
  }

  Value unquantized_bias = tfl_conv2d_op.bias();
  Type bias_ety =
      output_is_qtype ? rewriter.getI32Type() : output_type.getElementType();
  if (unquantized_bias)
    bias_ety = unquantized_bias.getType().cast<ShapedType>().getElementType();

  auto a1_conv2d_op = CreateOpAndInfer<tosa::Conv2DOp>(
      rewriter, op->getLoc(), output_type.clone(bias_ety),
      tfl_conv2d_op.input(), tfl_conv2d_op.filter(), unquantized_bias, pad,
      stride, dilation);

  Value conv2d_output;
  if (input_is_qtype) {
    conv2d_output =
        buildRescaleOpConvOutput(rewriter, op, a1_conv2d_op.getResult(),
                                 input_type, filter_type, output_type);
  } else {
    conv2d_output = a1_conv2d_op.getResult();
  }

  auto fused_activation_fn = tfl_conv2d_op.fused_activation_functionAttr();

  if (fused_activation_fn) {
    llvm::Optional<Value> fused_activation_val = convertFusedActivation(
        rewriter, op, conv2d_output, fused_activation_fn);

    if (!fused_activation_val) return failure();

    rewriter.replaceOp(op, {fused_activation_val.getValue()});
    return success();
  }

  rewriter.replaceOp(op, {conv2d_output});

  return success();
}

LogicalResult ConvertTFLTransposeConvOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_conv_op = cast<TFL::TransposeConvOp>(op);

  ShapedType input_type = tfl_conv_op.input().getType().dyn_cast<ShapedType>();
  ShapedType filter_type =
      tfl_conv_op.weights().getType().dyn_cast<ShapedType>();
  ShapedType output_type =
      tfl_conv_op.getResult().getType().dyn_cast<ShapedType>();
  // Not a ranked tensor output
  if (!input_type) return failure();
  if (!output_type) return failure();
  if (!filter_type) return failure();

  bool input_is_qtype =
      input_type.getElementType().isa<mlir::quant::QuantizedType>();
  bool filter_is_qtype =
      filter_type.getElementType().isa<mlir::quant::QuantizedType>();
  bool output_is_qtype =
      output_type.getElementType().isa<mlir::quant::QuantizedType>();

  if ((input_is_qtype != filter_is_qtype) ||
      (input_is_qtype != output_is_qtype)) {
    return op->emitOpError(
        "ConvertTFLConv2DOp: input/filter/output tensor should "
        "be all quantized or all floating-point.");
  }

  ArrayAttr stride;
  ArrayAttr dilation;
  ArrayAttr outpad;
  ArrayAttr output_shape;
  {
    int64_t stride_h = tfl_conv_op.stride_h();
    int64_t stride_w = tfl_conv_op.stride_w();
    stride = rewriter.getI64ArrayAttr({stride_h, stride_w});
  }

  // tfl.transpose_conv doesn't support dilations
  dilation = rewriter.getI64ArrayAttr({1, 1});

  {
    tensorflow::Padding tf_pad;
    if (!GetPaddingFromString(tfl_conv_op.padding().str(), &tf_pad).ok())
      return failure();

    if (!getTransposeConv2dPaddingValues(
            tf_pad,
            tensorflow::FORMAT_NHWC,  // TFLite only supports this
            1,                        // tensorflow::FORMAT_OHWI,
            input_type, filter_type, output_type, stride, dilation, rewriter,
            outpad))
      return failure();
  }
  {
    ElementsAttr output_shape_elems;
    // Match from input_size tensor first
    if (matchPattern(tfl_conv_op.output_shape(),
                     m_Constant(&output_shape_elems))) {
      SmallVector<int64_t> shape_vec;
      for (int i = 0; i < output_shape_elems.getNumElements(); i++)
        shape_vec.push_back(
            output_shape_elems.getValue<IntegerAttr>(i).getInt());
      output_shape = rewriter.getI64ArrayAttr(shape_vec);
    } else if (output_type.hasRank()) {
      // Use output tensor's shape otherwise
      output_shape = rewriter.getI64ArrayAttr(output_type.getShape());
    } else {
      // TODO(suderman): Figure out rankless shape propagation.
      return failure();
    }
  }

  int output_channel = 0;
  // TODO(suderman): We need to figure out how to guarantee output channel
  // propagation.
  if (output_type.hasRank()) {
    output_channel = output_type.getDimSize(3);
  } else if (filter_type.hasRank()) {
    output_channel = filter_type.getDimSize(0);
  } else {
    return failure();
  }

  llvm::Optional<Value> zero_bias;
  if (input_is_qtype) {
    uint32_t input_bits = input_type.getElementType()
                              .dyn_cast<mlir::quant::QuantizedType>()
                              .getStorageTypeIntegralWidth();
    uint32_t weight_bits = filter_type.getElementType()
                               .dyn_cast<mlir::quant::QuantizedType>()
                               .getStorageTypeIntegralWidth();

    if (input_bits == 16 && weight_bits == 8) {
      SmallVector<APInt> vec(output_channel, APInt(48, 0, true));
      zero_bias = getConstTensor<APInt>(rewriter, op, vec, {output_channel});
    } else {
      SmallVector<int32_t> vec(output_channel, 0);
      zero_bias = getConstTensor<int32_t>(rewriter, op, vec, {output_channel});
    }
  } else {
    SmallVector<float> vec(output_channel, 0.0f);
    zero_bias = getConstTensor<float>(rewriter, op, vec, {output_channel});
  }

  if (!zero_bias) return failure();
  Type bias_ety = zero_bias->getType().cast<ShapedType>().getElementType();

  auto a1_conv2d_op = CreateOpAndInfer<tosa::TransposeConv2DOp>(
      rewriter, op->getLoc(), output_type.clone(bias_ety), tfl_conv_op.input(),
      tfl_conv_op.weights(), zero_bias.getValue(), outpad, stride, dilation,
      output_shape);

  Value conv2d_output;
  if (input_is_qtype) {
    conv2d_output =
        buildRescaleOpConvOutput(rewriter, op, a1_conv2d_op.getResult(),
                                 input_type, filter_type, output_type);
  } else {
    conv2d_output = a1_conv2d_op.getResult();
  }

  rewriter.replaceOp(op, {conv2d_output});

  return success();
}

LogicalResult ConvertTFLDepthwiseConv2DOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_conv2d_op = cast<TFL::DepthwiseConv2DOp>(op);

  ShapedType input_type =
      tfl_conv2d_op.input().getType().dyn_cast<ShapedType>();
  ShapedType filter_type =
      tfl_conv2d_op.filter().getType().dyn_cast<ShapedType>();
  ShapedType output_type =
      tfl_conv2d_op.getResult().getType().dyn_cast<ShapedType>();
  // Not a shaped output
  if (!input_type) return failure();
  if (!output_type) return failure();
  if (!filter_type) return failure();

  bool input_is_qtype =
      input_type.getElementType().isa<mlir::quant::QuantizedType>();
  bool filter_is_qtype =
      filter_type.getElementType().isa<mlir::quant::QuantizedType>();
  bool output_is_qtype =
      output_type.getElementType().isa<mlir::quant::QuantizedType>();

  if ((input_is_qtype != filter_is_qtype) ||
      (input_is_qtype != output_is_qtype)) {
    return op->emitOpError(
        "ConvertTFLConv2DOp: input/filter/output tensor should "
        "be all quantized or all floating-point.");
  }

  // We need the filter shape to compute the transpose.
  if (!filter_type.hasRank()) return failure();
  auto filter_shape = filter_type.getShape();
  // Operator depthwiseConv2D
  // TFLite orders the depthwiseConv2D filter in IHWO, while TOSA orders
  // filter in HWIO
  //
  // The lowering reorders the filter.
  //
  // a1_transpose = tosa.transpose(filter, {1, 2, 3, 0})   // HWIO
  // a2_reshape = tosa.reshape(filter, H, W, depth_multiplier, I /
  // depth_multiplier)
  // a3_transpose_conv2d = tosa.transpose_conv2d(input, a2_reshape, padding,
  // stride, dilation)

  ArrayAttr pad;
  ArrayAttr stride;
  ArrayAttr dilation;
  auto depth_multiplier = tfl_conv2d_op.depth_multiplierAttr();

  {
    int64_t stride_h = tfl_conv2d_op.stride_h();
    int64_t stride_w = tfl_conv2d_op.stride_w();
    stride = rewriter.getI64ArrayAttr({stride_h, stride_w});
  }
  {
    int64_t dilation_h = tfl_conv2d_op.dilation_h_factor();
    int64_t dilation_w = tfl_conv2d_op.dilation_w_factor();
    dilation = rewriter.getI64ArrayAttr({dilation_h, dilation_w});
  }
  {
    tensorflow::Padding tf_pad;
    if (!GetPaddingFromString(tfl_conv2d_op.padding().str(), &tf_pad).ok())
      return failure();

    if (!getPaddingValuesFromPadType(
            tf_pad,
            tensorflow::FORMAT_NHWC,  // TFLite only supports this
            1,                        // tensorflow::FORMAT_OHWI,
            input_type, filter_type, stride, dilation, rewriter, pad))
      return failure();
  }

  SmallVector<int64_t, 4> a1_transpose_dims;
  a1_transpose_dims.push_back(filter_shape[1]);
  a1_transpose_dims.push_back(filter_shape[2]);
  a1_transpose_dims.push_back(filter_shape[3]);
  a1_transpose_dims.push_back(filter_shape[0]);

  SmallVector<int64_t, 4> a2_reshape_dims;
  a2_reshape_dims.push_back(a1_transpose_dims[0]);
  a2_reshape_dims.push_back(a1_transpose_dims[1]);
  a2_reshape_dims.push_back(a1_transpose_dims[2] / depth_multiplier.getInt());
  a2_reshape_dims.push_back(depth_multiplier.getInt());

  llvm::Optional<Value> a1_filter_transpose_perms = getConstTensor<int32_t>(
      rewriter, op, /*vec=*/{1, 2, 3, 0}, /*shape=*/{4});

  if (!a1_filter_transpose_perms) return failure();

  auto a1_filter_transpose_op = CreateOpAndInfer<tosa::TransposeOp>(
      rewriter, op->getLoc(),
      RankedTensorType::get(ArrayRef<int64_t>(a1_transpose_dims),
                            filter_type.getElementType()),
      tfl_conv2d_op.filter(), a1_filter_transpose_perms.getValue());

  auto a2_filter_reshape_op = CreateOpAndInfer<tosa::ReshapeOp>(
      rewriter, op->getLoc(),
      RankedTensorType::get(ArrayRef<int64_t>(a2_reshape_dims),
                            filter_type.getElementType()),
      a1_filter_transpose_op.getResult(),
      rewriter.getI64ArrayAttr(a2_reshape_dims));

  Value unquantized_bias = tfl_conv2d_op.bias();
  Type bias_ety =
      output_is_qtype ? rewriter.getI32Type() : output_type.getElementType();
  if (unquantized_bias)
    bias_ety = unquantized_bias.getType().cast<ShapedType>().getElementType();

  auto a3_depthwise_conv2d_op = CreateOpAndInfer<tosa::DepthwiseConv2DOp>(
      rewriter, op->getLoc(), output_type.clone(bias_ety),
      tfl_conv2d_op.input(), a2_filter_reshape_op.getResult(), unquantized_bias,
      pad, stride, dilation);

  Value conv2d_output;
  if (input_is_qtype) {
    conv2d_output = buildRescaleOpConvOutput(
        rewriter, op, a3_depthwise_conv2d_op.getResult(), input_type,
        filter_type, output_type);
  } else {
    conv2d_output = a3_depthwise_conv2d_op.getResult();
  }

  auto fused_activation_fn = tfl_conv2d_op.fused_activation_functionAttr();

  if (fused_activation_fn) {
    llvm::Optional<Value> fused_activation_val = convertFusedActivation(
        rewriter, op, conv2d_output, fused_activation_fn);

    if (!fused_activation_val) return failure();

    rewriter.replaceOp(op, {fused_activation_val.getValue()});
    return success();
  }

  rewriter.replaceOp(op, {conv2d_output});

  return success();
}

LogicalResult ConvertTFLFullyConnectedOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_fc_op = cast<TFL::FullyConnectedOp>(op);

  ShapedType output_type =
      tfl_fc_op.getResult(0).getType().dyn_cast<ShapedType>();
  // Not a ranked tensor output
  if (!output_type) return failure();

  RankedTensorType input_type =
      tfl_fc_op.input().getType().dyn_cast<RankedTensorType>();
  RankedTensorType filter_type =
      tfl_fc_op.filter().getType().dyn_cast<RankedTensorType>();
  RankedTensorType bias_type =
      tfl_fc_op.bias().getType().dyn_cast<RankedTensorType>();
  if (!input_type || !filter_type) return failure();

  bool input_is_qtype =
      input_type.getElementType().isa<mlir::quant::QuantizedType>();
  bool filter_is_qtype =
      filter_type.getElementType().isa<mlir::quant::QuantizedType>();
  bool output_is_qtype =
      output_type.getElementType().isa<mlir::quant::QuantizedType>();

  if ((input_is_qtype != filter_is_qtype) ||
      (input_is_qtype != output_is_qtype)) {
    return op->emitOpError(
        "ConvertTFLFullyConnectedOp: input/filter/output tensor should "
        "be all quantized or all floating-point.");
  }

  Value input_val = tfl_fc_op.input();

  // tfl.fully_connected() can takes various dimension tensor as input
  // need to reshape it to rank 2 tensor, which tosa.fully_connected only
  // supports if input tensor is rank 4.  It's not always reshaping to (dim[0] *
  // dim[1], dim[2] * dim[3]).

  // In some networks it's reshaping to (dim[0], dim[1] * dim[2] * dim[3]) so a
  // more general way to determine the reshape's shape is by looking at filter's
  // shape[1].
  if (input_type.getRank() != 2) {
    int64_t num_elems = filter_type.getShape()[1];
    int64_t num_batch = input_type.getNumElements() / num_elems;
    SmallVector<int64_t, 2> shape_vals({num_batch, num_elems});

    RankedTensorType reshape_type =
        RankedTensorType::get(shape_vals, input_type.getElementType());
    auto reshape_op = CreateOpAndInfer<tosa::ReshapeOp>(
        rewriter, op->getLoc(), reshape_type, tfl_fc_op.input(),
        rewriter.getI64ArrayAttr(shape_vals));

    input_val = reshape_op.getResult();
  }

  Value bias_val;
  if (!bias_type) {
    // For some matmuls, the bias may actually be a "UnitType" which has no
    // value. TOSA requires bias to be an array of output_channel_count values,
    // so create a constant of the appropriate number and type of zeros.
    SmallVector<int64_t, 1> bias_shape({filter_type.getShape()[0]});
    RankedTensorType new_bias_type =
        RankedTensorType::get(bias_shape, input_type.getElementType());

    DenseElementsAttr bias_attr;
    if (input_type.getElementType().isa<FloatType>()) {
      SmallVector<float> bias_arr(bias_shape[0]);

      for (int i = 0; i < bias_shape[0]; i++) {
        bias_arr[i] = 0.0;
      }
      bias_attr =
          DenseElementsAttr::get(new_bias_type, llvm::makeArrayRef(bias_arr));
    } else {
      SmallVector<int32_t> bias_arr(bias_shape[0]);

      for (int i = 0; i < bias_shape[0]; i++) {
        bias_arr[i] = 0;
      }
      bias_attr =
          DenseElementsAttr::get(new_bias_type, llvm::makeArrayRef(bias_arr));
    }
    auto bias_op = CreateOpAndInfer<tosa::ConstOp>(rewriter, op->getLoc(),
                                                   new_bias_type, bias_attr);
    bias_val = bias_op.getResult();
    bias_type = new_bias_type;
  } else {
    bias_val = tfl_fc_op.bias();
  }

  Type bias_ety = bias_val.getType().cast<ShapedType>().getElementType();

  auto fc_op = CreateOpAndInfer<tosa::FullyConnectedOp>(
      rewriter, op->getLoc(), UnrankedTensorType::get(bias_ety), input_val,
      tfl_fc_op.filter(), bias_val);

  Value fc_output;
  if (input_is_qtype) {
    fc_output = buildRescaleOpConvOutput(rewriter, op, fc_op.getResult(),
                                         input_type, filter_type, output_type);
  } else {
    fc_output = fc_op.getResult();
  }

  // If we know the output rank, we need to ensure the output shape is correct.
  ShapedType fc_type = fc_output.getType().cast<ShapedType>();
  if (output_type.hasRank() && fc_type.getRank() != output_type.getRank()) {
    llvm::SmallVector<int64_t> output_shape;

    fc_output = CreateOpAndInfer<tosa::ReshapeOp>(
        rewriter, op->getLoc(),
        UnrankedTensorType::get(fc_type.getElementType()), fc_output,
        rewriter.getI64ArrayAttr(output_type.getShape()));
  }

  auto fused_activation_fn = tfl_fc_op.fused_activation_functionAttr();

  if (fused_activation_fn) {
    llvm::Optional<Value> fused_activation_val =
        convertFusedActivation(rewriter, op, fc_output, fused_activation_fn);

    if (!fused_activation_val) return failure();

    rewriter.replaceOp(op, {fused_activation_val.getValue()});
    return success();
  }

  rewriter.replaceOp(op, {fc_output});

  return success();
}

LogicalResult ConvertTFLConcatenationOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_concat_op = cast<TFL::ConcatenationOp>(op);

  SmallVector<Value> values(tfl_concat_op.values());

  IntegerAttr axis_attr;
  {
    auto tmpAttr = tfl_concat_op.axisAttr();
    if (!tmpAttr) {
      tmpAttr = rewriter.getI64IntegerAttr(0);
    }
    axis_attr = tmpAttr;
  }
  int32_t axis = axis_attr.getInt();

  llvm::Optional<Value> result =
      convertConcatV2Op(rewriter, op, tfl_concat_op.getResult(), values, axis);

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});
  return success();
}

LogicalResult ConvertTFLReshapeOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_reshape_op = cast<TFL::ReshapeOp>(op);

  ShapedType output_type =
      tfl_reshape_op.getResult().getType().dyn_cast<ShapedType>();
  // Not a shaped tensor output
  if (!output_type) return failure();

  SmallVector<int64_t> shape_vals;

  // Either the output type needs to be ranked or we need a constant input
  // to compute the output rank.
  ElementsAttr shape_attr;
  if (!matchPattern(tfl_reshape_op.shape(), m_Constant(&shape_attr))) {
    if (!output_type.hasRank()) return failure();
    shape_vals.resize(output_type.getRank(), -1);
  } else {
    for (auto dim : shape_attr.getValues<int32_t>()) shape_vals.push_back(dim);
  }

  // Propagate the agreement between the output shape and constant value.
  if (output_type.hasRank()) {
    if (output_type.getRank() != shape_vals.size()) return failure();
    for (int i = 0; i < output_type.getRank(); i++) {
      if (shape_vals[i] == -1) shape_vals[i] = output_type.getDimSize(i);
    }
  }

  // We cannot handle more than 1 dynamic dimension.
  int64_t dynamic_count = 0;
  for (auto val : shape_vals)
    if (val == -1) dynamic_count++;
  if (dynamic_count > 1) return failure();

  ArrayAttr new_shape_attr = rewriter.getI64ArrayAttr(shape_vals);
  CreateReplaceOpAndInfer<tosa::ReshapeOp>(
      rewriter, op, output_type, tfl_reshape_op.input(), new_shape_attr);
  return success();
}

LogicalResult ConvertTFLRankOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_rank_op = cast<TFL::RankOp>(op);

  RankedTensorType input_type =
      tfl_rank_op.input().getType().dyn_cast<RankedTensorType>();
  if (!input_type) return failure();

  int32_t rank = input_type.getRank();

  RankedTensorType rank_type =
      RankedTensorType::get({1}, rewriter.getIntegerType(32));
  auto rank_attr = DenseElementsAttr::get(rank_type, {rank});
  auto rank_const = CreateOpAndInfer<tosa::ConstOp>(rewriter, op->getLoc(),
                                                    rank_type, rank_attr);

  rewriter.replaceOp(op, {rank_const.getResult()});

  return success();
}

LogicalResult ConvertTFLShapeOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_shape_op = cast<TFL::ShapeOp>(op);

  RankedTensorType output_type =
      tfl_shape_op.getResult().getType().dyn_cast<RankedTensorType>();
  // Not a ranked tensor output
  if (!output_type) return failure();

  RankedTensorType input_type =
      tfl_shape_op.input().getType().dyn_cast<RankedTensorType>();
  if (!input_type || !input_type.hasStaticShape())
    return rewriter.notifyMatchFailure(op, "input shape not static");

  auto input_shape = input_type.getShape();

  SmallVector<int32_t> shape_arr;
  for (int i = 0; i < input_shape.size(); i++) {
    shape_arr.emplace_back(input_shape[i]);
  }

  RankedTensorType shape_type = RankedTensorType::get(
      {static_cast<int32_t>(shape_arr.size())}, rewriter.getIntegerType(32));
  auto shape_attr =
      DenseElementsAttr::get(shape_type, llvm::makeArrayRef(shape_arr));
  auto shape_const = CreateOpAndInfer<tosa::ConstOp>(rewriter, op->getLoc(),
                                                     shape_type, shape_attr);

  rewriter.replaceOp(op, {shape_const.getResult()});

  return success();
}

LogicalResult ConvertTFLExpandDimsOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_expanddims_op = cast<TFL::ExpandDimsOp>(op);

  llvm::Optional<Value> result =
      convertExpandDimsOp(rewriter, op, tfl_expanddims_op.getResult(),
                          tfl_expanddims_op.input(), tfl_expanddims_op.dim());

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLSqueezeOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_squeeze_op = cast<TFL::SqueezeOp>(op);

  // Copy squeeze_dims into int32_t array
  auto squeeze_dims_attr = tfl_squeeze_op.squeeze_dimsAttr();
  SmallVector<int32_t> squeeze_dims;
  for (auto& squeeze_dim : squeeze_dims_attr) {
    squeeze_dims.emplace_back(squeeze_dim.dyn_cast<IntegerAttr>().getInt());
  }

  llvm::Optional<Value> result =
      convertSqueezeOp(rewriter, op, tfl_squeeze_op.getResult(),
                       tfl_squeeze_op.input(), squeeze_dims);

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLFillOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_fill_op = cast<TFL::FillOp>(op);

  RankedTensorType output_type =
      tfl_fill_op.getResult().getType().dyn_cast<RankedTensorType>();
  // Not a ranked tensor output
  if (!output_type) return failure();

  ElementsAttr dims_elems;
  if (!matchPattern(tfl_fill_op.dims(), m_Constant(&dims_elems)))
    return failure();
  SmallVector<int64_t> dims_vals;
  uint32_t total_size = 1;
  for (int i = 0; i < dims_elems.getNumElements(); i++) {
    dims_vals.push_back(dims_elems.getValue<IntegerAttr>(i).getInt());
    total_size *= dims_vals[i];
  }

  ElementsAttr value_elem;
  if (!matchPattern(tfl_fill_op.input(), m_Constant(&value_elem)))
    return failure();

  RankedTensorType fill_type = RankedTensorType::get(
      ArrayRef<int64_t>(dims_vals), value_elem.getType().getElementType());
  DenseElementsAttr fill_attr;

  // Convert to a compatible zero type.
  if (value_elem.getType().getElementType().isa<FloatType>()) {
    SmallVector<float> fill_arr(
        total_size,
        value_elem.getValue<FloatAttr>(0).getValue().convertToFloat());
    fill_attr = DenseElementsAttr::get(fill_type, llvm::makeArrayRef(fill_arr));
  } else {
    SmallVector<int32_t> fill_arr(
        total_size,
        value_elem.getValue<IntegerAttr>(0).getValue().getLimitedValue());
    fill_attr = DenseElementsAttr::get(fill_type, llvm::makeArrayRef(fill_arr));
  }
  auto fill_const_op = CreateOpAndInfer<tosa::ConstOp>(rewriter, op->getLoc(),
                                                       fill_type, fill_attr);
  rewriter.replaceOp(op, {fill_const_op.getResult()});

  return success();
}

LogicalResult ConvertTFLReduceAnyOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_any_op = cast<TFL::ReduceAnyOp>(op);

  RankedTensorType output_type =
      tfl_any_op.getResult().getType().dyn_cast<RankedTensorType>();
  if (!output_type) return failure();

  ElementsAttr axes_elems;
  if (!matchPattern(tfl_any_op.reduction_indices(), m_Constant(&axes_elems)))
    return failure();

  bool keep_dims = false;
  auto keep_dims_attr = tfl_any_op.keep_dimsAttr();
  if (keep_dims_attr) keep_dims = keep_dims_attr.getValue();

  llvm::Optional<Value> result = convertReduceAnyOp(
      rewriter, op, output_type, tfl_any_op.input(), axes_elems, keep_dims);

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLReduceMaxOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_max_op = cast<TFL::ReduceMaxOp>(op);

  RankedTensorType output_type =
      tfl_max_op.getResult().getType().dyn_cast<RankedTensorType>();
  if (!output_type) return failure();

  ElementsAttr axes_elems;
  if (!matchPattern(tfl_max_op.axes(), m_Constant(&axes_elems)))
    return failure();

  bool keep_dims = false;
  auto keep_dims_attr = tfl_max_op.keep_dimsAttr();
  if (keep_dims_attr) keep_dims = keep_dims_attr.getValue();

  llvm::Optional<Value> result = convertReduceMaxOp(
      rewriter, op, output_type, tfl_max_op.input(), axes_elems, keep_dims);

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLReduceMinOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_min_op = cast<TFL::ReduceMinOp>(op);

  RankedTensorType output_type =
      tfl_min_op.getResult().getType().dyn_cast<RankedTensorType>();
  if (!output_type) return failure();

  ElementsAttr axes_elems;
  if (!matchPattern(tfl_min_op.axes(), m_Constant(&axes_elems)))
    return failure();

  bool keep_dims = false;
  auto keep_dims_attr = tfl_min_op.keep_dimsAttr();
  if (keep_dims_attr) keep_dims = keep_dims_attr.getValue();

  llvm::Optional<Value> result = convertReduceMinOp(
      rewriter, op, output_type, tfl_min_op.input(), axes_elems, keep_dims);

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLReduceProdOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_prod_op = cast<TFL::ReduceProdOp>(op);

  RankedTensorType output_type =
      tfl_prod_op.getResult().getType().dyn_cast<RankedTensorType>();
  if (!output_type) return failure();

  ElementsAttr axes_elems;
  if (!matchPattern(tfl_prod_op.axes(), m_Constant(&axes_elems)))
    return failure();

  bool keep_dims = false;
  auto keep_dims_attr = tfl_prod_op.keep_dimsAttr();
  if (keep_dims_attr) keep_dims = keep_dims_attr.getValue();

  llvm::Optional<Value> result = convertReduceProdOp(
      rewriter, op, output_type, tfl_prod_op.input(), axes_elems, keep_dims);

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLMeanOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_mean_op = cast<TFL::MeanOp>(op);

  RankedTensorType output_type =
      tfl_mean_op.getResult().getType().dyn_cast<RankedTensorType>();
  if (!output_type) return failure();

  ElementsAttr axes_elems;
  if (!matchPattern(tfl_mean_op.axis(), m_Constant(&axes_elems)))
    return failure();

  bool keep_dims = false;
  auto keep_dims_attr = tfl_mean_op.keep_dimsAttr();
  if (keep_dims_attr) keep_dims = keep_dims_attr.getValue();

  llvm::Optional<Value> result = convertReduceMeanOp(
      rewriter, op, output_type, tfl_mean_op.input(), axes_elems, keep_dims);

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLSumOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_sum_op = cast<TFL::SumOp>(op);

  RankedTensorType output_type =
      tfl_sum_op.getResult().getType().dyn_cast<RankedTensorType>();
  if (!output_type) return failure();

  ElementsAttr axes_elems;
  if (!matchPattern(tfl_sum_op.axes(), m_Constant(&axes_elems)))
    return failure();

  bool keep_dims = false;
  auto keep_dims_attr = tfl_sum_op.keep_dimsAttr();
  if (keep_dims_attr) keep_dims = keep_dims_attr.getValue();

  llvm::Optional<Value> result = convertReduceSumOp(
      rewriter, op, output_type, tfl_sum_op.input(), axes_elems, keep_dims);

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLEluOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_elu_op = cast<TFL::EluOp>(op);

  llvm::Optional<Value> result =
      convertEluOp(rewriter, op, tfl_elu_op.getResult(), tfl_elu_op.x());

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLSoftmaxOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_softmax_op = cast<TFL::SoftmaxOp>(op);

  llvm::Optional<Value> result = convertSoftmaxOp(
      rewriter, op, tfl_softmax_op.getResult(), tfl_softmax_op.input(),
      tfl_softmax_op.betaAttr().getValueAsDouble());

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLSqrtOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_rsqrt_op = cast<TFL::SqrtOp>(op);
  auto rsqrt = CreateOpAndInfer<tosa::RsqrtOp>(
      rewriter, op->getLoc(), tfl_rsqrt_op.getType(), tfl_rsqrt_op.x());

  CreateReplaceOpAndInfer<tosa::ReciprocalOp>(rewriter, op, rsqrt.getType(),
                                              rsqrt);

  return success();
}

LogicalResult ConvertTFLLogSoftmaxOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_logsoftmax_op = cast<TFL::LogSoftmaxOp>(op);

  llvm::Optional<Value> result = convertLogSoftmaxOp(
      rewriter, op, tfl_logsoftmax_op.getResult(), tfl_logsoftmax_op.input());

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLSliceOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_slice_op = cast<TFL::SliceOp>(op);

  ShapedType output_type =
      tfl_slice_op.getResult().getType().dyn_cast<ShapedType>();
  // Not a shaped tensor output
  if (!output_type) return failure();

  ElementsAttr begin_elems, size_elems;

  SmallVector<int64_t> begin_vals, size_vals;

  if (!matchPattern(tfl_slice_op.begin(), m_Constant(&begin_elems)) ||
      !matchPattern(tfl_slice_op.size(), m_Constant(&size_elems))) {
    return failure();
  }

  for (int i = 0; i < begin_elems.getNumElements(); i++)
    begin_vals.push_back(begin_elems.getValue<IntegerAttr>(i).getInt());

  for (int i = 0; i < size_elems.getNumElements(); i++)
    size_vals.push_back(size_elems.getValue<IntegerAttr>(i).getInt());

  ArrayAttr begin = rewriter.getI64ArrayAttr(begin_vals);
  ArrayAttr size = rewriter.getI64ArrayAttr(size_vals);

  CreateReplaceOpAndInfer<tosa::SliceOp>(rewriter, op, output_type,
                                         tfl_slice_op.input(), begin, size);
  return success();
}

LogicalResult ConvertTFLTileOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_tile_op = cast<TFL::TileOp>(op);

  ShapedType output_type =
      tfl_tile_op.getResult().getType().dyn_cast<ShapedType>();
  // Not a ranked tensor output
  if (!output_type) return failure();

  ElementsAttr multiples_elems;
  if (!matchPattern(tfl_tile_op.multiples(), m_Constant(&multiples_elems)))
    return failure();
  SmallVector<int64_t> multiples_vals;
  for (int i = 0; i < multiples_elems.getNumElements(); i++)
    multiples_vals.push_back(multiples_elems.getValue<IntegerAttr>(i).getInt());

  ArrayAttr multiples_attr = rewriter.getI64ArrayAttr(multiples_vals);
  CreateReplaceOpAndInfer<tosa::TileOp>(rewriter, op, output_type,
                                        tfl_tile_op.input(), multiples_attr);

  return success();
}

LogicalResult ConvertTFLTransposeOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_transpose_op = cast<TFL::TransposeOp>(op);

  Type output_type = tfl_transpose_op.getResult().getType();
  CreateReplaceOpAndInfer<tosa::TransposeOp>(rewriter, op, output_type,
                                             tfl_transpose_op.input(),
                                             tfl_transpose_op.perm());

  return success();
}

LogicalResult ConvertTFLPackOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_pack_op = cast<TFL::PackOp>(op);

  SmallVector<Value> inputs(tfl_pack_op.values());
  assert(!inputs.empty());

  IntegerAttr axis_attr;
  {
    auto tmpAttr = tfl_pack_op.axisAttr();
    if (!tmpAttr) tmpAttr = rewriter.getI64IntegerAttr(0);
    axis_attr = tmpAttr;
  }
  int32_t axis_i32 = axis_attr.getInt();

  llvm::Optional<Value> result =
      convertPackOp(rewriter, op, tfl_pack_op.getResult(), inputs, axis_i32);

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLUnpackOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_unpack_op = cast<TFL::UnpackOp>(op);

  IntegerAttr axis_attr;
  {
    auto tmpAttr = tfl_unpack_op.axisAttr();
    if (!tmpAttr) tmpAttr = rewriter.getI64IntegerAttr(0);
    axis_attr = tmpAttr;
  }
  int32_t axis_i32 = axis_attr.getInt();

  llvm::Optional<SmallVector<Value>> results =
      convertUnpackOp(rewriter, op, tfl_unpack_op.input(), axis_i32);

  if (!results) return failure();

  rewriter.replaceOp(op, results.getValue());

  return success();
}

// Splits in num_split parts along split_dim
LogicalResult ConvertTFLSplitOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_split_op = cast<TFL::SplitOp>(op);

  // Get the number of splits
  int32_t num_split = -1;
  auto numSplitAttr = tfl_split_op.num_splitsAttr();
  if (numSplitAttr) {
    num_split = numSplitAttr.getInt();
  } else {
    return failure();
  }

  // Get the axis
  ElementsAttr axisAttrElems;
  if (!matchPattern(tfl_split_op.split_dim(), m_Constant(&axisAttrElems))) {
    return op->emitOpError("Cannot read split_dim elems");
  }

  // The axis/split_dim parameter is stored as a 0D tensor instead of
  // an integer attribute in TFLite MLIR.
  int32_t axis = axisAttrElems.getValue<IntegerAttr>({}).getInt();

  llvm::Optional<SmallVector<Value>> results =
      convertSplitOp(rewriter, op, tfl_split_op.getResult(0),
                     tfl_split_op.value(), num_split, axis);

  if (!results) return failure();

  rewriter.replaceOp(op, results.getValue());

  return success();
}

// Splits in num_split parts along split_dim
LogicalResult ConvertTFLSplitVOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_splitv_op = cast<TFL::SplitVOp>(op);

  // Get the size_splits array
  SmallVector<int32_t> size_split;
  ElementsAttr size_split_elems;
  if (!matchPattern(tfl_splitv_op.size_splits(),
                    m_Constant(&size_split_elems))) {
    return failure();
  }

  for (int i = 0; i < size_split_elems.getNumElements(); i++) {
    size_split.push_back(size_split_elems.getValue<IntegerAttr>(i).getInt());
  }

  // Get the axis
  ElementsAttr axisAttrElems;
  if (!matchPattern(tfl_splitv_op.split_dim(), m_Constant(&axisAttrElems))) {
    return op->emitOpError("Cannot read split_dim elems");
  }

  // The axis/split_dim parameter is stored as a 0D tensor instead of
  // an integer attribute in TFLite MLIR.
  int32_t axis = axisAttrElems.getValue<IntegerAttr>(0).getInt();

  llvm::Optional<SmallVector<Value>> results =
      convertSplitVOp(rewriter, op, tfl_splitv_op.getResult(0),
                      tfl_splitv_op.value(), size_split, axis);

  if (!results) return failure();

  rewriter.replaceOp(op, results.getValue());

  return success();
}

LogicalResult ConvertTFLPadOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_pad_op = cast<TFL::PadOp>(op);

  ShapedType output_type =
      tfl_pad_op.getResult().getType().dyn_cast<ShapedType>();
  // Not a ranked tensor output
  if (!output_type) return failure();

  auto pad_op =
      CreateOpAndInfer<tosa::PadOp>(rewriter, op->getLoc(), output_type,
                                    tfl_pad_op.input(), tfl_pad_op.padding());

  rewriter.replaceOp(op, {pad_op.getResult()});
  return success();
}

LogicalResult ConvertTFLResizeBilinearOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_resize_op = cast<TFL::ResizeBilinearOp>(op);

  RankedTensorType output_type =
      tfl_resize_op.getResult().getType().dyn_cast<RankedTensorType>();
  // Not a ranked tensor output
  if (!output_type) return failure();

  llvm::Optional<Value> result = convertResizeOp(
      rewriter, op, output_type, tfl_resize_op.input(), StringRef("BILINEAR"),
      tfl_resize_op.align_cornersAttr().getValue(),
      tfl_resize_op.half_pixel_centersAttr().getValue());

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLResizeNearestNeighborOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_resize_op = cast<TFL::ResizeNearestNeighborOp>(op);

  RankedTensorType output_type =
      tfl_resize_op.getResult().getType().dyn_cast<RankedTensorType>();
  // Not a ranked tensor output
  if (!output_type) return failure();

  llvm::Optional<Value> result =
      convertResizeOp(rewriter, op, output_type, tfl_resize_op.input(),
                      StringRef("NEAREST_NEIGHBOR"),
                      tfl_resize_op.align_cornersAttr().getValue(),
                      tfl_resize_op.half_pixel_centersAttr().getValue());

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLSelectOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_sel_op = cast<TFL::SelectOp>(op);

  llvm::Optional<Value> result =
      convertSelectOp(rewriter, op, tfl_sel_op.getResult(),
                      tfl_sel_op.condition(), tfl_sel_op.x(), tfl_sel_op.y());
  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLSelectV2Op::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_sel_op = cast<TFL::SelectV2Op>(op);

  llvm::Optional<Value> result =
      convertSelectOp(rewriter, op, tfl_sel_op.getResult(),
                      tfl_sel_op.condition(), tfl_sel_op.x(), tfl_sel_op.y());
  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLSpaceToBatchNdOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_s2b_op = cast<TFL::SpaceToBatchNdOp>(op);
  llvm::Optional<Value> result = convertSpaceToBatchNDOp(
      rewriter, op, tfl_s2b_op.getResult(), tfl_s2b_op.input(),
      tfl_s2b_op.block_shape(), tfl_s2b_op.paddings());

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLBatchToSpaceNdOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_b2s_op = cast<TFL::BatchToSpaceNdOp>(op);

  llvm::Optional<Value> result = convertBatchToSpaceNDOp(
      rewriter, op, tfl_b2s_op.getResult(), tfl_b2s_op.input(),
      tfl_b2s_op.block_shape(), tfl_b2s_op.indices());

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLSpaceToDepthOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_s2d_op = cast<TFL::SpaceToDepthOp>(op);

  auto block_size_attr = tfl_s2d_op.block_sizeAttr();
  llvm::Optional<Value> result = convertSpaceToDepthOp(
      rewriter, op, tfl_s2d_op.getResult(), tfl_s2d_op.input(), block_size_attr,
      rewriter.getStringAttr("NHWC"));

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLDepthToSpaceOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_d2s_op = cast<TFL::DepthToSpaceOp>(op);

  auto block_size_attr = tfl_d2s_op.block_sizeAttr();
  llvm::Optional<Value> result = convertDepthToSpaceOp(
      rewriter, op, tfl_d2s_op.getResult(), tfl_d2s_op.input(), block_size_attr,
      rewriter.getStringAttr("NHWC"));

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLStridedSliceOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_ss_op = cast<TFL::StridedSliceOp>(op);

  llvm::Optional<Value> result = convertStridedSliceOp(
      rewriter, op, tfl_ss_op.getResult(), tfl_ss_op.input(), tfl_ss_op.begin(),
      tfl_ss_op.end(), tfl_ss_op.strides(), tfl_ss_op.begin_maskAttr().getInt(),
      tfl_ss_op.end_maskAttr().getInt(), tfl_ss_op.ellipsis_maskAttr().getInt(),
      tfl_ss_op.new_axis_maskAttr().getInt(),
      tfl_ss_op.shrink_axis_maskAttr().getInt());
  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLZerosLikeOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_zeroslike_op = cast<TFL::ZerosLikeOp>(op);

  llvm::Optional<Value> result = convertZerosLikeOp(
      rewriter, op, tfl_zeroslike_op.getResult(), tfl_zeroslike_op.input());

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLHardSwishOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_hardswish_op = cast<TFL::HardSwishOp>(op);
  RankedTensorType output_type =
      tfl_hardswish_op.getResult().getType().dyn_cast<RankedTensorType>();
  // Not a ranked tensor output
  if (!output_type) return failure();

  RankedTensorType input_type =
      tfl_hardswish_op.input().getType().dyn_cast<RankedTensorType>();
  // Not a ranked tensor output
  if (!input_type) return failure();

  // TFL hardswish: f(x) -> (x * relu6(x+3))/6

  if (input_type.getElementType().isa<mlir::quant::QuantizedType>() &&
      output_type.getElementType().isa<mlir::quant::QuantizedType>()) {
    // Should match TFLite reference numerical behavior
    mlir::quant::UniformQuantizedType input_qtype =
        input_type.getElementType()
            .dyn_cast_or_null<mlir::quant::UniformQuantizedType>();
    mlir::quant::UniformQuantizedType output_qtype =
        output_type.getElementType()
            .dyn_cast_or_null<mlir::quant::UniformQuantizedType>();

    auto hardswish_func = [](double v) -> double {
      double w = v + 3.0;
      w = w < 0.0 ? 0.0 : w > 6.0 ? 6.0 : w;
      return v * w / 6.0;
    };

    if (input_qtype.getStorageTypeIntegralWidth() == 8) {
      // Implement with 8-bit table lookup.
      Value table_const = getTosaConst8bitTable(
          rewriter, op, input_qtype.getScale(), input_qtype.getZeroPoint(),
          output_qtype.getScale(), output_qtype.getZeroPoint(), hardswish_func);

      CreateReplaceOpAndInfer<tosa::TableOp>(
          rewriter, op, output_type, tfl_hardswish_op.input(), table_const);
    }

  } else {
    // op1 = constop(3)
    // op2 = add(x, op1)
    // op3 = clamp(op2, 0, 6)
    // op4 = mul(x, op3)
    // op5 = reciprocal(6)
    // op6 = mul (op4, op5)

    Value op1_value = getTosaConstTensorSingleF32(rewriter, op, 3.0);

    auto op2_add_x_op1 =
        CreateOpAndInfer<tosa::AddOp>(rewriter, op->getLoc(), output_type,
                                      tfl_hardswish_op.input(), op1_value);

    auto op3_relu_op2_6 = CreateOpAndInfer<tosa::ClampOp>(
        rewriter, op->getLoc(), output_type, op2_add_x_op1.getResult(),
        rewriter.getI64IntegerAttr(0), rewriter.getI64IntegerAttr(0),
        rewriter.getF32FloatAttr(0.0f), rewriter.getF32FloatAttr(6.0f));

    auto op4_mul_x_op3 = CreateOpAndInfer<tosa::MulOp>(
        rewriter, op->getLoc(), output_type, tfl_hardswish_op.input(),
        op3_relu_op2_6.getResult(), 0);

    auto const_6 = getTosaConstTensorSingleF32(rewriter, op, 6.0);
    auto op5_reciprocal_6 = CreateOpAndInfer<tosa::ReciprocalOp>(
        rewriter, op->getLoc(), const_6.getType(), const_6);

    auto op6_mul_op4_op5 = CreateOpAndInfer<tosa::MulOp>(
        rewriter, op->getLoc(), output_type, op4_mul_x_op3.getResult(),
        op5_reciprocal_6.getResult(), 0);

    rewriter.replaceOp(op, {op6_mul_op4_op5.getResult()});
  }

  return success();
}

LogicalResult ConvertTFLLogisticOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_logistic_op = cast<TFL::LogisticOp>(op);

  ShapedType output_type =
      tfl_logistic_op.getResult().getType().dyn_cast<ShapedType>();
  RankedTensorType input_type =
      tfl_logistic_op.x().getType().dyn_cast<RankedTensorType>();
  if (!input_type || !output_type) return failure();

  bool input_is_qtype =
      input_type.getElementType().isa<mlir::quant::UniformQuantizedType>();
  bool output_is_qtype =
      output_type.getElementType().isa<mlir::quant::UniformQuantizedType>();

  if (input_is_qtype != output_is_qtype) {
    return op->emitOpError(
        "ConvertTFLLogisticOp: input/output tensor should "
        "be all quantized or all floating-point.");
  }

  if (input_is_qtype) {
    ShapedType int32_type = output_type.clone(rewriter.getIntegerType(32));
    mlir::quant::UniformQuantizedType input_qtype =
        input_type.getElementType()
            .dyn_cast_or_null<mlir::quant::UniformQuantizedType>();
    mlir::quant::UniformQuantizedType output_qtype =
        output_type.getElementType()
            .dyn_cast_or_null<mlir::quant::UniformQuantizedType>();

    auto sigmoid_func = [](double x) -> double {
      return 1.0 / (1.0 + std::exp(-x));
    };

    if (input_qtype.getStorageTypeIntegralWidth() == 8) {
      Value table_const = getTosaConst8bitTable(
          rewriter, op, input_qtype.getScale(), input_qtype.getZeroPoint(),
          output_qtype.getScale(), output_qtype.getZeroPoint(), sigmoid_func);

      CreateReplaceOpAndInfer<tosa::TableOp>(rewriter, op, output_type,
                                             tfl_logistic_op.x(), table_const);
    } else {  // int16
      if (input_qtype.getZeroPoint() != 0 || output_qtype.getZeroPoint() != 0) {
        op->emitOpError(
            "ConvertTFLLogistic: input/output zeropoint should be 0 in 16-bit "
            "mode");
        return failure();
      }
      double input_min = -32768 * input_qtype.getScale();
      double input_max = 32767 * input_qtype.getScale();

      // Generate table with gen_lut() in
      // tensorflow/lite/kernels/internal/common.h
      Value table_const = getTosaConst16bitTable(rewriter, op, sigmoid_func,
                                                 input_min, input_max);

      auto op1_table_in = CreateOpAndInfer<tosa::TableOp>(
          rewriter, op->getLoc(), int32_type, tfl_logistic_op.x(), table_const);

      Value op2_rescale_op1 =
          buildRescale(rewriter, op, output_type, op1_table_in.getResult(),
                       1.0 / 128.0, 0, 0, false, true);

      rewriter.replaceOp(op, {op2_rescale_op1});
    }
  } else {
    CreateReplaceOpAndInfer<tosa::SigmoidOp>(rewriter, op, output_type,
                                             tfl_logistic_op.x());
  }

  return success();
}

LogicalResult ConvertTFLTanhOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_tanh_op = cast<TFL::TanhOp>(op);
  ShapedType output_type =
      tfl_tanh_op.getResult().getType().dyn_cast<ShapedType>();
  RankedTensorType input_type =
      tfl_tanh_op.input().getType().dyn_cast<RankedTensorType>();
  if (!input_type || !output_type) return failure();

  bool input_is_qtype =
      input_type.getElementType().isa<mlir::quant::UniformQuantizedType>();
  bool output_is_qtype =
      output_type.getElementType().isa<mlir::quant::UniformQuantizedType>();

  if (input_is_qtype != output_is_qtype) {
    return op->emitOpError(
        "ConvertTFLTanhOp: input/output tensor should "
        "be all quantized or all floating-point.");
  }

  if (input_is_qtype) {
    ShapedType int32_type = output_type.clone(rewriter.getIntegerType(32));
    mlir::quant::UniformQuantizedType input_qtype =
        input_type.getElementType()
            .dyn_cast_or_null<mlir::quant::UniformQuantizedType>();
    mlir::quant::UniformQuantizedType output_qtype =
        output_type.getElementType()
            .dyn_cast_or_null<mlir::quant::UniformQuantizedType>();

    auto tanh_func = [](double x) -> double {
      x = std::exp(-2.0 * x);
      return (1.0 - x) / (1.0 + x);
    };

    if (input_qtype.getStorageTypeIntegralWidth() == 8) {
      Value table_const = getTosaConst8bitTable(
          rewriter, op, input_qtype.getScale(), input_qtype.getZeroPoint(),
          output_qtype.getScale(), output_qtype.getZeroPoint(), tanh_func);

      CreateReplaceOpAndInfer<tosa::TableOp>(rewriter, op, output_type,
                                             tfl_tanh_op.input(), table_const);
    } else {  // int16
      if (input_qtype.getZeroPoint() != 0 || output_qtype.getZeroPoint() != 0) {
        op->emitOpError(
            "ConvertTFLLogistic: input/output zeropoint should be 0 in 16-bit "
            "mode");
        return failure();
      }
      double input_min = -32768 * input_qtype.getScale();
      double input_max = 32767 * input_qtype.getScale();

      // Generate table with gen_lut() in
      // tensorflow/lite/kernels/internal/common.h
      Value table_const =
          getTosaConst16bitTable(rewriter, op, tanh_func, input_min, input_max);

      auto op1_table_in = CreateOpAndInfer<tosa::TableOp>(
          rewriter, op->getLoc(), int32_type, tfl_tanh_op.input(), table_const);

      Value op2_rescale_op1 =
          buildRescale(rewriter, op, output_type, op1_table_in.getResult(),
                       1.0 / 128.0, 0, 0, false, true);

      rewriter.replaceOp(op, {op2_rescale_op1});
    }

  } else {
    CreateReplaceOpAndInfer<tosa::TanhOp>(rewriter, op, output_type,
                                          tfl_tanh_op.input());
  }

  return success();
}

LogicalResult ConvertTFLPReluOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_prelu_op = cast<TFL::PReluOp>(op);
  RankedTensorType output_type =
      tfl_prelu_op.getResult().getType().dyn_cast<RankedTensorType>();
  if (!output_type) return failure();

  return failure();
}

LogicalResult ConvertTFLLeakyReluOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_leakyrelu_op = cast<TFL::LeakyReluOp>(op);
  RankedTensorType input_type =
      tfl_leakyrelu_op.input().getType().dyn_cast<RankedTensorType>();

  ShapedType output_type =
      tfl_leakyrelu_op.getResult().getType().dyn_cast<ShapedType>();

  if (!input_type || !output_type) return failure();

  bool output_is_qtype =
      output_type.getElementType().isa<mlir::quant::UniformQuantizedType>();

  // Implement LeakyRelu as element-wise:
  //   out = x > 0 ? x : alpha * x
  //
  // In TOSA ops:
  //
  //   const_zero = constant(0)
  //   op1 = mul(x, alpha)
  //   op2 = greater_equal(x, const_zero)
  //   out = select(a2, x, a1)
  //
  // If alpha can be constrained to 0.0 <= alpha <= 1.0, then
  // an alternative simpler lowering could be implemented with:
  //
  //   max(mul(x, alapha), x)
  //
  // But this alternative is not robust unless alpha meets those constraints.

  FloatAttr tmpAttr = tfl_leakyrelu_op.alphaAttr();
  // There is disagreement between the MLIR .td defaults and TF
  // documentation on 0.2 vs 0.3, but 0.2 will be used here.
  double alpha = 0.2;

  if (tmpAttr) {
    alpha = tmpAttr.getValueAsDouble();
  }

  if (output_is_qtype) {
    // op1 = rescale(input)
    // rescaled_alpha = (alpha << alpha_shift) // Remains within int32 range
    // op2 = mul(rescaled_input, rescaled_alpha, alpha_shift)
    // op3 = greater_equal(op1, 0)
    // op4 = select(op3, op1, op2)
    // out = rescale(op4)
    ShapedType rescale_type = output_type.clone(rewriter.getI32Type());

    UniformQuantizedType input_qtype =
        input_type.getElementType().cast<UniformQuantizedType>();

    UniformQuantizedType output_qtype =
        output_type.getElementType().cast<UniformQuantizedType>();

    double scale_alpha =
        input_qtype.getScale() * alpha / output_qtype.getScale();
    double scale_identity = input_qtype.getScale() / output_qtype.getScale();

    Value op1_rescale_in =
        buildRescaleToInt32(rewriter, op, tfl_leakyrelu_op.input(), 1.0,
                            input_qtype.getZeroPoint());

    Value const_zero = getTosaConstTensorSingleI32(rewriter, op, 0);
    auto op2_ge = CreateOpAndInfer<tosa::GreaterEqualOp>(
        rewriter, op->getLoc(), rescale_type.clone(rewriter.getI1Type()),
        op1_rescale_in, const_zero);

    Value op3_rescale_alpha_in = buildRescale(
        rewriter, op, output_type, tfl_leakyrelu_op.input(), scale_alpha,
        input_qtype.getZeroPoint(), output_qtype.getZeroPoint(), true, true);

    Value op4_rescale_identity_in = buildRescale(
        rewriter, op, output_type, tfl_leakyrelu_op.input(), scale_identity,
        input_qtype.getZeroPoint(), output_qtype.getZeroPoint(), true, true);

    CreateReplaceOpAndInfer<tosa::SelectOp>(rewriter, op, output_type, op2_ge,
                                            op4_rescale_identity_in,
                                            op3_rescale_alpha_in);

    return success();

  } else {
    Value const_zero = getTosaConstTensorSingleF32(rewriter, op, 0.0);

    auto op1_mul = CreateOpAndInfer<tosa::MulOp>(
        rewriter, op->getLoc(), output_type, tfl_leakyrelu_op.input(),
        getTosaConstTensorSingleF32(rewriter, op, alpha), 0);

    auto op2_ge = CreateOpAndInfer<tosa::GreaterEqualOp>(
        rewriter, op->getLoc(), output_type.clone(rewriter.getIntegerType(1)),
        tfl_leakyrelu_op.input(), const_zero);

    CreateReplaceOpAndInfer<tosa::SelectOp>(rewriter, op, output_type, op2_ge,
                                            tfl_leakyrelu_op.input(),
                                            op1_mul.getResult());

    return success();
  }
}

LogicalResult ConvertTFLNegOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_neg_op = cast<TFL::NegOp>(op);
  ShapedType output_type =
      tfl_neg_op.getResult().getType().dyn_cast<ShapedType>();
  if (!output_type) return failure();

  CreateReplaceOpAndInfer<tosa::NegateOp>(rewriter, op, output_type,
                                          tfl_neg_op.x());

  return success();
}

LogicalResult ConvertTFLYieldOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  rewriter.replaceOpWithNewOp<tosa::YieldOp>(op, op->getResultTypes(),
                                             op->getOperands());

  return success();
}

LogicalResult ConvertTFLCustomOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_custom_op = cast<TFL::CustomOp>(op);
  rewriter.replaceOpWithNewOp<tosa::CustomOp>(
      op, op->getResultTypes(), tfl_custom_op.custom_code(), op->getOperands());

  return success();
}

LogicalResult ConvertTFLReverseV2Op::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_reverse_op = cast<TFL::ReverseV2Op>(op);

  RankedTensorType input_type =
      tfl_reverse_op.input().getType().dyn_cast<RankedTensorType>();
  RankedTensorType output_type =
      tfl_reverse_op.getResult().getType().dyn_cast<RankedTensorType>();
  if (!input_type || !output_type) return failure();

  ElementsAttr axis_elems;
  if (!matchPattern(tfl_reverse_op.axis(), m_Constant(&axis_elems)))
    return failure();

  auto input_rank = input_type.getShape().size();
  Value val = tfl_reverse_op.input();
  if (axis_elems.getNumElements() == 0) {
    auto identity_op = CreateOpAndInfer<tosa::IdentityOp>(
        rewriter, op->getLoc(), output_type, val);
    val = identity_op.getResult();
  } else {
    for (int i = 0; i < axis_elems.getNumElements(); i++) {
      int64_t axis_val = axis_elems.getValue<IntegerAttr>(i).getInt();
      if (axis_val < 0) axis_val += input_rank;
      auto axis_attr = rewriter.getI64IntegerAttr(axis_val);
      auto reverse_op = CreateOpAndInfer<tosa::ReverseOp>(
          rewriter, op->getLoc(), output_type, val, axis_attr);

      val = reverse_op.getResult();
    }
  }

  rewriter.replaceOp(op, {val});

  return success();
}

LogicalResult ConvertTFLQuantizeOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_quantize_op = cast<TFL::QuantizeOp>(op);

  RankedTensorType input_type =
      tfl_quantize_op.input().getType().dyn_cast<RankedTensorType>();
  ShapedType output_type =
      tfl_quantize_op.getResult().getType().dyn_cast<ShapedType>();
  if (!input_type || !output_type) return failure();

  ShapedType qtype =
      tfl_quantize_op.getResult().getType().dyn_cast<ShapedType>();
  if (!qtype) return failure();

  UniformQuantizedType element_type =
      qtype.getElementType().dyn_cast<UniformQuantizedType>();
  if (!element_type) return failure();

  UniformQuantizedType input_element_type =
      input_type.getElementType().dyn_cast<UniformQuantizedType>();

  // If input is already a quantized type, this is basically a RESCALE (or
  // tensorflow::ops::Requantize)
  if (input_element_type) {
    double rescale_scale =
        input_element_type.getScale() / element_type.getScale();
    Value rescale_op =
        buildRescale(rewriter, op, output_type, tfl_quantize_op.input(),
                     rescale_scale, input_element_type.getZeroPoint(),
                     element_type.getZeroPoint(), true, true);

    rewriter.replaceOp(op, {rescale_op});
    return success();
  } else {
    double scale = 1 / element_type.getScale();
    int64_t zp = element_type.getZeroPoint();
    int64_t num_bits = element_type.getStorageTypeIntegralWidth();
    zp = element_type.isSigned() ? zp : zp - (1 << (num_bits - 1));

    llvm::Optional<Value> result = convertQuantizeOp(
        rewriter, op, output_type, tfl_quantize_op.input(), scale, zp);

    if (!result) return failure();

    rewriter.replaceOp(op, {result.getValue()});

    return success();
  }
}

LogicalResult ConvertTFLDequantizeOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_dequantize_op = cast<TFL::DequantizeOp>(op);

  ShapedType output_type =
      tfl_dequantize_op.getResult().getType().dyn_cast<ShapedType>();
  // Not a ranked tensor output
  if (!output_type) return failure();

  RankedTensorType qtype =
      tfl_dequantize_op.input().getType().dyn_cast<RankedTensorType>();
  if (!qtype) return failure();

  Type element_type = qtype.getElementType();
  if (element_type.isa<FloatType>()) {
    CreateReplaceOpAndInfer<tosa::CastOp>(rewriter, op, output_type,
                                          tfl_dequantize_op.input());
    return success();
  }

  if (auto eq_ty = element_type.dyn_cast<quant::UniformQuantizedType>()) {
    double scale = eq_ty.getScale();
    int64_t zp = eq_ty.getZeroPoint();
    int64_t num_bits = eq_ty.getStorageTypeIntegralWidth();
    zp = eq_ty.isSigned() ? zp : zp - (1 << (num_bits - 1));

    llvm::Optional<Value> result = convertDequantizeOp(
        rewriter, op, output_type, tfl_dequantize_op.input(), scale, zp, 0);

    if (!result) return failure();

    rewriter.replaceOp(op, {result.getValue()});
    return success();
  }

  if (quant::UniformQuantizedPerAxisType eq_ty =
          element_type.dyn_cast<quant::UniformQuantizedPerAxisType>()) {
    SmallVector<float> zps;
    for (auto zp : eq_ty.getZeroPoints()) {
      int64_t num_bits = eq_ty.getStorageTypeIntegralWidth();
      zps.push_back(eq_ty.isSigned() ? zp : zp - (1 << (num_bits - 1)));
    }

    SmallVector<float> scales;
    for (auto scale : eq_ty.getScales()) {
      scales.push_back(scale);
    }

    llvm::Optional<Value> result = convertDequantizeOp(
        rewriter, op, output_type, tfl_dequantize_op.input(), scales, zps,
        eq_ty.getQuantizedDimension());

    if (!result) return failure();

    rewriter.replaceOp(op, {result.getValue()});
    return success();
  }

  return failure();
}

LogicalResult ConvertTFLConstOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_const_op = cast<TFL::ConstOp>(op);

  ShapedType output_type =
      tfl_const_op.getResult().getType().dyn_cast<ShapedType>();
  if (!output_type) return failure();

  ElementsAttr elements = tfl_const_op.value();
  Type element_type = elements.getType().getElementType();
  if (output_type.getElementType().isa<quant::QuantizedType>()) {
    output_type = RankedTensorType::get(output_type.getShape(), element_type);
  }

  // If the output shape is unranked we can extract the result shape from the
  // attribute shape. This occurs as some TFLite folders create constants with
  // unranked shapes.
  if (!output_type.hasRank()) {
    output_type = elements.getType().cast<ShapedType>().clone(element_type);
  }

  rewriter.replaceOpWithNewOp<tosa::ConstOp>(op, output_type, elements);

  return success();
}

LogicalResult ConvertTFLQConstOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_qconst_op = cast<TFL::QConstOp>(op);

  ShapedType output_type =
      tfl_qconst_op.getResult().getType().dyn_cast<ShapedType>();
  if (!output_type) return failure();

  ElementsAttr elements = tfl_qconst_op.value();

  // If the output shape is unranked we can extract the result shape from the
  // attribute shape. This occurs as some TFLite folders create constants with
  // unranked shapes.
  if (!output_type.hasRank()) {
    output_type = elements.getType().cast<ShapedType>().clone(
        output_type.getElementType());
  }

  rewriter.replaceOpWithNewOp<tosa::ConstOp>(op, output_type, elements);

  return success();
}

LogicalResult ConvertConstantOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_const_op = cast<ConstantOp>(op);

  ShapedType output_type =
      tfl_const_op.getResult().getType().dyn_cast<ShapedType>();
  // Not a ranked tensor output
  if (!output_type) return failure();

  ElementsAttr attr = tfl_const_op.valueAttr().dyn_cast<ElementsAttr>();

  auto e_type = output_type.getElementType();
  // TOSA only support up to 48-bits
  // If source is higher than that, it's not representabble.
  // For data type like 64 bits, we need to truncate them into 48 bits.
  if (e_type.isInteger(64)) {
    e_type = rewriter.getIntegerType(48);
    attr = attr.cast<DenseIntOrFPElementsAttr>().mapValues(
        e_type, [](const APInt& x) -> APInt { return x.trunc(48); });
  }

  if (!output_type.hasRank()) {
    if (auto attr_type = attr.getType().dyn_cast<ShapedType>()) {
      output_type = attr_type.clone(e_type);
    }
  }

  output_type = output_type.clone(e_type);
  rewriter.replaceOpWithNewOp<tosa::ConstOp>(op, output_type, attr);

  return success();
}

LogicalResult ConvertTFLGatherOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_gather_op = cast<TFL::GatherOp>(op);

  int32_t axis = tfl_gather_op.axisAttr().getInt();
  int32_t batch_dims = 0;  // Not a parameter in tfl.Gather; default to 0.

  llvm::Optional<Value> result = convertGatherOp(
      rewriter, op, tfl_gather_op.getResult(), tfl_gather_op.params(),
      tfl_gather_op.indices(), batch_dims, axis);

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLGatherNdOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_gathernd_op = cast<TFL::GatherNdOp>(op);

  llvm::Optional<Value> result =
      convertGatherNdOp(rewriter, op, tfl_gathernd_op.getResult(),
                        tfl_gathernd_op.params(), tfl_gathernd_op.indices());

  if (!result) return failure();
  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLOneHotOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto tfl_one_hot_op = cast<TFL::OneHotOp>(op);

  ElementsAttr depth_elems;
  if (!matchPattern(tfl_one_hot_op.depth(), m_Constant(&depth_elems)))
    return failure();
  int32_t depth = depth_elems.getValue<IntegerAttr>({}).getInt();

  IntegerAttr axisAttr = tfl_one_hot_op.axisAttr();
  int32_t axis = axisAttr.getInt();

  llvm::Optional<Value> result = convertOneHotOp(
      rewriter, op, tfl_one_hot_op.getResult(), tfl_one_hot_op.indices(),
      tfl_one_hot_op.on_value(), tfl_one_hot_op.off_value(), depth, axis);

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

LogicalResult ConvertTFLArgMaxOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto arg_max_op = cast<TFL::ArgMaxOp>(op);

  ElementsAttr dim_elems;
  if (!matchPattern(arg_max_op.dim(), m_Constant(&dim_elems))) return failure();

  int32_t dim = dim_elems.getValue<IntegerAttr>({}).getInt();
  CreateReplaceOpAndInfer<tosa::ArgMaxOp>(
      rewriter, op, arg_max_op.getType(), arg_max_op.input(),
      rewriter.getIntegerAttr(rewriter.getI64Type(), dim));

  return success();
}

LogicalResult ConvertTFLFakeQuantOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  auto fakequant_op = cast<TFL::FakeQuantOp>(op);

  ShapedType output_type =
      fakequant_op.getResult().getType().dyn_cast<ShapedType>();
  // Not a ranked tensor output
  if (!output_type) return failure();

  llvm::Optional<Value> result =
      convertFakeQuantOp(rewriter, op, output_type, fakequant_op.input(),
                         fakequant_op.minAttr().getValueAsDouble(),
                         fakequant_op.maxAttr().getValueAsDouble(),
                         fakequant_op.num_bitsAttr().getInt(),
                         fakequant_op.narrow_rangeAttr().getValue());

  if (!result) return failure();

  rewriter.replaceOp(op, {result.getValue()});

  return success();
}

void LegalizeTFL::runOnFunction() {
  ConversionTarget target(getContext());

  target.addIllegalDialect<TFL::TensorFlowLiteDialect>();
  target.addIllegalOp<quant::StatisticsOp>();
  // Operations are legal if they don't contain any illegal type.
  target.markUnknownOpDynamicallyLegal([](Operation* op) {
    if (auto constantOp = dyn_cast<ConstantOp>(op)) {
      return constantOp.getType().isa<NoneType>();
    }
    return true;
  });

  auto* ctx = &getContext();
  auto func = getFunction();

  RewritePatternSet patterns(&getContext());

  // Add the generated patterns to the list.
  populateWithGenerated(patterns);

#define DEF_PATTERN_INSERT(PAT) patterns.insert<Convert##PAT##Op>(ctx);

  DEF_PATTERN_INSERT(TFLRelu);
  DEF_PATTERN_INSERT(TFLRelu6);
  DEF_PATTERN_INSERT(TFLEqual);
  DEF_PATTERN_INSERT(TFLNotEqual);
  DEF_PATTERN_INSERT(TFLGreater);
  DEF_PATTERN_INSERT(TFLGreaterEqual);
  DEF_PATTERN_INSERT(TFLAdd);
  DEF_PATTERN_INSERT(TFLSub);
  DEF_PATTERN_INSERT(TFLMul);
  DEF_PATTERN_INSERT(TFLSquare);
  DEF_PATTERN_INSERT(TFLSquaredDifference);
  DEF_PATTERN_INSERT(TFLDiv);
  DEF_PATTERN_INSERT(TFLMaximum);
  DEF_PATTERN_INSERT(TFLMinimum);
  DEF_PATTERN_INSERT(TFLFloorMod);
  DEF_PATTERN_INSERT(TFLFloorDiv);
  DEF_PATTERN_INSERT(TFLAddN);
  DEF_PATTERN_INSERT(TFLAveragePool2D);
  DEF_PATTERN_INSERT(TFLMaxPool2D);
  DEF_PATTERN_INSERT(TFLConcatenation);
  DEF_PATTERN_INSERT(TFLReshape);
  DEF_PATTERN_INSERT(TFLRank);
  DEF_PATTERN_INSERT(TFLShape);
  DEF_PATTERN_INSERT(TFLExpandDims);
  DEF_PATTERN_INSERT(TFLSqueeze);
  DEF_PATTERN_INSERT(TFLFill);
  DEF_PATTERN_INSERT(TFLElu);
  DEF_PATTERN_INSERT(TFLSoftmax);
  DEF_PATTERN_INSERT(TFLLogSoftmax);
  DEF_PATTERN_INSERT(TFLSqrt);
  DEF_PATTERN_INSERT(TFLReduceAny);
  DEF_PATTERN_INSERT(TFLReduceMax);
  DEF_PATTERN_INSERT(TFLReduceMin);
  DEF_PATTERN_INSERT(TFLMean);
  DEF_PATTERN_INSERT(TFLReduceProd);
  DEF_PATTERN_INSERT(TFLSum);
  DEF_PATTERN_INSERT(TFLConv2D);
  DEF_PATTERN_INSERT(TFLTransposeConv);
  DEF_PATTERN_INSERT(TFLDepthwiseConv2D);
  DEF_PATTERN_INSERT(TFLFullyConnected);
  DEF_PATTERN_INSERT(TFLSplit);
  DEF_PATTERN_INSERT(TFLSplitV);
  DEF_PATTERN_INSERT(TFLPack);
  DEF_PATTERN_INSERT(TFLUnpack);
  DEF_PATTERN_INSERT(TFLTranspose);
  DEF_PATTERN_INSERT(TFLTile);
  DEF_PATTERN_INSERT(TFLSlice);
  DEF_PATTERN_INSERT(TFLStridedSlice);
  DEF_PATTERN_INSERT(TFLZerosLike);
  DEF_PATTERN_INSERT(TFLHardSwish);
  DEF_PATTERN_INSERT(TFLLess);
  DEF_PATTERN_INSERT(TFLLessEqual);
  DEF_PATTERN_INSERT(TFLPad);
  DEF_PATTERN_INSERT(TFLResizeBilinear);
  DEF_PATTERN_INSERT(TFLResizeNearestNeighbor);
  DEF_PATTERN_INSERT(TFLSelect);
  DEF_PATTERN_INSERT(TFLSelectV2);
  DEF_PATTERN_INSERT(TFLSpaceToBatchNd);
  DEF_PATTERN_INSERT(TFLBatchToSpaceNd);
  DEF_PATTERN_INSERT(TFLSpaceToDepth);
  DEF_PATTERN_INSERT(TFLDepthToSpace);
  DEF_PATTERN_INSERT(TFLLogistic);
  DEF_PATTERN_INSERT(TFLTanh);
  DEF_PATTERN_INSERT(TFLPRelu);
  DEF_PATTERN_INSERT(TFLLeakyRelu);
  DEF_PATTERN_INSERT(TFLNeg);
  DEF_PATTERN_INSERT(TFLYield);
  DEF_PATTERN_INSERT(TFLCustom);
  DEF_PATTERN_INSERT(TFLReverseV2);
  DEF_PATTERN_INSERT(TFLQuantize);
  DEF_PATTERN_INSERT(TFLDequantize);
  DEF_PATTERN_INSERT(TFLConst);
  DEF_PATTERN_INSERT(TFLQConst);
  DEF_PATTERN_INSERT(Constant);
  DEF_PATTERN_INSERT(TFLGather);
  DEF_PATTERN_INSERT(TFLGatherNd);
  DEF_PATTERN_INSERT(TFLArgMax);
  DEF_PATTERN_INSERT(TFLFakeQuant);
  DEF_PATTERN_INSERT(TFLOneHot);

  GreedyRewriteConfig config;
  config.useTopDownTraversal = true;
  if (failed(applyPatternsAndFoldGreedily(func, std::move(patterns), config))) {
    signalPassFailure();
  }

  func.walk([&](tosa::ConstOp op) {
    auto ety = op.value().getType().getElementType();
    auto new_ty = op.getType().cast<ShapedType>().clone(ety);
    op.getResult().setType(new_ty);
  });

  // Insert UnrealizedConversionCasts to guarantee ReturnOp agress with
  // the FuncOp type.
  IRRewriter rewriter(func.getContext());
  func.walk([&](ReturnOp op) {
    FuncOp parent = dyn_cast<FuncOp>(op->getParentOp());
    if (!parent) return;

    rewriter.setInsertionPoint(op);
    FunctionType funcTy = func.getType();
    auto resultTys = funcTy.getResults();

    bool castAdded = false;
    SmallVector<Value> castedValues;
    for (auto it : llvm::zip(op->getOperands(), resultTys)) {
      Value operand = std::get<0>(it);
      Type currentTy = operand.getType();
      Type castTy = std::get<1>(it);
      if (currentTy == castTy) {
        castedValues.push_back(operand);
        continue;
      }

      castedValues.push_back(
          rewriter.create<tensor::CastOp>(op.getLoc(), castTy, operand)
              .getResult());

      castAdded = true;
    }

    if (castAdded) {
      rewriter.replaceOpWithNewOp<ReturnOp>(op, castedValues);
    }
  });
}
}  // namespace

// Creates an instance of the TensorFlow Lite dialect LegalizeTFL pass.
std::unique_ptr<OperationPass<FuncOp>> createLegalizeTFLPass() {
  return std::make_unique<LegalizeTFL>();
}

}  // namespace tosa
}  // namespace mlir
