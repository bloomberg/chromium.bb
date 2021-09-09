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

#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <string>
#include <tuple>
#include <type_traits>

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Sequence.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/FormatVariadic.h"
#include "mlir/Dialect/StandardOps/IR/Ops.h"  // from @llvm-project
#include "mlir/Dialect/Traits.h"  // from @llvm-project
#include "mlir/IR/Attributes.h"  // from @llvm-project
#include "mlir/IR/Builders.h"  // from @llvm-project
#include "mlir/IR/Diagnostics.h"  // from @llvm-project
#include "mlir/IR/DialectImplementation.h"  // from @llvm-project
#include "mlir/IR/Function.h"  // from @llvm-project
#include "mlir/IR/Identifier.h"  // from @llvm-project
#include "mlir/IR/Location.h"  // from @llvm-project
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "mlir/IR/Matchers.h"  // from @llvm-project
#include "mlir/IR/OpDefinition.h"  // from @llvm-project
#include "mlir/IR/OpImplementation.h"  // from @llvm-project
#include "mlir/IR/PatternMatch.h"  // from @llvm-project
#include "mlir/IR/StandardTypes.h"  // from @llvm-project
#include "mlir/IR/TypeUtilities.h"  // from @llvm-project
#include "mlir/IR/Types.h"  // from @llvm-project
#include "mlir/IR/Value.h"  // from @llvm-project
#include "mlir/Parser.h"  // from @llvm-project
#include "mlir/Support/LLVM.h"  // from @llvm-project
#include "mlir/Support/LogicalResult.h"  // from @llvm-project
#include "mlir/Transforms/InliningUtils.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_attributes.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_side_effects.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_structs.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_types.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/util/tensor_format.h"

namespace mlir {
namespace TF {

// Propagates underscore and device attributes from src to dst.
// TODO(b/158769932): This should be a general feature instead post some policy
// discussion.
static void PropagateAttributes(Operation *src, Operation *dst) {
  auto device = mlir::Identifier::get("device", src->getContext());
  for (auto named_attr : src->getAttrs()) {
    if (*named_attr.first.begin() == '_' || named_attr.first == device)
      dst->setAttr(named_attr.first, named_attr.second);
  }
}

//===----------------------------------------------------------------------===//
// TF op helper functions
//===----------------------------------------------------------------------===//

// Returns the RankedTensorType for the given operand. TensorFlow constant ops
// may have non-static shape because the shape is not propagated during constant
// folding. If the defining op for the given operand is a constant op, this
// routine uses the constant op's attribute to get the actual shape.
static RankedTensorType GetRankedTensorTypeForOperand(Value operand) {
  DenseElementsAttr attr;
  if (matchPattern(operand, m_Constant(&attr))) {
    return attr.getType().dyn_cast<RankedTensorType>();
  }
  return operand.getType().dyn_cast<RankedTensorType>();
}

// Returns true if the given `value` is of ranked float tensor type with the
// given `rank`.
static inline bool IsOfRankedFloatTensorType(RankedTensorType type, int rank) {
  return type && type.getRank() == rank &&
         type.getElementType().isa<FloatType>();
}

// Returns true if the given `value` has the specified rank or has unranked
// type.
static inline bool IsOfRankOrUnranked(Value value, int64_t rank) {
  RankedTensorType type = GetRankedTensorTypeForOperand(value);
  return !type || type.getRank() == rank;
}

// Returns true if the given `value` has at least the specified rank or has
// unranked type.
static inline bool HasRankAtLeast(Value value, int64_t rank) {
  RankedTensorType type = GetRankedTensorTypeForOperand(value);
  return !type || type.getRank() >= rank;
}

// Returns true if the given `value` has at most the specified rank or has
// unranked type.
static inline bool HasRankAtMost(Value value, int64_t rank) {
  RankedTensorType type = GetRankedTensorTypeForOperand(value);
  return !type || type.getRank() <= rank;
}

static bool IsUnknownDimOrRank(int64_t dim_or_rank) {
  return dim_or_rank == -1;
}

// Returns the tf.Equal/tf.NotEqual result type given `x` and `y` and inputs. If
// `incompatible_shape_error` is true, reports error if `x` and `y` has
// incompatible shapes. Otherwise, returns a tensor type with unknown rank.
static Type DeduceEqualCmpOpType(Builder *builder, Location loc, Value x,
                                 Value y, BoolAttr incompatible_shape_error) {
  auto result_type =
      OpTrait::util::getBroadcastedType(x.getType(), y.getType());
  if (!result_type) {
    if (incompatible_shape_error.getValue()) {
      mlir::emitError(loc, "non-broadcastable operands");
    } else {
      return UnrankedTensorType::get(builder->getI1Type());
    }
  }

  auto ranked_type = result_type.dyn_cast<RankedTensorType>();
  if (!ranked_type) return UnrankedTensorType::get(builder->getI1Type());

  return RankedTensorType::get(ranked_type.getShape(), builder->getI1Type());
}

// Returns dimension index for the given TensorFlow axis that supports negative
// indexing.
static int64_t GetDimForAxis(int64_t axis, int64_t rank) {
  return axis >= 0 ? axis : axis + rank;
}

// Infers output type for reduction ops such as SumOp, MaxOp etc.
// TODO(b/e667204a): Move this logic to shape inference once it supports custom
// inference functions.
static Type InferReductionOpType(Value input, Value reduction_indices,
                                 BoolAttr keep_dims, Builder *builder) {
  Type input_ty = input.getType();
  Type element_ty = getElementTypeOrSelf(input_ty);

  // Output type is unranked if input type is not ranked.
  auto ranked_ty = input_ty.dyn_cast<RankedTensorType>();
  if (!ranked_ty) return UnrankedTensorType::get(element_ty);
  int64_t rank = ranked_ty.getRank();

  DenseIntElementsAttr indices;
  if (!matchPattern(reduction_indices, m_Constant(&indices))) {
    // Output type is unranked if reduction indices are not constant and reduced
    // dimensions are not kept.
    if (!keep_dims.getValue()) return UnrankedTensorType::get(element_ty);

    // Otherwise, output type has same rank as the input.
    return RankedTensorType::get(SmallVector<int64_t, 4>(rank, -1), element_ty);
  }

  int64_t num_reduce_dim = 0;
  llvm::SmallVector<bool, 4> is_reduce_dim(rank, false);
  for (const APInt &index : indices.getValues<APInt>()) {
    int64_t dim = GetDimForAxis(index.getSExtValue(), rank);
    // Invalid input.
    if (dim < 0 || dim >= rank) return UnrankedTensorType::get(element_ty);

    if (!is_reduce_dim[dim]) {
      is_reduce_dim[dim] = true;
      num_reduce_dim++;
    }
  }

  ArrayRef<int64_t> shape = ranked_ty.getShape();
  SmallVector<int64_t, 4> out_shape;
  out_shape.reserve(rank - (keep_dims.getValue() ? 0 : num_reduce_dim));
  for (int64_t i = 0; i < rank; ++i) {
    if (!is_reduce_dim[i])
      out_shape.push_back(shape[i]);
    else if (keep_dims.getValue())
      out_shape.push_back(1);
  }
  return RankedTensorType::get(out_shape, element_ty);
}

// Verifies that the given types are cast compatible. If not, emits appropriate
// error for the given op. If mask_one_dim is set to true, then the types are
// allowed to have one mismatching dimension. Masking one of the dimensions is
// useful for ops like Concat that requires all ranked inputs to have the same
// rank and match dimension sizes for all but one of the dimensions.
static LogicalResult VerifyTypesCompatibility(
    Operation::operand_type_range types, bool mask_one_dim, Operation *op) {
  constexpr int64_t kUninitialized = -1;
  int64_t common_rank = kUninitialized;
  llvm::SmallVector<int64_t, 4> common_dims;
  int64_t dim_to_mask = kUninitialized;

  // Initialize common_rank with rank of the first ranked type and verify that
  // following ranked types have the same rank.
  // Similarly, initialize each of the dimensions with the first type that has
  // the dimension size available and verify that all following types have the
  // same size for the dimension. However, if mask_one_dim is true, note down
  // the dimension index on the first mismatch and ignore dimension at that
  // index in following types.
  for (Type ty : types) {
    RankedTensorType ranked_ty = ty.dyn_cast<RankedTensorType>();
    if (!ranked_ty) continue;

    int64_t rank = ranked_ty.getRank();
    if (common_rank == kUninitialized) {
      common_rank = rank;
      common_dims.resize(common_rank, kUninitialized);
    } else if (common_rank != rank) {
      return op->emitError()
             << "operand type " << ranked_ty
             << " is not compatible with preceding operands; expected rank: "
             << common_rank;
    }

    for (int64_t i = 0, e = common_rank; i != e; i++) {
      if (i == dim_to_mask) continue;

      int64_t dim = ranked_ty.getDimSize(i);
      if (dim == kUninitialized) continue;

      int64_t &common_dim = common_dims[i];
      if (common_dim == kUninitialized) {
        common_dim = dim;
      } else if (common_dim != dim) {
        // If mask_one_dim is true, do not emit an error if this is the only
        // dimension with mismatches. Note down the dimension to mask it from
        // the following types.
        if (mask_one_dim && dim_to_mask == kUninitialized) {
          dim_to_mask = i;
          continue;
        }

        return op->emitError() << "operand type " << ranked_ty
                               << " is not compatible with preceding operands; "
                                  "expected dimension at index "
                               << i << ": " << common_dim;
      }
    }
  }
  return success();
}

// This is a helper for the Select to SelectV2 canonicalization. The `data` rank
// refers to the rank of `t`/`e` (these two inputs have equal rank; this is
// checked in the verifier).
//
// In most cases, the predicate for Select can be used directly as the predicate
// for SelectV2. However, there is one case that varies, which is when the
// predicate is a tensor and the data is multidimensional. In this case, Select
// op semantics dictate that the predicate tensor length must match the size of
// the first data dimension. This varies from normal broadcasting semantics
// (which are used in SelectV2), so we must reshape the tensor in this case to
// be compatible.
static Value ReshapeSelectPredIfNecessary(OpBuilder *builder, Location loc,
                                          Value cond, int data_rank) {
  auto cond_tensor = cond.getType().cast<RankedTensorType>();
  // Reshape is only needed in the case that the cond rank is 1 (i.e. it is
  // a vector) AND t/e rank is > 1.
  if (cond_tensor.getRank() != 1 || data_rank <= 1) {
    // No reshape necessary. Leave cond as it is.
    return cond;
  }

  // This is the case where a reshape is needed. We want to construct the
  // shape [x,1,...1], where x is the value in the pred tensor and the
  // length of the shape is equal to data_rank.
  SmallVector<int64_t, 8> shape(data_rank, 1);
  shape[0] = cond_tensor.getShape().front();
  auto new_shape_type =
      RankedTensorType::get({data_rank}, builder->getIntegerType(64));
  auto shape_attr = DenseIntElementsAttr::get(new_shape_type, shape);
  auto new_shape = builder->create<ConstOp>(loc, shape_attr);
  return builder->create<ReshapeOp>(loc, cond, new_shape);
}

//===----------------------------------------------------------------------===//
// Helper functions detect device capabilities from RuntimeDevices.
//===----------------------------------------------------------------------===//

namespace {
using DeviceNameUtils = ::tensorflow::DeviceNameUtils;
using ParsedName = ::tensorflow::DeviceNameUtils::ParsedName;

bool IsGpuDevice(const DeviceNameUtils::ParsedName &device) {
  return device.type == ::tensorflow::DEVICE_GPU;
}

}  // namespace

// Returns true if at least one GPU device is available at runtime.
bool CanUseGpuDevice(const RuntimeDevices &devices) {
  return llvm::any_of(devices.device_names(), IsGpuDevice);
}

// Returns true if all of the GPUs available at runtime support TensorCores
// (NVIDIA compute capability >= 7.0).
bool CanUseTensorCores(const RuntimeDevices &devices) {
  auto has_tensor_cores = [&](const DeviceNameUtils::ParsedName &device) {
    auto md = devices.GetGpuDeviceMetadata(device);
    return md ? md->cc_major().getInt() >= 7 : false;
  };
  return llvm::all_of(
      llvm::make_filter_range(devices.device_names(), IsGpuDevice),
      has_tensor_cores);
}

// Returns true if operation does not have explicit device placement that would
// prevent it from running on GPU device.
bool CanUseGpuDevice(Operation *op) {
  auto device_attr = op->getAttrOfType<StringAttr>("device");
  if (!device_attr || device_attr.getValue().empty()) return true;

  DeviceNameUtils::ParsedName device;
  if (!DeviceNameUtils::ParseFullName(device_attr.getValue().str(), &device))
    return false;

  // We can't use GPU if operation explicitly placed on non-GPU device.
  return !device.has_type || device.type == ::tensorflow::DEVICE_GPU;
}

//===----------------------------------------------------------------------===//
// TF op helper functions to work with layout transformation.
//===----------------------------------------------------------------------===//

SmallVector<int64_t, 4> ReversePermutation(ArrayRef<int64_t> permutation) {
  SmallVector<int64_t, 4> reverse(permutation.size());
  for (size_t i = 0; i < permutation.size(); ++i) {
    reverse[permutation[i]] = i;
  }
  return reverse;
}

SmallVector<int64_t, 4> GetDataFormatPermutation(StringRef from, StringRef to) {
  if (from == "NHWC" && to == "NCHW") {
    return {0, 3, 1, 2};
  } else if (from == "NCHW" && to == "NHWC") {
    return {0, 2, 3, 1};
  } else {
    return {};
  }
}

// Shuffle elements in the `attr` according to the permutation. Optional
// `inner_size` allows to shuffle array attributes created from rank 2 tensors
// on outer dimension only.
ArrayAttr ShuffleArrayAttr(ArrayAttr attr, ArrayRef<int64_t> permutation,
                           int inner_size = 1) {
  if (attr.size() == 0) return attr;

  assert(attr.size() % inner_size == 0);
  assert(attr.size() / inner_size == permutation.size());

  SmallVector<Attribute, 8> values{attr.begin(), attr.end()};
  SmallVector<Attribute, 8> shuffled(values.size());

  for (size_t i = 0; i < permutation.size(); ++i) {
    for (size_t j = 0; j < inner_size; ++j) {
      shuffled[i * inner_size + j] = values[permutation[i] * inner_size + j];
    }
  }

  return ArrayAttr::get(shuffled, attr.getContext());
}

// Shuffle ranked tensor dimensions according to the permutation.
Type ShuffleRankedTensorType(Type type, ArrayRef<int64_t> permutation) {
  if (auto ranked_type = type.dyn_cast<RankedTensorType>()) {
    ArrayRef<int64_t> shape = ranked_type.getShape();
    assert(permutation.size() == shape.size());

    SmallVector<int64_t, 4> new_shape(permutation.size());
    for (size_t i = 0; i < permutation.size(); ++i)
      new_shape[i] = shape[permutation[i]];

    return RankedTensorType::get(new_shape, ranked_type.getElementType());
  }

  return type;
}

static bool AreCancellablePermutations(DenseIntElementsAttr perm0,
                                       DenseIntElementsAttr perm1) {
  if (perm0.getNumElements() == 0 || perm1.getNumElements() == 0) return false;
  if (perm0.getNumElements() != perm1.getNumElements()) return false;

  SmallVector<int64_t, 8> perm0_values;
  for (const auto &value : perm0.getIntValues())
    perm0_values.push_back(value.getSExtValue());

  SmallVector<int64_t, 8> perm1_values;
  for (const auto &value : perm1.getIntValues())
    perm1_values.push_back(value.getSExtValue());

  for (int i = 0; i < perm0_values.size(); ++i) {
    if (perm0_values[perm1_values[i]] != i) return false;
  }

  return true;
}

// Default implementation of `LayoutSensitiveInterface::UpdateDataFormat` for
// layout sensitive operations that do not have any additional layout dependent
// attributes besides `data_format` string.
template <typename Op>
LogicalResult UpdateDataFormat(StringRef data_format, Op *op) {
  auto perm = GetDataFormatPermutation(op->data_format(), data_format);
  if (perm.empty()) return failure();

  // Update data format attribute.
  op->setAttr("data_format", StringAttr::get(data_format, op->getContext()));

  // Update types for all layout sensitive results.
  auto layout_sensitive = cast<LayoutSensitiveInterface>(op->getOperation());
  for (unsigned idx : layout_sensitive.GetLayoutDependentResults()) {
    OpResult result = op->getOperation()->getResult(idx);
    result.setType(ShuffleRankedTensorType(result.getType(), perm));
  }

  return success();
}

// Default implementation for folding operand transpose into the operation.
// See `FoldOperandsTransposeInterface::FoldOperandsPermutation`.
template <typename Op>
LogicalResult FoldOperandsPermutation(
    ArrayRef<int64_t> permutation, Op *op,
    ArrayRef<std::pair<StringRef, ArrayAttr>> shuffle_attrs = {}) {
  MLIRContext *context = op->template getParentOfType<ModuleOp>().getContext();

  // We only support NHWC <-> NCHW permutations.
  static constexpr std::array<int64_t, 4> kNchwToNhwc = {0, 2, 3, 1};
  static constexpr std::array<int64_t, 4> kNhwcToNchw = {0, 3, 1, 2};

  // Operation data format after folding `permutation`.
  StringRef target_data_format = [&]() -> StringRef {
    if (op->data_format() == "NHWC" && permutation.equals(kNchwToNhwc)) {
      return "NCHW";  // cancel NCHW->NHWC operand permutation
    } else if (op->data_format() == "NCHW" && permutation.equals(kNhwcToNchw)) {
      return "NHWC";  // cancel NHWC->NCHW operand permutation
    } else {
      return "";
    }
  }();
  if (target_data_format.empty()) return failure();

  // To fold operand `permutation` into the `op` we need shuffle all layout
  // dependent attributes and types with a reverse permutation, and change
  // operation data format to `target_data_format`.
  //
  // Example:
  //   %1 = SomeOp(...)   {data_format = NHWC}
  //   %2 = Transpose(%1) {permutation = NHWC->NCHW}
  //   %3 = Op(%2)        {data_format = NCHW}
  //
  // To bypass %2 we have to change data format to shuffle data format from NCHW
  // to NHWC, which is the reverse of operand permutation (function argument).
  auto reverse_permutation =
      GetDataFormatPermutation(op->data_format(), target_data_format);
  if (reverse_permutation.empty()) return failure();

  op->setAttr("data_format", StringAttr::get(target_data_format, context));

  for (auto pair : shuffle_attrs) {
    StringRef attr_name = pair.first;
    ArrayAttr attr_value = pair.second;
    op->setAttr(attr_name, ShuffleArrayAttr(attr_value, reverse_permutation));
  }

  auto fold = cast<FoldOperandsTransposeInterface>(op->getOperation());
  for (unsigned idx : fold.GetLayoutDependentResults()) {
    OpResult result = op->getOperation()->getResult(idx);
    result.setType(
        ShuffleRankedTensorType(result.getType(), reverse_permutation));
  }

  return success();
}

//===----------------------------------------------------------------------===//
// Rewrite Pattern for removing trivial Arithmetic op.
//===----------------------------------------------------------------------===//

namespace {
// Folder that returns LHS of an Arithmetic Op if the RHS is a constant
// known to be Identity (e.g X+0)
template <
    typename OpT,
    typename std::enable_if<llvm::is_one_of<
        OpT, AddV2Op, SubOp, MulOp, DivOp, RealDivOp>::value>::type * = nullptr>
OpFoldResult IdentityArithmeticOpFolder(OpT arithmetic_op,
                                        ArrayRef<Attribute> operands) {
  auto result_op_type = arithmetic_op.getResult().getType();
  auto lhs_type = arithmetic_op.x().getType().template cast<ShapedType>();
  if (!result_op_type.template cast<ShapedType>().hasStaticShape()) return {};

  // We only handle non-broadcastable case.
  if (result_op_type != lhs_type) {
    return {};
  }

  // Mul and Div ops have identity value one while AddV2 and SubOp have identity
  // value zero.
  int identity =
      (std::is_same<OpT, MulOp>::value || std::is_same<OpT, DivOp>::value ||
       std::is_same<OpT, RealDivOp>::value);

  Type element_ty = lhs_type.getElementType();
  Attribute identity_attr;
  if (auto ty = element_ty.template dyn_cast<FloatType>()) {
    identity_attr = FloatAttr::get(ty, static_cast<double>(identity));
  } else if (auto ty = element_ty.template dyn_cast<IntegerType>()) {
    identity_attr = IntegerAttr::get(ty, static_cast<int64_t>(identity));
  } else {
    return {};
  }

  if (auto attr = operands[1].dyn_cast_or_null<DenseElementsAttr>()) {
    if (attr.isSplat() && attr.getSplatValue() == identity_attr)
      return arithmetic_op.x();
  }

  auto rhs_type = arithmetic_op.y().getType().template cast<ShapedType>();
  // TODO(chhe): we could fold and add an identity to force the broadcast.
  if (result_op_type != rhs_type) {
    return {};
  }

  bool is_symmetric =
      (std::is_same<OpT, AddV2Op>::value || std::is_same<OpT, MulOp>::value);
  if (auto attr = operands[0].dyn_cast_or_null<DenseElementsAttr>()) {
    if (is_symmetric && attr.isSplat() && attr.getSplatValue() == identity_attr)
      return arithmetic_op.y();
  }
  return {};
}
}  // namespace

namespace {
#include "tensorflow/compiler/mlir/tensorflow/transforms/generated_canonicalize.inc"
}  // namespace

//===----------------------------------------------------------------------===//
// AddOp
//===----------------------------------------------------------------------===//

void AddOp::getCanonicalizationPatterns(OwningRewritePatternList &results,
                                        MLIRContext *context) {
  results.insert<AddToAddV2>(context);
}

//===----------------------------------------------------------------------===//
// AddNOp
//===----------------------------------------------------------------------===//

OpFoldResult AddNOp::fold(ArrayRef<Attribute> operands) {
  if (operands.size() == 1) return *inputs().begin();
  return {};
}

//===----------------------------------------------------------------------===//
// AddV2Op
//===----------------------------------------------------------------------===//

void AddV2Op::getCanonicalizationPatterns(OwningRewritePatternList &results,
                                          MLIRContext *context) {
  results.insert<AddV2OfNegLeft, AddV2OfNegRight>(context);
}

OpFoldResult AddV2Op::fold(ArrayRef<Attribute> operands) {
  return IdentityArithmeticOpFolder<AddV2Op>(*this, operands);
}

//===----------------------------------------------------------------------===//
// AllOp
//===----------------------------------------------------------------------===//

// Verifies an reduction op's `input` and reduction `dims`.
static LogicalResult VerifyReductionInputAndDims(Value input, Value dims,
                                                 Location loc) {
  auto dims_type = dims.getType().dyn_cast<RankedTensorType>();
  if (!dims_type) return success();
  if (dims_type.getRank() > 1)
    return emitError(loc, "dimensions can only be 0D or 1D tensor");

  auto input_type = input.getType().dyn_cast<RankedTensorType>();
  if (!input_type) return success();
  int64_t rank = input_type.getRank();

  DenseIntElementsAttr dims_attr;
  if (!matchPattern(dims, m_Constant(&dims_attr))) return success();
  for (const auto &dim_pair : llvm::enumerate(dims_attr)) {
    int64_t cur_dim = dim_pair.value().getSExtValue();
    if (cur_dim < -rank || cur_dim >= rank)
      return emitError(loc)
             << dim_pair.index() << "-th dimension should be in the range of [-"
             << rank << ", " << rank << ")";
  }

  return success();
}

static LogicalResult Verify(AllOp op) {
  return VerifyReductionInputAndDims(op.input(), op.reduction_indices(),
                                     op.getLoc());
}

//===----------------------------------------------------------------------===//
// AnyOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(AnyOp op) {
  return VerifyReductionInputAndDims(op.input(), op.reduction_indices(),
                                     op.getLoc());
}

//===----------------------------------------------------------------------===//
// AssertOp
//===----------------------------------------------------------------------===//

namespace {

// Removes Assert with constant true predicate.
struct AssertWithTrue : public OpRewritePattern<AssertOp> {
  using OpRewritePattern<AssertOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(AssertOp op,
                                PatternRewriter &rewriter) const override {
    ElementsAttr cst;
    if (matchPattern(op.condition(), m_Constant(&cst))) {
      if (cst.getValue<BoolAttr>({}).getValue()) {
        rewriter.eraseOp(op);
        return success();
      }
    }
    return failure();
  }
};
}  // namespace

void AssertOp::getCanonicalizationPatterns(OwningRewritePatternList &results,
                                           MLIRContext *context) {
  results.insert<AssertWithTrue>(context);
}

//===----------------------------------------------------------------------===//
// BatchMatMulOp
//===----------------------------------------------------------------------===//

void BatchMatMulOp::getCanonicalizationPatterns(
    OwningRewritePatternList &results, MLIRContext *context) {
  results.insert<BatchMatMulToMatMul>(context);
}

//===----------------------------------------------------------------------===//
// BatchMatMulV2Op
//===----------------------------------------------------------------------===//

static LogicalResult Verify(BatchMatMulV2Op op) {
  if (!HasRankAtLeast(op.x(), 2)) {
    return op.emitOpError("requires lhs operand to have rank at least two");
  }
  if (!HasRankAtLeast(op.y(), 2)) {
    return op.emitOpError("requires rhs operand to have rank at least two");
  }
  return success();
}

void BatchMatMulV2Op::getCanonicalizationPatterns(
    OwningRewritePatternList &results, MLIRContext *context) {
  results.insert<BatchMatMulV2ToMatMul>(context);
}

//===----------------------------------------------------------------------===//
// BatchToSpaceOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(BatchToSpaceOp op) {
  // Op already has a constraint that block_size >= 2.
  int64_t block_size = op.block_size().getSExtValue();

  llvm::SmallVector<int64_t, 4> input_shape(4, ShapedType::kDynamicSize);
  auto input_type = op.input().getType().cast<TensorType>();
  if (input_type.hasRank()) {
    if (input_type.getRank() != 4)
      return op.emitOpError()
             << "requires input to be a 4D tensor, but got " << input_type;

    int64_t input_batch = input_type.getDimSize(0);
    if (input_batch != ShapedType::kDynamicSize &&
        input_batch % (block_size * block_size) != 0) {
      return op.emitOpError()
             << "requires input batch (dimension 0) to be evenly divisible "
                "by (block_size * block_size), but got input batch "
             << input_batch << " and block_size " << block_size;
    }

    input_shape.assign(input_type.getShape().begin(),
                       input_type.getShape().end());
  }

  auto crops_type = op.crops().getType().cast<TensorType>();
  if (crops_type.hasRank()) {
    if (crops_type.getRank() != 2)
      return op.emitOpError()
             << "requires crops to be a 2D tensor, but got " << crops_type;

    auto dim_of_size = [&](int64_t dim, int64_t size) {
      if (crops_type.isDynamicDim(dim)) return true;
      return crops_type.getDimSize(dim) == size;
    };
    if (!dim_of_size(0, 2) || !dim_of_size(1, 2))
      return op.emitOpError()
             << "requires crops to be a tensor<2x2>, but got " << crops_type;
  }

  DenseIntElementsAttr crops_attr;
  // Crops are defined as [[crop_top, crop_bottom], [crop_left, crop_right]],
  // and flattened as [crop_top, crop_bottom, crop_left, crop_right]
  llvm::SmallVector<int64_t, 4> crops_values;
  if (matchPattern(op.crops(), m_Constant(&crops_attr))) {
    assert(crops_attr.getNumElements() == 4 &&
           "tf.BatchToSpace crops must have 4 elements");

    auto crops_range = crops_attr.getIntValues();
    for (const auto &crops_value : crops_range) {
      int64_t crops_value_int = crops_value.getSExtValue();
      if (crops_value_int < 0)
        return op.emitOpError()
               << "requires all crop values to be nonnegative, but got "
               << crops_attr;

      crops_values.push_back(crops_value_int);
    }
  }

  auto output_type = op.output().getType().cast<TensorType>();
  if (output_type.hasRank()) {
    if (output_type.getRank() != 4)
      return op.emitOpError()
             << "requires output to be a 4D tensor, but got " << output_type;

    auto static_dims = [](int64_t dim_a, int64_t dim_b) {
      return dim_a != ShapedType::kDynamicSize &&
             dim_b != ShapedType::kDynamicSize;
    };

    auto output_shape = output_type.getShape();

    // output batch = input batch / (block_size * block_size).
    int64_t input_batch = input_shape[0];
    int64_t output_batch = output_shape[0];
    if (static_dims(input_batch, output_batch) &&
        (output_batch * block_size * block_size) != input_batch)
      return op.emitOpError()
             << "requires output batch (dimension 0) to be equal to input "
                "batch (dimension 0) / (block_size * block_size), but got "
                "output batch "
             << output_batch << ", input batch " << input_batch
             << ", and block_size " << block_size;

    auto check_spatial_dim = [&](int64_t spatial_dim_index,
                                 llvm::StringRef dim_name,
                                 llvm::StringRef crop_a_name,
                                 llvm::StringRef crop_b_name) -> LogicalResult {
      int64_t input_dim = input_shape[spatial_dim_index];
      int64_t output_dim = output_shape[spatial_dim_index];
      if (!static_dims(input_dim, output_dim)) return success();

      int64_t input_dim_pad = input_dim * block_size;
      // If crops are unknown, the maximum output spatial dim size is input
      // spatial dim size * block_size, as crops can be minimum 0.
      if (crops_values.empty() && output_dim > input_dim * block_size)
        return op.emitOpError()
               << "requires output " << dim_name << " (dimension "
               << spatial_dim_index << ") to be less than or equal to input "
               << dim_name << " (dimension " << spatial_dim_index
               << ") * block_size, but got output " << dim_name << " "
               << output_dim << ", input " << dim_name << " " << input_dim
               << ", and block_size " << block_size;

      if (!crops_values.empty()) {
        // output spatial dim = input spatial dim * block_size - crops.
        int64_t crop_a = crops_values[2 * (spatial_dim_index - 1)];
        int64_t crop_b = crops_values[2 * (spatial_dim_index - 1) + 1];
        if (output_dim != input_dim_pad - crop_a - crop_b)
          return op.emitOpError()
                 << "requires output " << dim_name << " (dimension "
                 << spatial_dim_index << ") to be equal to input " << dim_name
                 << " (dimension " << spatial_dim_index << ") * block_size - "
                 << crop_a_name << " - " << crop_b_name << ", but got output "
                 << dim_name << " " << output_dim << ", input " << dim_name
                 << " " << input_dim << ", " << crop_a_name << " " << crop_a
                 << ", " << crop_b_name << " " << crop_b << ", and block_size "
                 << block_size;
      }

      return success();
    };

    if (failed(check_spatial_dim(1, "height", "crop_top", "crop_bottom")) ||
        failed(check_spatial_dim(2, "width", "crop_left", "crop_right")))
      return failure();

    int64_t input_depth = input_shape[3];
    int64_t output_depth = output_shape[3];
    if (static_dims(input_depth, output_depth) && output_depth != input_depth)
      return op.emitOpError()
             << "requires output depth (dimension 3) to be equal to input "
                "depth (dimension 3), but got output depth "
             << output_depth << " and input depth " << input_depth;
  }

  return success();
}

void BatchToSpaceOp::getCanonicalizationPatterns(
    OwningRewritePatternList &results, MLIRContext *context) {
  results.insert<BatchToSpaceToBatchToSpaceND>(context);
}

//===----------------------------------------------------------------------===//
// BiasAddOp
//===----------------------------------------------------------------------===//

// Verifies that,
// * the value and bias operands have valid ranks or are unranked.
// * Channel dimension of the value operand and length of bias matches if they
//   are not unknown.
//
static LogicalResult Verify(BiasAddOp op) {
  StringRef format = op.data_format();
  if (format == "NHWC") {
    if (!HasRankAtLeast(op.value(), 2))
      return op.emitOpError(
          "requires value operand to have rank at least two with `NHWC` data "
          "format");
  } else {
    // Op definition requires data_format to be either NHWC or NCHW.
    DCHECK_EQ(format.str(), "NCHW");
    if (!HasRankAtLeast(op.value(), 3))
      return op.emitOpError(
          "requires value operand to have rank at least three with `NCHW` data "
          "format");
  }

  if (!IsOfRankOrUnranked(op.bias(), 1))
    return op.emitOpError("requires bias operand to have rank exactly one");

  RankedTensorType value_ty = op.value().getType().dyn_cast<RankedTensorType>();
  RankedTensorType bias_ty = op.bias().getType().dyn_cast<RankedTensorType>();
  if (!bias_ty || !value_ty) return success();

  // TODO(hinsu): Leverage tensor_format.h utility in TensorFlow to compute
  // dimension indices based on format.
  int64_t feature_dim_idx = format == "NHWC" ? value_ty.getRank() - 1 : 1;
  int64_t feature_dim = value_ty.getDimSize(feature_dim_idx);
  int64_t bias_len = bias_ty.getDimSize(0);
  if (feature_dim != -1 && bias_len != -1 && feature_dim != bias_len) {
    return op.emitOpError()
           << "requires channel dimension and feature dimension to match; "
              "found "
           << feature_dim << " and " << bias_len << ", respectively";
  }
  return success();
}

//===----------------------------------------------------------------------===//
// BiasAddGradOp
//===----------------------------------------------------------------------===//

// Verifies that,
// * the out_backprop operands have valid ranks or are unranked.
//
static LogicalResult Verify(BiasAddGradOp op) {
  StringRef format = op.data_format();
  if (format == "NHWC") {
    if (!HasRankAtLeast(op.out_backprop(), 2))
      return op.emitOpError(
          "requires out_backprop operand to have rank at least two with `NHWC` "
          "data format");
  } else {
    // Op definition requires data_format to be either NHWC or NCHW.
    DCHECK_EQ(format.str(), "NCHW");
    if (!HasRankAtLeast(op.out_backprop(), 3))
      return op.emitOpError(
          "requires out_backprop operand to have rank at least three with "
          "`NCHW` data format");
  }

  return success();
}

//===----------------------------------------------------------------------===//
// BiasAddV1Op
//===----------------------------------------------------------------------===//

void BiasAddV1Op::getCanonicalizationPatterns(OwningRewritePatternList &results,
                                              MLIRContext *context) {
  results.insert<BiasAddV1ToBiasAdd>(context);
}

//===----------------------------------------------------------------------===//
// BitcastOp
//===----------------------------------------------------------------------===//

void BitcastOp::getCanonicalizationPatterns(OwningRewritePatternList &results,
                                            MLIRContext *context) {
  results.insert<BitcastSameType, BitcastNested>(context);
}

//===----------------------------------------------------------------------===//
// BroadcastToOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(BroadcastToOp op) {
  // TODO(antiagainst): check that
  // * The 'shape' input is an 1-D int tensor.
  // * Each dimension pair of the source and target shapes are either equal
  //   or one of them is one.
  return success();
}

//===----------------------------------------------------------------------===//
// CaseOp
//===----------------------------------------------------------------------===//

class FoldConstantCaseOp : public OpRewritePattern<TF::CaseOp> {
 public:
  explicit FoldConstantCaseOp(MLIRContext *context)
      : OpRewritePattern<TF::CaseOp>(context) {}
  LogicalResult matchAndRewrite(TF::CaseOp op,
                                PatternRewriter &rewriter) const override;
};

LogicalResult FoldConstantCaseOp::matchAndRewrite(
    TF::CaseOp op, PatternRewriter &rewriter) const {
  // Extract the constant cond value.
  DenseIntElementsAttr branch;
  if (!matchPattern(op.branch_index(), m_Constant(&branch))) return failure();

  // Only attempt to fold scalar valued case statements.
  // TODO(jpienaar): This can be removed if CaseOp's verifier covers it.
  if (!branch.getType().cast<RankedTensorType>().getShape().empty())
    return failure();

  int index = *branch.getValues<int>().begin();
  // TODO(jpienaar): This can be removed if CaseOp's verifier covers it.
  if (index >= op.branches().size()) return failure();

  auto func = op.branches()[index].cast<SymbolRefAttr>();
  auto empty = rewriter.getStringAttr("");
  auto call_op = rewriter.create<PartitionedCallOp>(
      op.getLoc(), op.getResultTypes(), op.getOperands().drop_front(), func,
      /*config=*/empty, /*config_proto=*/empty, /*executor_type=*/empty);
  PropagateAttributes(op.getOperation(), call_op);
  rewriter.replaceOp(op, call_op.getResults());
  return success();
}

void CaseOp::getCanonicalizationPatterns(OwningRewritePatternList &results,
                                         MLIRContext *context) {
  results.insert<FoldConstantCaseOp>(context);
}

//===----------------------------------------------------------------------===//
// CastOp
//===----------------------------------------------------------------------===//

OpFoldResult CastOp::fold(ArrayRef<Attribute> operands) {
  // Cast with the same type is a no-op.
  Value operand = getOperand();
  if (getType() == operand.getType()) return operand;
  return {};
}

//===----------------------------------------------------------------------===//
// ConcatOp and ConcatV2Op
//===----------------------------------------------------------------------===//

template <typename OpT,
          typename std::enable_if<llvm::is_one_of<
              OpT, ConcatOp, ConcatV2Op>::value>::type * = nullptr>
static LogicalResult Verify(OpT op) {
  // TODO(hinsu): Convert variadic length attributes to derived attributes.
  Operation::operand_range values = op.values();

  int axis_idx = std::is_same<OpT, ConcatOp>() ? 0 : 1;
  Value axis = *op.getODSOperands(axis_idx).begin();
  if (!HasRankAtMost(axis, 1)) {
    return op.emitOpError(
        "requires axis to be of scalar type (or vector type for older "
        "versions)");
  }

  return VerifyTypesCompatibility(values,
                                  /*mask_one_dim=*/true, op.getOperation());
}

void ConcatOp::getCanonicalizationPatterns(OwningRewritePatternList &results,
                                           MLIRContext *context) {
  results.insert<ConvertToConcatV2>(context);
}

//===----------------------------------------------------------------------===//
// ConcatOffsetOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(ConcatOffsetOp op) {
  if (op.N() < 2)
    return op.emitOpError() << "requires N to be at least 2, got " << op.N();

  if (op.shape().size() != op.offset().size())
    return op.emitOpError()
           << "requires sizes of shapes and offsets to be the same, got sizes "
           << op.shape().size() << " and " << op.offset().size();

  auto ranked_dim = op.concat_dim().getType().dyn_cast<RankedTensorType>();
  if (ranked_dim && ranked_dim.getRank() != 0)
    return op.emitOpError()
           << "requires concat_dim to be a scalar, got tensor of rank "
           << ranked_dim.getRank();

  int64_t num_dims = -1;
  for (auto shape_offset_idx :
       llvm::enumerate(llvm::zip(op.shape(), op.offset()))) {
    Value shape = std::get<0>(shape_offset_idx.value());
    Value offset = std::get<1>(shape_offset_idx.value());
    const size_t idx = shape_offset_idx.index();

    if (failed(verifyCompatibleShape(shape.getType(), offset.getType())))
      return op.emitOpError() << "requires operand and result " << idx
                              << " to have compatible shapes";

    auto ranked_shape = shape.getType().dyn_cast<RankedTensorType>();
    if (!ranked_shape) continue;

    if (ranked_shape.getRank() != 1)
      return op.emitOpError() << "requires shape tensor operand " << idx
                              << " to be of rank 1, got tensor of rank "
                              << ranked_shape.getRank();

    if (!ranked_shape.hasStaticShape()) continue;

    int64_t ranked_shape_dim = ranked_shape.getDimSize(0);
    if (num_dims == -1)
      num_dims = ranked_shape_dim;
    else if (ranked_shape_dim != num_dims)
      return op.emitOpError()
             << "requires shape tensor (rank 1) operand " << idx
             << " to be of length " << num_dims
             << ", got tensor (rank 1) of length " << ranked_shape_dim;
  }

  return success();
}

LogicalResult ConcatOffsetOp::fold(ArrayRef<Attribute> operands,
                                   SmallVectorImpl<OpFoldResult> &results) {
  // ConcatOffset must have its first operand be concat_dim and at least two
  // shape tensors in variadic shapes operand.
  if (operands.size() < 3) return failure();

  // Check concat_dim is a scalar.
  auto concat_dim_attr = operands[0].dyn_cast_or_null<DenseIntElementsAttr>();
  if (!concat_dim_attr || concat_dim_attr.getType().getRank() != 0)
    return failure();

  llvm::SmallVector<DenseIntElementsAttr, 4> shapes;
  shapes.reserve(operands.size() - 1);
  for (Attribute shape : llvm::drop_begin(operands, 1))
    if (auto shape_attr = shape.dyn_cast_or_null<DenseIntElementsAttr>())
      shapes.push_back(shape_attr);
    else
      return failure();

  // Check all shapes are vectors of the same length.
  if (shapes.front().getType().getRank() != 1) return success();
  const int64_t num_dims = shapes.front().getNumElements();
  for (DenseIntElementsAttr shape : llvm::drop_begin(shapes, 1))
    if (shape.getType().getRank() != 1 || shape.getNumElements() != num_dims)
      return failure();

  // Check concat_dim is within [-num_dims, num_dims).
  int32_t concat_dim = (*concat_dim_attr.getValues<int32_t>().begin());
  if (concat_dim < 0) concat_dim += num_dims;
  if (concat_dim >= num_dims || concat_dim < 0) return failure();

  // Check all elements besides at concat_dim match across all shape tensors.
  SmallVector<int32_t, 4> shape0;
  shape0.reserve(num_dims);
  for (int32_t dim : shapes.front().getValues<int32_t>()) shape0.push_back(dim);

  for (DenseIntElementsAttr shape : llvm::drop_begin(shapes, 1)) {
    for (auto dims_and_idx : llvm::enumerate(llvm::zip(shape0, shape))) {
      if (dims_and_idx.index() == concat_dim) continue;

      if (std::get<0>(dims_and_idx.value()) !=
          std::get<1>(dims_and_idx.value()).getSExtValue())
        return failure();
    }
  }

  // Compute an exclusive cumulative sum of elements at concat_dim.
  results.reserve(shapes.size());
  SmallVector<int32_t, 4> cumulative_sum(num_dims, 0);
  RankedTensorType offset_type =
      RankedTensorType::get({num_dims}, IntegerType::get(32, getContext()));
  for (DenseIntElementsAttr shape : shapes) {
    results.push_back(DenseIntElementsAttr::get(offset_type, cumulative_sum));
    cumulative_sum[concat_dim] += shape.getValue<int32_t>(concat_dim);
  }

  return success();
}

//===----------------------------------------------------------------------===//
// ConjOp
//===----------------------------------------------------------------------===//

void ConjOp::getCanonicalizationPatterns(OwningRewritePatternList &results,
                                         MLIRContext *context) {
  results.insert<ConjNested>(context);
}

//===----------------------------------------------------------------------===//
// ConstOp
//===----------------------------------------------------------------------===//

OpFoldResult ConstOp::fold(ArrayRef<Attribute> operands) {
  assert(operands.empty() && "constant has no operands");

  // Return the held attribute value.
  return value();
}

// Builds a constant op with the specified attribute `value`. The result
// op's type is deduced from `value`; if `value` is of scalar type,
// wraps it up with a tensor type of empty shape.
// TODO(jpienaar): This one differs from the autogenerated one as it takes an
// attribute but always creates an ElementsAttr internally.
void ConstOp::build(OpBuilder &builder, OperationState &result,
                    Attribute value) {
  ShapedType type;
  if (auto elem_attr = value.dyn_cast<ElementsAttr>()) {
    return ConstOp::build(builder, result, elem_attr);
  } else if (value.isa<BoolAttr>() || value.isa<FloatAttr>() ||
             value.isa<IntegerAttr>()) {
    // All TensorFlow types must be tensor types. In the build() method,
    // we want to provide more flexibility by allowing attributes of scalar
    // types. But we need to wrap it up with ElementsAttr to construct
    // valid TensorFlow constants.
    type = RankedTensorType::get(/*shape=*/{}, value.getType());
    return ConstOp::build(builder, result, DenseElementsAttr::get(type, value));
  }
  // TODO(jpienaar): support other TensorFlow specific types.
  llvm_unreachable("unsupported attribute type for building tf.Const");
}

void ConstOp::build(OpBuilder &builder, OperationState &result, Type type,
                    Attribute value) {
  // Handle the case where the type and value are already tensors.
  if (type.isa<TensorType>() && value.isa<ElementsAttr>()) {
    result.addTypes(type);
    result.addAttribute("value", value);
    return;
  }

  // Otherwise, default to the attribute builder.
  ConstOp::build(builder, result, value);
  assert(type == result.types[0] && "type mismatch in construction");
}

LogicalResult ConstOp::inferReturnTypes(
    MLIRContext *context, Optional<Location> location, ValueRange operands,
    DictionaryAttr attributes, RegionRange regions,
    SmallVectorImpl<Type> &inferredReturnTypes) {
  auto value = attributes.get("value");
  if (!value) return emitOptionalError(location, "missing attribute 'value'");
  if (auto elem_attr = value.dyn_cast<ElementsAttr>()) {
    inferredReturnTypes.assign({elem_attr.getType()});
    return success();
  }
  return emitOptionalError(location,
                           "attribute 'value' failed to satisfy constraint: "
                           "constant vector/tensor");
}

//===----------------------------------------------------------------------===//
// Conv2DOp and Conv3DOp
//===----------------------------------------------------------------------===//

template <typename OpT>
static LogicalResult VerifyConvOpAttributes(OpT op, int num_dims) {
  if (!IsOfRankOrUnranked(op.getResult(), num_dims))
    return op.emitOpError()
           << "requires result to be " << num_dims << "D tensor";

  auto is_not_positive = [](Attribute val) {
    return val.cast<IntegerAttr>().getValue().getSExtValue() <= 0;
  };

  int64_t strides_size = op.strides().size();
  if (strides_size != num_dims)
    return op.emitOpError() << "requires strides attribute length to be "
                            << num_dims << "; actual length " << strides_size;
  if (llvm::any_of(op.strides().getValue(), is_not_positive))
    return op.emitOpError("requires positive strides");

  int64_t dilations_size = op.strides().size();
  if (op.dilations().size() != num_dims)
    return op.emitOpError() << "requires dilations attribute length to be "
                            << num_dims << "; actual length " << dilations_size;
  if (llvm::any_of(op.dilations().getValue(), is_not_positive))
    return op.emitOpError("requires positive dilations");

  return success();
}

// Verifies that,
// * Ranks of operands and result are valid
// * Number of input channels is divisible by the number of filter input
//   channels
// * Length of explicit_paddings attribute is valid and has non negative
//   elements
// * strides and dilations attributes have positive elements
template <typename OpT, typename std::enable_if<llvm::is_one_of<
                            OpT, Conv2DOp, Conv3DOp>::value>::type * = nullptr>
static LogicalResult Verify(OpT op) {
  int num_spatial_dims = std::is_same<OpT, Conv2DOp>() ? 2 : 3;
  int num_dims = 2 + num_spatial_dims;

  if (!IsOfRankOrUnranked(op.input(), num_dims) ||
      !IsOfRankOrUnranked(op.filter(), num_dims))
    return op.emitOpError()
           << "requires operands to be " << num_dims << "D tensor";

  // EXPLICIT padding mode and the associated attribute is limited to Conv2D.
  // So, fetch attribute by string instead of the op.explicit_paddings()
  // attribute getter.
  if (op.padding() == "EXPLICIT") {
    auto paddings = op.template getAttrOfType<ArrayAttr>("explicit_paddings");
    if (!paddings)
      return op.emitOpError() << "requires attribute 'explicit_paddings' with "
                                 "'EXPLICIT' padding mode";

    int64_t paddings_size = paddings.size();
    int64_t expected_size = 2 * num_dims;

    if (paddings_size != expected_size)
      return op.emitOpError()
             << "requires explicit_paddings attribute length to be "
             << expected_size << "; actual length " << paddings_size;

    auto is_negative = [](Attribute val) {
      return val.cast<IntegerAttr>().getValue().getSExtValue() < 0;
    };
    if (llvm::any_of(paddings.getValue(), is_negative))
      return op.emitOpError("requires non negative explicit paddings");
  }

  LogicalResult verify_result = VerifyConvOpAttributes(op, num_dims);
  if (failed(verify_result)) {
    return verify_result;
  }

  int64_t input_channels = -1;
  if (auto ty = op.input().getType().template dyn_cast<RankedTensorType>()) {
    std::string data_format = op.data_format().str();
    tensorflow::TensorFormat format;
    auto is_valid = FormatFromString(data_format, &format);
    DCHECK(is_valid) << data_format;
    int idx = tensorflow::GetTensorFeatureDimIndex(num_dims, format);
    input_channels = ty.getDimSize(idx);
  }

  int64_t filter_channels = -1;
  if (auto ty = op.filter().getType().template dyn_cast<RankedTensorType>()) {
    int idx = tensorflow::GetFilterTensorInputChannelsDimIndex(
        num_dims, tensorflow::FORMAT_HWIO);
    filter_channels = ty.getDimSize(idx);
  }

  if (input_channels != -1 && filter_channels != -1 &&
      input_channels % filter_channels != 0)
    return op.emitOpError()
           << "requires the number of input channels to be divisible by the "
              "number of filter input channels; found "
           << input_channels << " and " << filter_channels << ", respectively";

  return success();
}

LogicalResult Conv2DOp::UpdateDataFormat(StringRef data_format) {
  auto perm = GetDataFormatPermutation(this->data_format(), data_format);
  if (perm.empty()) return failure();

  // Update data_format attribute and result types.
  if (failed(::mlir::TF::UpdateDataFormat(data_format, this))) return failure();

  // Update convolution attributes.
  setAttr("dilations", ShuffleArrayAttr(dilations(), perm));
  setAttr("strides", ShuffleArrayAttr(strides(), perm));
  setAttr("explicit_paddings", ShuffleArrayAttr(explicit_paddings(), perm, 2));

  return success();
}

StringRef Conv2DOp::GetOptimalLayout(const RuntimeDevices &devices) {
  // Keep current data format if no GPUs are available or if explicit placement
  // does not allow to use GPU for this operation.
  if (!CanUseGpuDevice(devices) || !CanUseGpuDevice(getOperation()))
    return data_format();

  // Input must be a tensor.
  auto input_ty = input().getType().dyn_cast<TensorType>();
  if (!input_ty) return data_format();

  // For f16 data type on devices with Tensor Cores support NHWC data format
  // is up to ~2x faster.
  const bool is_f16 = input_ty.getElementType().isF16();
  if (is_f16 && CanUseTensorCores(devices)) return "NHWC";

  // For f32/f16 data type decision depends on the filter size in spatial
  // dimensions, for other data types we keep current data format.
  if (!input_ty.getElementType().isF32() && !input_ty.getElementType().isF16())
    return data_format();

  // Keep current data format if filter rank is unknown or not equal to 4.
  auto filter_ty = filter().getType().dyn_cast<RankedTensorType>();
  if (!filter_ty || filter_ty.getRank() != 4) return data_format();

  const int64_t d0 = filter_ty.getDimSize(0);
  const int64_t d1 = filter_ty.getDimSize(1);

  auto all_ones = [](ArrayAttr arr) -> bool {
    return llvm::all_of(arr, [](Attribute attr) -> bool {
      return attr.cast<IntegerAttr>().getInt() == 1;
    });
  };

  // Convolutions with 1x1 filter and with strides and dilations all ones, can
  // be computed as a GEMM in NHWC data format, and can be up to ~2x times
  // faster than convolution in NCHW.
  const bool one_by_one = d0 == 1 && d1 == 1;
  const bool trivial_strides = all_ones(strides());
  const bool trivial_dilations = all_ones(dilations());

  // TODO(ezhulenev): This might lead to excessive transposes in the final IR,
  // if the ratio of 1x1 convolutions to regular convolutions is close to 1:1.
  // Also FusedBatchNorm in training mode prefers NCHW data format. Check if all
  // users can efficiently use NHWC data format?
  if (one_by_one && trivial_strides && trivial_dilations) {
    return "NHWC";
  }

  // If filter spatial dimensions are unknown or not 1x1 we prefer NCHW, because
  // it's the fastest option on NVIDIA GPUs with cuDNN library support.
  return "NCHW";
}

//===----------------------------------------------------------------------===//
// Conv2dBackpropFilterOp
//===----------------------------------------------------------------------===//

LogicalResult Conv2DBackpropFilterOp::UpdateDataFormat(StringRef data_format) {
  StringRef src_data_format = this->data_format();

  auto perm = GetDataFormatPermutation(src_data_format, data_format);
  if (perm.empty()) return failure();

  // Update data_format attribute and result types.
  if (failed(::mlir::TF::UpdateDataFormat(data_format, this))) return failure();

  // Update convolution attributes.
  setAttr("dilations", ShuffleArrayAttr(dilations(), perm));
  setAttr("strides", ShuffleArrayAttr(strides(), perm));
  setAttr("explicit_paddings", ShuffleArrayAttr(explicit_paddings(), perm, 2));

  // Permute filter sizes operand.
  OpBuilder builder(getOperation());
  auto filter_sizes_permuted = builder.create<TF::DataFormatVecPermuteOp>(
      getLoc(), filter_sizes(), StringAttr::get(src_data_format, getContext()),
      StringAttr::get(data_format, getContext()));
  setOperand(1, filter_sizes_permuted);

  return success();
}

StringRef Conv2DBackpropFilterOp::GetOptimalLayout(
    const RuntimeDevices &devices) {
  // Keep current data format if no GPUs are available or if explicit placement
  // does not allow to use GPU for this operation.
  if (!CanUseGpuDevice(devices) || !CanUseGpuDevice(getOperation()))
    return data_format();

  // Input must be a tensor.
  auto input_ty = input().getType().dyn_cast<TensorType>();
  if (!input_ty) return data_format();

  // For f16 data type on devices with Tensor Cores support NHWC data format
  // is up to ~2x faster.
  const bool is_f16 = input_ty.getElementType().isF16();
  if (is_f16 && CanUseTensorCores(devices)) return "NHWC";

  // Otherwise always use "NCHW".
  return "NCHW";
}

//===----------------------------------------------------------------------===//
// Conv2DBackpropInputOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(Conv2DBackpropInputOp op) {
  int num_spatial_dims = 2;
  int num_dims = 2 + num_spatial_dims;

  if (!IsOfRankOrUnranked(op.out_backprop(), num_dims) ||
      !IsOfRankOrUnranked(op.filter(), num_dims))
    return op.emitOpError()
           << "requires operands to be " << num_dims << "D tensor";

  LogicalResult verify_result = VerifyConvOpAttributes(op, num_dims);
  if (failed(verify_result)) {
    return verify_result;
  }

  return success();
}

LogicalResult Conv2DBackpropInputOp::UpdateDataFormat(StringRef data_format) {
  StringRef src_data_format = this->data_format();

  auto perm = GetDataFormatPermutation(src_data_format, data_format);
  if (perm.empty()) return failure();

  // Update data_format attribute and result types.
  if (failed(::mlir::TF::UpdateDataFormat(data_format, this))) return failure();

  // Update convolution attributes.
  setAttr("dilations", ShuffleArrayAttr(dilations(), perm));
  setAttr("strides", ShuffleArrayAttr(strides(), perm));
  setAttr("explicit_paddings", ShuffleArrayAttr(explicit_paddings(), perm, 2));

  // Permute input sizes operand.
  OpBuilder builder(getOperation());
  auto input_sizes_permuted = builder.create<TF::DataFormatVecPermuteOp>(
      getLoc(), input_sizes(), StringAttr::get(src_data_format, getContext()),
      StringAttr::get(data_format, getContext()));
  setOperand(0, input_sizes_permuted);

  return success();
}

StringRef Conv2DBackpropInputOp::GetOptimalLayout(
    const RuntimeDevices &devices) {
  // Keep current data format if no GPUs are available or if explicit placement
  // does not allow to use GPU for this operation.
  if (!CanUseGpuDevice(devices) || !CanUseGpuDevice(getOperation()))
    return data_format();

  // Filter must be a tensor.
  auto filter_ty = filter().getType().dyn_cast<TensorType>();
  if (!filter_ty) return data_format();

  // For f16 data type on devices with Tensor Cores support NHWC data format
  // is up to ~2x faster.
  const bool is_f16 = filter_ty.getElementType().isF16();
  if (is_f16 && CanUseTensorCores(devices)) return "NHWC";

  // Otherwise always use "NCHW".
  return "NCHW";
}

//===----------------------------------------------------------------------===//
// DataFormatVecPermuteOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(DataFormatVecPermuteOp op) {
  auto input_ty = op.x().getType().dyn_cast<RankedTensorType>();
  if (!input_ty) return success();

  int rank = input_ty.getRank();
  if (rank != 1 && rank != 2)
    return op.emitOpError("requires input of rank 1 or 2");

  if (rank == 1) {
    int64_t dim0 = input_ty.getDimSize(0);
    if (dim0 != ShapedType::kDynamicSize && dim0 != 4 && dim0 != 2)
      return op.emitOpError("requires 1D input of size 4 or size 2");
  }

  if (rank == 2) {
    int64_t dim0 = input_ty.getDimSize(0);
    if (dim0 != ShapedType::kDynamicSize && dim0 != 4)
      return op.emitOpError(
          "requires first dimensions of 2D input to be of size 4");

    int64_t dim1 = input_ty.getDimSize(1);
    if (dim1 != ShapedType::kDynamicSize && dim1 != 2)
      return op.emitOpError(
          "requires second dimensions of 2D input to be of size 2");
  }

  return success();
}

//===----------------------------------------------------------------------===//
// DivOp
//===----------------------------------------------------------------------===//

void DivOp::getCanonicalizationPatterns(OwningRewritePatternList &results,
                                        MLIRContext *context) {
  results.insert<DivWithSqrtDivisor>(context);
}

OpFoldResult DivOp::fold(ArrayRef<Attribute> operands) {
  return IdentityArithmeticOpFolder<DivOp>(*this, operands);
}

//===----------------------------------------------------------------------===//
// DynamicStitchOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(DynamicStitchOp op) {
  if (op.N() < 1) return op.emitOpError("requires attribute N with value >= 1");

  if (RankedTensorType out_ty = op.getType().dyn_cast<RankedTensorType>()) {
    if (out_ty.getRank() == 0) {
      return op.emitOpError("requires non scalar output");
    }
  }

  llvm::SmallDenseSet<int64_t, 8> index_values;
  bool all_indices_const = true;
  int32_t max_index = -1;
  llvm::Optional<SmallVector<int64_t, 4>> inferred_item_shape;
  for (auto it : llvm::zip(op.indices(), op.data())) {
    Value index = std::get<0>(it);

    DenseIntElementsAttr index_attr;
    if (matchPattern(index, m_Constant(&index_attr))) {
      for (int32_t index : index_attr.getValues<int32_t>()) {
        if (index < 0)
          return op.emitOpError()
                 << "requires non-negative index values; found " << index;
        max_index = std::max(index, max_index);
        index_values.insert(index);
      }
    } else {
      all_indices_const = false;
    }

    Value data = std::get<1>(it);
    RankedTensorType index_ty = index.getType().dyn_cast<RankedTensorType>();
    RankedTensorType data_ty = data.getType().dyn_cast<RankedTensorType>();
    if (!index_ty || !data_ty) continue;

    int64_t index_rank = index_ty.getRank();
    ArrayRef<int64_t> data_shape = data_ty.getShape();
    ArrayRef<int64_t> index_shape = index_ty.getShape();
    if (failed(mlir::verifyCompatibleShape(index_shape,
                                           data_shape.take_front(index_rank))))
      return op.emitOpError() << "requires shape of data with type " << data_ty
                              << " to have prefix matching with shape of the "
                                 "corresponding index type "
                              << index_ty;

    ArrayRef<int64_t> item_shape = data_shape.drop_front(index_rank);
    if (!inferred_item_shape) {
      inferred_item_shape = llvm::to_vector<4>(item_shape);
      continue;
    }

    if (failed(mlir::verifyCompatibleShape(item_shape, *inferred_item_shape)))
      return op.emitOpError() << "has inconsistent shaped data and index "
                                 "pairs; inferred item shapes ["
                              << llvm::makeArrayRef(*inferred_item_shape)
                              << "] and [" << item_shape << "] don't match";
    for (int i = 0, e = item_shape.size(); i < e; ++i) {
      int64_t &inferred_dim = (*inferred_item_shape)[i];
      int64_t dim = item_shape[i];
      if (ShapedType::isDynamic(inferred_dim)) inferred_dim = dim;
    }
  }

  // If all indices are constants, then verify that they cover all indices in
  // the range [0, max_index] and the output type is legal.
  if (all_indices_const) {
    for (int32_t i = 0; i <= max_index; i++) {
      if (!index_values.count(i))
        return op.emitOpError() << "missing index " << i;
    }

    if (inferred_item_shape) {
      SmallVector<int64_t, 4> expected_shape;
      expected_shape.push_back(max_index + 1);
      expected_shape.append(inferred_item_shape->begin(),
                            inferred_item_shape->end());

      auto out_ty = op.getType().cast<TensorType>();
      auto expected_out_ty =
          RankedTensorType::get(expected_shape, out_ty.getElementType());

      if (!AreCastCompatible({out_ty, expected_out_ty})) {
        return op.emitOpError() << "has invalid output type; should be "
                                   "compatible with inferred type "
                                << expected_out_ty;
      }
    }
  }

  return success();
}

//===----------------------------------------------------------------------===//
// EinsumOp
//===----------------------------------------------------------------------===//

// Verifies that,
// * Arity of the op is at most two.
//
// TODO(hinsu): Verify einsum equation attribute.
static LogicalResult Verify(EinsumOp op) {
  if (op.N() > 2) {
    return op.emitOpError("supports at most two operands");
  }
  return success();
}

//===----------------------------------------------------------------------===//
// EmptyOp
//===----------------------------------------------------------------------===//

OpFoldResult EmptyOp::fold(ArrayRef<Attribute> operands) {
  assert(operands.size() == 1 && "empty op has one operand");

  Attribute attr = operands.front();
  if (!attr) return {};

  auto int_attr = attr.cast<DenseIntElementsAttr>();
  SmallVector<int64_t, 6> out_shape;
  for (const auto val : int_attr.getValues<int32_t>()) {
    out_shape.push_back(val);
  }

  auto type = getResult().getType().cast<ShapedType>();
  auto etype = type.getElementType();

  // We can not fold if the result is not static.
  if (!type.hasStaticShape()) return {};

  if (auto float_type = etype.dyn_cast<FloatType>()) {
    auto out_type = RankedTensorType::get(out_shape, float_type);
    return DenseElementsAttr::get(out_type,
                                  {APFloat(float_type.getFloatSemantics())});
  }

  if (auto int_type = etype.dyn_cast<IntegerType>()) {
    auto out_type = RankedTensorType::get(out_shape, etype);
    APInt val(int_type.getWidth(), 0, int_type.getSignedness());
    return DenseElementsAttr::get(out_type, val);
  }

  return {};
}

//===----------------------------------------------------------------------===//
// EmptyTensorListOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(EmptyTensorListOp op) {
  if (!IsOfRankOrUnranked(op.element_shape(), 0) &&
      !IsOfRankOrUnranked(op.element_shape(), 1)) {
    return op.emitOpError("requires element_shape operand to be 0D/1D tensor");
  }

  if (!IsOfRankOrUnranked(op.max_num_elements(), 0)) {
    return op.emitOpError("requires max_num_elements operand to be 0D tensor");
  }
  return success();
}

//===----------------------------------------------------------------------===//
// EqualOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(EqualOp op) {
  // If we allow inputs to have incompatible type, then nothing to do.
  if (!op.incompatible_shape_error()) return success();

  // Otherwise, check inputs are broadcastable.
  return mlir::OpTrait::impl::verifyCompatibleOperandBroadcast(
      op.getOperation());
}

void EqualOp::build(OpBuilder &builder, OperationState &result, Value x,
                    Value y, BoolAttr incompatible_shape_error) {
  auto result_type = DeduceEqualCmpOpType(&builder, result.location, x, y,
                                          incompatible_shape_error);
  return build(builder, result, result_type, x, y, incompatible_shape_error);
}

//===----------------------------------------------------------------------===//
// ExpandDimsOp
//===----------------------------------------------------------------------===//

Type InferExpandDimsOpType(Value input, Value dim) {
  Type element_ty = input.getType().cast<TensorType>().getElementType();
  auto unranked_ty = UnrankedTensorType::get(element_ty);

  auto input_ty = input.getType().dyn_cast<RankedTensorType>();
  if (!input_ty) return unranked_ty;

  DenseIntElementsAttr dim_attr;
  if (!matchPattern(dim, m_Constant(&dim_attr)) ||
      dim_attr.getNumElements() != 1)
    return unranked_ty;
  int64_t dim_val = (*dim_attr.begin()).getSExtValue();
  int64_t input_rank = input_ty.getRank();

  if (dim_val < -input_rank - 1 || dim_val > input_rank + 1) return unranked_ty;
  if (dim_val < 0) dim_val += input_rank + 1;

  SmallVector<int64_t, 4> shape = llvm::to_vector<4>(input_ty.getShape());
  shape.insert(shape.begin() + dim_val, 1);
  return RankedTensorType::get(shape, element_ty);
}

void ExpandDimsOp::build(OpBuilder &builder, OperationState &result,
                         Value input, Value dim) {
  return build(builder, result, InferExpandDimsOpType(input, dim), input, dim);
}

//===----------------------------------------------------------------------===//
// FakeQuantWithMinMaxArgsOp
//===----------------------------------------------------------------------===//
static LogicalResult Verify(FakeQuantWithMinMaxArgsOp op) {
  // TODO(fengliuai): moving the following to an utility method.
  const llvm::fltSemantics &semantics = op.min().getSemantics();
  float rmin, rmax;
  if (&semantics == &APFloat::IEEEsingle()) {
    rmin = op.min().convertToFloat();
    rmax = op.max().convertToFloat();
  } else {
    rmin = op.min().convertToDouble();
    rmax = op.max().convertToDouble();
  }
  // Range boundaries must be valid.
  if (rmin >= rmax) {
    return op.emitOpError("range is invalid: [" + Twine(std::to_string(rmin)) +
                          "," + Twine(std::to_string(rmax)) + "]");
  }
  int64_t num_bits = op.num_bits().getSExtValue();
  if (num_bits < 2 || num_bits > 16) {
    return op.emitOpError(
        "requires num_bits to be between 2 and 16, inclusive");
  }
  return success();
}

//===----------------------------------------------------------------------===//
// FakeQuantWithMinMaxVarsOp
//===----------------------------------------------------------------------===//
static LogicalResult Verify(FakeQuantWithMinMaxVarsOp op) {
  auto min = GetRankedTensorTypeForOperand(op.min());
  if (min && !IsOfRankedFloatTensorType(min, 0))
    return op.emitOpError("requires min to be a 0d float tensor");

  auto max = GetRankedTensorTypeForOperand(op.max());
  if (max && !IsOfRankedFloatTensorType(max, 0))
    return op.emitOpError("requires max to be a 0d float tensor");

  int64_t num_bits = op.num_bits().getSExtValue();
  if (num_bits < 2 || num_bits > 16) {
    return op.emitOpError(
        "requires num_bits to be between 2 and 16, inclusive");
  }
  return success();
}

//===----------------------------------------------------------------------===//
// FakeQuantWithMinMaxVarsPerChannelOp
//===----------------------------------------------------------------------===//
static LogicalResult Verify(FakeQuantWithMinMaxVarsPerChannelOp op) {
  auto min = GetRankedTensorTypeForOperand(op.min());
  if (min && !IsOfRankedFloatTensorType(min, 1))
    return op.emitOpError("requires min to be a 1d float tensor");

  auto max = GetRankedTensorTypeForOperand(op.max());
  if (max && !IsOfRankedFloatTensorType(max, 1))
    return op.emitOpError("requires max to be a 1d float tensor");

  Value inputs = op.inputs();
  if (!HasRankAtLeast(inputs, 1))
    return op.emitError("requires inputs to be at least 1d float tensor");

  int64_t num_bits = op.num_bits().getSExtValue();
  if (num_bits < 2 || num_bits > 16) {
    return op.emitOpError(
        "requires num_bits to be between 2 and 16, inclusive");
  }

  auto inputs_type = inputs.getType().dyn_cast<RankedTensorType>();
  if (!inputs_type) return success();
  int depth = inputs_type.getDimSize(inputs_type.getRank() - 1);
  if ((min && min.getDimSize(0) != depth) ||
      (max && max.getDimSize(0) != depth)) {
    return op.emitOpError(
        "requires min and max to have same size as last dimension of inputs");
  }

  return success();
}

//===----------------------------------------------------------------------===//
// FillOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(FillOp op) {
  if (!IsOfRankOrUnranked(op.dims(), 1))
    return op.emitOpError() << "requires dims to be a 1D tensor";
  if (!IsOfRankOrUnranked(op.value(), 0))
    return op.emitOpError() << "requires value to be a scalar";

  return success();
}

static ShapedType InferFillOpType(Value dims, Value value) {
  Type etype = value.getType().cast<ShapedType>().getElementType();

  DenseIntElementsAttr dims_attr;
  if (!matchPattern(dims, m_Constant(&dims_attr))) {
    return UnrankedTensorType::get(etype);
  }

  llvm::SmallVector<int64_t, 4> shape;
  shape.reserve(dims_attr.getNumElements());
  for (const APInt dim : dims_attr.getValues<APInt>()) {
    shape.push_back(dim.getSExtValue());
  }
  return RankedTensorType::get(shape, etype);
}

void FillOp::build(OpBuilder &builder, OperationState &result, Value dims,
                   Value value) {
  FillOp::build(builder, result, InferFillOpType(dims, value), dims, value);
}

OpFoldResult FillOp::fold(ArrayRef<Attribute> operands) {
  assert(operands.size() == 2 && "fill op has two operand");

  auto type = getType().cast<ShapedType>();
  // DenseElementsAttr that is used in this folder only supports int and float
  // types.
  // TODO(hinsu): Handle complex types once there is a attribute kind for
  // complex.
  if (!type.getElementType().isIntOrFloat()) return {};

  auto value = operands[1].dyn_cast_or_null<ElementsAttr>();
  if (!value) return {};

  if (type.hasStaticShape())
    return DenseElementsAttr::get(type, value.getValue({}));

  auto dims = operands[0].dyn_cast_or_null<DenseIntElementsAttr>();
  if (!dims) return {};

  llvm::SmallVector<int64_t, 4> shape;
  shape.reserve(dims.getNumElements());
  for (const APInt dim : dims.getValues<APInt>()) {
    shape.push_back(dim.getSExtValue());
  }
  type = RankedTensorType::get(shape, type.getElementType());

  return DenseElementsAttr::get(type, value.getValue({}));
}

//===----------------------------------------------------------------------===//
// FusedBatchNormGradOp
//===----------------------------------------------------------------------===//

// TODO(b/150954845): Add benchmarks to verify that layout preference didn't
// change in the latest GPU generations.

LogicalResult FusedBatchNormGradV3Op::UpdateDataFormat(StringRef data_format) {
  return ::mlir::TF::UpdateDataFormat(data_format, this);
}

StringRef FusedBatchNormGradV3Op::GetOptimalLayout(
    const RuntimeDevices &devices) {
  // Keep current data format if no GPUs are available or if explicit placement
  // does not allow to use GPU for this operation.
  if (!CanUseGpuDevice(devices) || !CanUseGpuDevice(getOperation()))
    return data_format();

  // For f16 data type on devices with Tensor Cores support NHWC data format
  // is up to ~2x faster.
  auto x_ty = x().getType().cast<TensorType>();
  const bool is_f16 = x_ty.getElementType().isF16();
  if (is_f16 && CanUseTensorCores(devices)) return "NHWC";

  // For all other data types prefer NCHW.
  return "NCHW";
}

//===----------------------------------------------------------------------===//
// FusedBatchNormOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(FusedBatchNormOp op) {
  auto x = GetRankedTensorTypeForOperand(op.x());
  if (x && !IsOfRankedFloatTensorType(x, 4))
    return op.emitOpError("requires x to be a 4D float tensor");

  auto scale = GetRankedTensorTypeForOperand(op.scale());
  if (scale && !IsOfRankedFloatTensorType(scale, 1))
    return op.emitOpError("requires scale to be a 1D float tensor");

  auto offset = GetRankedTensorTypeForOperand(op.offset());
  if (offset && !IsOfRankedFloatTensorType(offset, 1))
    return op.emitOpError("requires offset to be a 1D float tensor");

  auto mean = GetRankedTensorTypeForOperand(op.mean());
  if (mean && !IsOfRankedFloatTensorType(mean, 1))
    return op.emitOpError("requires mean to be a 1D float tensor");

  auto variance = GetRankedTensorTypeForOperand(op.variance());
  if (variance && !IsOfRankedFloatTensorType(variance, 1))
    return op.emitOpError("requires variance to be a 1D float tensor");

  // TODO(antiagainst): check attributes

  return success();
}

LogicalResult FusedBatchNormV3Op::FoldOperandsPermutation(
    ArrayRef<int64_t> permutation) {
  // FusedBatchNorm in training mode is a layout sentitive operation, and should
  // have already assigned an optimal data format.
  if (is_training()) return failure();

  return ::mlir::TF::FoldOperandsPermutation(permutation, this);
}

LogicalResult FusedBatchNormV3Op::UpdateDataFormat(StringRef data_format) {
  return ::mlir::TF::UpdateDataFormat(data_format, this);
}

StringRef FusedBatchNormV3Op::GetOptimalLayout(const RuntimeDevices &devices) {
  // In inference mode FusedBatchNorm is not sensitive to data layout.
  if (!is_training()) return data_format();

  // Keep current data format if no GPUs are available or if explicit placement
  // does not allow to use GPU for this operation.
  if (!CanUseGpuDevice(devices) || !CanUseGpuDevice(getOperation()))
    return data_format();

  // For f16 data type on devices with Tensor Cores support NHWC data format
  // is up to ~2x faster.
  auto x_ty = x().getType().cast<TensorType>();
  const bool is_f16 = x_ty.getElementType().isF16();
  if (is_f16 && CanUseTensorCores(devices)) return "NHWC";

  // For all other data types prefer NCHW.
  return "NCHW";
}

//===----------------------------------------------------------------------===//
// GatherV2Op
//===----------------------------------------------------------------------===//

static LogicalResult Verify(GatherV2Op op) {
  int64_t batch_dims = op.batch_dims().getSExtValue();
  if (auto ty = op.indices().getType().dyn_cast<RankedTensorType>()) {
    int64_t rank = ty.getRank();
    if (batch_dims > rank || batch_dims < -rank)
      return op.emitOpError()
             << "batch_dims (" << batch_dims << ") must be in range [" << -rank
             << ", " << rank + 1 << ")";
    if (batch_dims < 0) batch_dims += rank;
  }

  if (!HasRankAtMost(op.axis(), 1))
    return op.emitOpError("requires axis to have rank at most 1");

  DenseIntElementsAttr axis_attr;
  if (matchPattern(op.axis(), m_Constant(&axis_attr))) {
    int64_t axis = (*axis_attr.begin()).getSExtValue();
    if (auto ty = op.params().getType().dyn_cast<RankedTensorType>()) {
      int64_t rank = ty.getRank();
      if (axis >= rank || axis < -rank)
        return op.emitOpError() << "axis (" << axis << ") must be in range ["
                                << -rank << ", " << rank << ")";
      if (axis < 0) axis += rank;
    }

    if (batch_dims >= 0 && axis >= 0 && axis < batch_dims) {
      return op.emitOpError() << "requires axis (" << axis
                              << ") to be greater than or equal to batch_dims ("
                              << batch_dims << ")";
    }
  }
  return success();
}

//===----------------------------------------------------------------------===//
// IfOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(IfOp op) {
  auto module = op.getParentOfType<ModuleOp>();
  auto then_fn = module.lookupSymbol<FuncOp>(op.then_branch());
  if (!then_fn)
    return op.emitOpError("then_branch refers to an undefined function : ")
           << op.then_branch();
  auto else_fn = module.lookupSymbol<FuncOp>(op.else_branch());
  if (!else_fn)
    return op.emitOpError("else_branch refers to an undefined function : ")
           << op.else_branch();
  auto then_fn_type = then_fn.getType();
  auto else_fn_type = else_fn.getType();

  // Non-conditional operands starting with the second operand are passed to
  // branches and should be pair-wise compatible with branches' inputs.
  unsigned expected_num_inputs = op.getNumOperands() - 1;
  if (then_fn_type.getNumInputs() != expected_num_inputs ||
      else_fn_type.getNumInputs() != expected_num_inputs)
    return op.emitError("branches should have " + Twine(expected_num_inputs) +
                        " inputs");

  for (unsigned i = 0; i < expected_num_inputs; ++i) {
    auto operand_type = op.getOperand(i + 1).getType().cast<TensorType>();
    auto then_input_type = then_fn_type.getInput(i).cast<TensorType>();
    if (!AreCastCompatible({operand_type, then_input_type}))
      return op.emitError(
          llvm::formatv("then branch input type {0} is incompatible with "
                        "operand type {1} at index {2}",
                        then_input_type, operand_type, i));

    auto else_input_type = else_fn_type.getInput(i).cast<TensorType>();
    if (!AreCastCompatible({operand_type, else_input_type}))
      return op.emitError(
          llvm::formatv("else branch input type {0} is incompatible with "
                        "operand type {1} at index {2}",
                        else_input_type, operand_type, i));

    // If branches have incompatible input types that means that no tensor can
    // serve as input to both the functions. Hence, the op is invalid.
    if (!AreCastCompatible({then_input_type, else_input_type}))
      return op.emitError(llvm::formatv(
          "branches inputs have incompatible types {0} and {1} at index {2}",
          then_input_type, else_input_type, i));
  }

  // Branches' results should be pair-wise compatible with the op results.
  unsigned expected_num_results = op.getNumResults();
  if (then_fn_type.getNumResults() != expected_num_results ||
      else_fn_type.getNumResults() != expected_num_results)
    return op.emitError("branches should have " + Twine(expected_num_results) +
                        " results");

  for (unsigned i = 0; i < expected_num_results; ++i) {
    auto result_type = op.getResult(i).getType().cast<TensorType>();
    auto then_result_type = then_fn_type.getResult(i).cast<TensorType>();
    if (!AreCastCompatible({then_result_type, result_type}))
      return op.emitError(
          llvm::formatv("then branch result type {0} is incompatible with op "
                        "result type {1} at index {2}",
                        then_result_type, result_type, i));

    auto else_result_type = else_fn_type.getResult(i).cast<TensorType>();
    if (!AreCastCompatible({else_result_type, result_type}))
      return op.emitError(
          llvm::formatv("else branch result type {0} is incompatible with op "
                        "result type {1} at index {2}",
                        else_result_type, result_type, i));
  }
  return success();
}

//===----------------------------------------------------------------------===//
// YieldOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(YieldOp op) {
  auto parent = op.getParentOp();
  // A YieldOp should be contained within an IfRegion op
  // (and WhileRegion in future)
  if (!isa<IfRegionOp>(parent))
    op.emitError() << " expects parent op "
                   << "'" << IfRegionOp::getOperationName() << "' but got '"
                   << parent->getName().getStringRef() << "'";
  return success();
}

//===----------------------------------------------------------------------===//
// IfRegionOp
//===----------------------------------------------------------------------===//

LogicalResult VerifyRegionResults(Operation *op, Region &region,
                                  StringRef region_name) {
  auto op_name = op->getName().getStringRef();
  // verify that op outputs match yield inputs
  YieldOp yield = cast<YieldOp>(region.front().getTerminator());
  unsigned expected_num_results = op->getNumResults();
  if (yield.getNumOperands() != expected_num_results)
    return op->emitError(region_name + " region should have " +
                         Twine(expected_num_results) + " results");

  for (int idx : llvm::seq<int>(0, expected_num_results)) {
    auto op_result_type = op->getResult(idx).getType().cast<TensorType>();
    auto region_result_type =
        yield.getOperand(idx).getType().cast<TensorType>();
    if (!AreCastCompatible({region_result_type, op_result_type}))
      return op->emitError(llvm::formatv(
          "{0} result type {1} is incompatible with {2} "
          "result type {3} at index {4}",
          region_name, region_result_type, op_name, op_result_type, idx));
  }
  return success();
}

static LogicalResult Verify(IfRegionOp op) {
  if (failed(VerifyRegionResults(op, op.then_branch(), "then")))
    return failure();
  if (failed(VerifyRegionResults(op, op.else_branch(), "else")))
    return failure();
  if (op.then_branch().front().getNumArguments() != 0)
    return op.emitOpError() << "then region cannot have any arguments";
  if (op.else_branch().front().getNumArguments() != 0)
    return op.emitOpError() << "else region cannot have any arguments";
  return success();
}

//===----------------------------------------------------------------------===//
// InvertOp
//===----------------------------------------------------------------------===//

void InvertOp::getCanonicalizationPatterns(OwningRewritePatternList &results,
                                           MLIRContext *context) {
  results.insert<InvertNested>(context);
}

//===----------------------------------------------------------------------===//
// InvertPermutationOp
//===----------------------------------------------------------------------===//

// Verifies that the input is 1D.
static LogicalResult Verify(InvertPermutationOp op) {
  auto x_type = op.x().getType().cast<TensorType>();
  if (!x_type.hasRank()) return success();
  if (x_type.getShape().size() != 1)
    return op.emitOpError() << "requires input x to be 1-dimensional";

  return success();
}

//===----------------------------------------------------------------------===//
// LeakyReluOp
//===----------------------------------------------------------------------===//

OpFoldResult LeakyReluOp::fold(ArrayRef<Attribute> operands) {
  assert(operands.size() == 1 && "leaky relu has one operand");

  // leaky_relu(x, alpha: 1) -> x
  if (alpha().convertToFloat() == 1.0f) return getOperand();

  auto calculate = [&](FloatAttr arg) {
    APFloat val = arg.getValue();
    if (val.isNegative()) val = alpha() * val;
    return FloatAttr::get(arg.getType(), val);
  };

  if (auto arg = operands[0].dyn_cast_or_null<FloatAttr>()) {
    return calculate(arg);
  } else if (auto arg = operands[0].dyn_cast_or_null<SplatElementsAttr>()) {
    if (auto elementAttr = arg.getSplatValue().dyn_cast<FloatAttr>())
      return DenseElementsAttr::get(arg.getType(), calculate(elementAttr));
  }
  return {};
}

//===----------------------------------------------------------------------===//
// LogOp
//===----------------------------------------------------------------------===//

void LogOp::getCanonicalizationPatterns(OwningRewritePatternList &results,
                                        MLIRContext *context) {
  results.insert<LogOfSoftmax>(context);
}

//===----------------------------------------------------------------------===//
// ReadVariableOp
//===----------------------------------------------------------------------===//

void ReadVariableOp::getCanonicalizationPatterns(
    OwningRewritePatternList &results, MLIRContext *context) {
  results.insert<ReadVariableOfCast>(context);
}

//===----------------------------------------------------------------------===//
// LogicalNotOp
//===----------------------------------------------------------------------===//

void LogicalNotOp::getCanonicalizationPatterns(
    OwningRewritePatternList &results, MLIRContext *context) {
  results.insert<LogicalNotNested, LogicalNotOfEqual, LogicalNotOfNotEqual,
                 LogicalNotOfGreater, LogicalNotOfGreaterEqual,
                 LogicalNotOfLess, LogicalNotOfLessEqual>(context);
}

//===----------------------------------------------------------------------===//
// MatrixBandPartOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(MatrixBandPartOp op) {
  if (!HasRankAtLeast(op.input(), 2)) {
    return op.emitOpError()
           << "requires `input` to have rank of at least 2, but found "
           << op.input().getType();
  }
  if (!IsOfRankOrUnranked(op.num_lower(), 0)) {
    return op.emitOpError()
           << "requires `num_lower` to have 0 dimensions, but found "
           << op.num_lower().getType();
  }
  if (!IsOfRankOrUnranked(op.num_upper(), 0)) {
    return op.emitOpError()
           << "requires `num_upper` to have 0 dimensions, but found "
           << op.num_upper().getType();
  }
  return success();
}

//===----------------------------------------------------------------------===//
// MaxOp
//===----------------------------------------------------------------------===//

void MaxOp::build(OpBuilder &builder, OperationState &result, Value input,
                  Value reduction_indices, BoolAttr keep_dims) {
  Type out_ty =
      InferReductionOpType(input, reduction_indices, keep_dims, &builder);
  build(builder, result, out_ty, input, reduction_indices, keep_dims);
}

//===----------------------------------------------------------------------===//
// MaxPoolOp
//===----------------------------------------------------------------------===//

LogicalResult MaxPoolOp::FoldOperandsPermutation(
    ArrayRef<int64_t> permutation) {
  return ::mlir::TF::FoldOperandsPermutation(
      permutation, this, {{"strides", strides()}, {"ksize", ksize()}});
}

//===----------------------------------------------------------------------===//
// MaxPoolGradOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(MaxPoolGradOp op) {
  if (!IsOfRankOrUnranked(op.orig_input(), 4)) {
    return op.emitOpError() << "requires orig_input to be rank 4";
  }
  if (!IsOfRankOrUnranked(op.orig_output(), 4)) {
    return op.emitOpError() << "requires orig_output to be rank 4";
  }
  if (!IsOfRankOrUnranked(op.grad(), 4)) {
    return op.emitOpError() << "requires grad to be rank 4";
  }
  return success();
}

//===----------------------------------------------------------------------===//
// MeanOp
//===----------------------------------------------------------------------===//

LogicalResult MeanOp::FoldOperandsPermutation(ArrayRef<int64_t> permutation) {
  // Reduction indices must be defined by a constant operation.
  auto reduction_op =
      dyn_cast_or_null<TF::ConstOp>(reduction_indices().getDefiningOp());
  if (!reduction_op) return failure();

  auto reductions_value = reduction_op.value().dyn_cast<DenseElementsAttr>();
  if (!reductions_value) return failure();

  // Prepare new reduction indices according to operand permutation.
  SmallVector<int32_t, 4> shuffled_reduction;
  llvm::transform(reductions_value.getIntValues(),
                  std::back_inserter(shuffled_reduction),
                  [&](APInt idx) { return permutation[idx.getSExtValue()]; });

  // Add constant operation with a new reduction indices.
  OpBuilder builder(getOperation());
  auto type = mlir::RankedTensorType::get(shuffled_reduction.size(),
                                          builder.getIntegerType(32));
  auto values = mlir::DenseIntElementsAttr::get(type, shuffled_reduction);
  auto shuffled_reduction_op = builder.create<TF::ConstOp>(getLoc(), values);

  // Use new reduction indices.
  setOperand(1, shuffled_reduction_op);

  return success();
}

//===----------------------------------------------------------------------===//
// MulOp
//===----------------------------------------------------------------------===//

OpFoldResult MulOp::fold(ArrayRef<Attribute> operands) {
  return IdentityArithmeticOpFolder<MulOp>(*this, operands);
}

//===----------------------------------------------------------------------===//
// NegOp
//===----------------------------------------------------------------------===//

void NegOp::getCanonicalizationPatterns(OwningRewritePatternList &results,
                                        MLIRContext *context) {
  results.insert<NegNested>(context);
}

//===----------------------------------------------------------------------===//
// NotEqualOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(NotEqualOp op) {
  // If we allow inputs to have incompatible type, then nothing to do.
  if (!op.incompatible_shape_error()) return success();

  // Otherwise, check inputs are broadcastable.
  return mlir::OpTrait::impl::verifyCompatibleOperandBroadcast(
      op.getOperation());
}

void NotEqualOp::build(OpBuilder &builder, OperationState &result, Value x,
                       Value y, BoolAttr incompatible_shape_error) {
  auto result_type = DeduceEqualCmpOpType(&builder, result.location, x, y,
                                          incompatible_shape_error);
  return build(builder, result, result_type, x, y, incompatible_shape_error);
}

//===----------------------------------------------------------------------===//
// OneHotOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(OneHotOp op) {
  int64_t axis = op.axis().getSExtValue();

  auto indices_ty = op.indices().getType().dyn_cast<RankedTensorType>();
  if (indices_ty &&
      !(axis == -1 || (axis >= 0 && axis <= indices_ty.getShape().size()))) {
    return op.emitOpError()
           << "expected axis (" << axis << ") to be -1 or between [0, "
           << indices_ty.getShape().size() << "]";
  }

  if (axis < -1) {
    return op.emitOpError() << "expected axis (" << axis
                            << ") to be -1 or between [0, rank(indices()))";
  }

  if (!IsOfRankOrUnranked(op.depth(), 0)) {
    return op.emitOpError() << "requires depth to be a scalar";
  }
  if (!IsOfRankOrUnranked(op.on_value(), 0)) {
    return op.emitOpError() << "requires on_value to be a scalar";
  }
  if (!IsOfRankOrUnranked(op.off_value(), 0)) {
    return op.emitOpError() << "requires off_value to be a scalar";
  }

  DenseIntElementsAttr depth_attr;
  if (matchPattern(op.depth(), m_Constant(&depth_attr))) {
    if (depth_attr.getType().getRank() != 0)
      return op.emitOpError() << "requires depth to be a scalar";
    int64_t depth = depth_attr.getValue<APInt>({}).getSExtValue();
    if (depth < 0) {
      return op.emitOpError() << "depth must be non-negative, got: " << depth;
    }
  }

  return success();
}

static TensorType InferOneHotOpType(Value indices, Value depth, Value on_value,
                                    Value off_value, IntegerAttr axis) {
  int64_t axis_val = axis.getInt();
  Type element_ty = on_value.getType().cast<TensorType>().getElementType();
  auto unranked_ty = UnrankedTensorType::get(element_ty);
  if (axis_val < -1) return unranked_ty;

  auto indices_ty = indices.getType().dyn_cast<RankedTensorType>();
  if (!indices_ty) return unranked_ty;

  auto shape = llvm::to_vector<2>(indices_ty.getShape());
  if (axis_val == -1) axis_val = shape.size();

  int64_t depth_val = ShapedType::kDynamicSize;
  DenseIntElementsAttr depth_attr;
  if (matchPattern(depth, m_Constant(&depth_attr)) &&
      depth_attr.getNumElements() == 1)
    depth_val = (*depth_attr.begin()).getSExtValue();
  shape.insert(shape.begin() + axis_val, depth_val);
  return RankedTensorType::get(shape, element_ty);
}

void OneHotOp::build(OpBuilder &builder, OperationState &result, Value indices,
                     Value depth, Value on_value, Value off_value,
                     IntegerAttr axis) {
  build(builder, result,
        InferOneHotOpType(indices, depth, on_value, off_value, axis), indices,
        depth, on_value, off_value, axis);
}

//===----------------------------------------------------------------------===//
// PackOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(PackOp op) {
  // TODO(hinsu): Convert variadic length attributes to derived attributes.
  Operation::operand_range values = op.values();

  if (failed(VerifyTypesCompatibility(values,
                                      /*mask_one_dim=*/false,
                                      op.getOperation()))) {
    return failure();
  }

  int64_t inputs_rank = -1;
  for (Value value : values) {
    if (auto ty = value.getType().dyn_cast<RankedTensorType>()) {
      // Exit early as input types are verified to be compatible so all ranked
      // tensors have the same rank.
      inputs_rank = ty.getRank();
      break;
    }
  }
  if (inputs_rank == -1) return success();

  // The values can be packed along any of the dimensions between 0 and
  // inputs rank, inclusive. Also, as the negative axis values wrap around so
  // the axis value range is [-(R+1), R+1).
  int64_t range_begin = -inputs_rank - 1;  // Inclusive
  int64_t range_end = inputs_rank + 1;     // Exclusive
  int64_t axis = op.axis().getSExtValue();
  if (axis < range_begin || axis >= range_end) {
    return op.emitError() << "attribute 'axis' should be within range ["
                          << range_begin << ", " << range_end
                          << "); actual value: " << axis;
  }

  return success();
}

//===----------------------------------------------------------------------===//
// PadOp
//===----------------------------------------------------------------------===//

LogicalResult PadOp::FoldOperandsPermutation(ArrayRef<int64_t> permutation) {
  // Paddings must be defined by a constant operation.
  auto paddings_op = dyn_cast_or_null<TF::ConstOp>(paddings().getDefiningOp());
  if (!paddings_op) return failure();

  auto paddings_value = paddings_op.value().dyn_cast<DenseElementsAttr>();
  if (!paddings_value ||
      paddings_value.getNumElements() != permutation.size() * 2)
    return failure();

  SmallVector<int32_t, 8> shuffled_paddings(paddings_value.getNumElements());
  for (auto index_pair : llvm::enumerate(paddings_value.getIntValues())) {
    size_t outer_idx = index_pair.index() / 2;
    size_t inner_idx = index_pair.index() % 2;

    shuffled_paddings[permutation[outer_idx] * 2 + inner_idx] =
        index_pair.value().getSExtValue();
  }

  // Add constant operation with a new paddings.
  OpBuilder builder(getOperation());
  auto type = mlir::RankedTensorType::get(paddings_value.getType().getShape(),
                                          builder.getIntegerType(32));
  auto values = mlir::DenseIntElementsAttr::get(type, shuffled_paddings);
  auto shuffled_paddings_op = builder.create<TF::ConstOp>(getLoc(), values);

  // Use new paddings.
  setOperand(1, shuffled_paddings_op);

  // Change the result type.
  getResult().setType(ShuffleRankedTensorType(getResult().getType(),
                                              ReversePermutation(permutation)));

  return success();
}

//===----------------------------------------------------------------------===//
// ParseExampleV2Op
//===----------------------------------------------------------------------===//

static LogicalResult Verify(ParseExampleV2Op op) {
  // NOTE(mrry): This validates properties of an op that would previously be
  // validated by the TensorFlow OpDef type checker. In addition to these
  // checks, the shape inference function for ParseExampleV2 validates the
  // consistency of the argument and result types.

  // Validate dense variadic input and output lengths.
  // NOTE(mrry): The Tdense attr is derived from dense_defaults, so we
  // do not need to validate dense_defaults.
  auto dense_types_count =
      std::distance(op.Tdense().begin(), op.Tdense().end());
  auto dense_values_count =
      std::distance(op.dense_values().begin(), op.dense_values().end());
  if (dense_values_count != dense_types_count) {
    return op.emitError() << "output 'dense_values' should have same length "
                          << "as attribute 'Tdense'";
  }

  // Validate sparse variadic output lengths.
  // NOTE(mrry): The sparse_types attr is derived from sparse_values, so we
  // do not need to validate sparse_values.
  auto sparse_types_count =
      std::distance(op.sparse_types().begin(), op.sparse_types().end());
  if (op.num_sparse() != sparse_types_count) {
    return op.emitError() << "attribute 'num_sparse' should be the same as "
                          << "the length of attribute 'sparse_types'";
  }
  if (op.sparse_indices().size() != sparse_types_count) {
    return op.emitError() << "output 'sparse_indices' should have same length "
                          << "as attribute 'sparse_types'";
  }
  if (op.sparse_shapes().size() != sparse_types_count) {
    return op.emitError() << "output 'sparse_shapes' should have same length "
                          << "as attribute 'sparse_types'";
  }

  // Validate ragged variadic output lengths.
  auto ragged_value_types_count = std::distance(op.ragged_value_types().begin(),
                                                op.ragged_value_types().end());
  auto ragged_split_types_count = std::distance(op.ragged_split_types().begin(),
                                                op.ragged_split_types().end());
  if (ragged_value_types_count != ragged_split_types_count) {
    return op.emitError() << "attribute 'ragged_value_types' should have same "
                          << "length as attribute 'ragged_split_types'";
  }

  return success();
}

//===----------------------------------------------------------------------===//
// PartitionedCallOp
//===----------------------------------------------------------------------===//

template <class OpClass>
static LogicalResult VerifyPartitionedCall(OpClass op) {
  auto module = op.template getParentOfType<ModuleOp>();
  SymbolRefAttr func = op.getAttr("f").template cast<SymbolRefAttr>();

  auto function =
      dyn_cast_or_null<FuncOp>(SymbolTable::lookupSymbolIn(module, func));

  if (!function) {
    return op.emitError("'f' attribute refers to an undefined function: ")
           << func;
  }

  FunctionType function_ty = function.getType();
  int func_arg_count = function_ty.getNumInputs();
  int arg_count = op.args().size();

  if (arg_count != func_arg_count) {
    return op.emitError() << "argument count mismatch: 'args' has " << arg_count
                          << " arguments, but '" << func << "' expects "
                          << func_arg_count;
  }

  return success();
}

//===----------------------------------------------------------------------===//
// PowOp
//===----------------------------------------------------------------------===//

OpFoldResult PowOp::fold(ArrayRef<Attribute> operands) {
  auto constant_y = operands[1].dyn_cast_or_null<DenseFPElementsAttr>();
  if (constant_y && constant_y.isSplat()) {
    APFloat y_value = constant_y.getSplatValue<APFloat>();
    auto output_type = getType().cast<ShapedType>();
    if (y_value.isZero() && output_type.hasStaticShape()) {
      return DenseElementsAttr::get(
          output_type,
          FloatAttr::get(output_type.getElementType(), /*value=*/1.0));
    }
    if (y_value.isExactlyValue(1.0)) {
      return x();
    }
  }
  return {};
}

//===----------------------------------------------------------------------===//
// QrOp
//===----------------------------------------------------------------------===//

// Verifies that,
//
// * Input type, if ranked, must have at least 2 dimensions and at most
//   INT32_MAX dimensions.
//
static LogicalResult Verify(QrOp op) {
  auto ttype = op.input().getType().cast<TensorType>();
  if (!ttype.hasRank()) return success();
  if (!HasRankAtLeast(op.input(), 2))
    return op.emitOpError(
        "requires ranked input tensor to be of rank 2 or more");
  if (!HasRankAtMost(op.input(), std::numeric_limits<int32_t>::max()))
    return op.emitOpError(
        "requires ranked input tensor to be of rank INT32_MAX or less");

  return success();
}

//===----------------------------------------------------------------------===//
// ReciprocalOp
//===----------------------------------------------------------------------===//

void ReciprocalOp::getCanonicalizationPatterns(
    OwningRewritePatternList &results, MLIRContext *context) {
  results.insert<ReciprocalNested>(context);
}

//===----------------------------------------------------------------------===//
// RandomUniformOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(RandomUniformOp op) {
  if (!IsOfRankOrUnranked(op.shape(), 1))
    return op.emitOpError("shape must be 1D tensor");
  return success();
}

//===----------------------------------------------------------------------===//
// RangeOp
//===----------------------------------------------------------------------===//

void RangeOp::build(OpBuilder &builder, OperationState &result, Value start,
                    Value limit, Value delta) {
  assert(start.getType() == limit.getType());
  assert(start.getType() == delta.getType());
  DenseIntElementsAttr start_val;
  DenseIntElementsAttr limit_val;
  DenseIntElementsAttr delta_val;
  if (matchPattern(start, m_Constant(&start_val)) &&
      matchPattern(limit, m_Constant(&limit_val)) &&
      matchPattern(delta, m_Constant(&delta_val))) {
    auto size = llvm::APIntOps::RoundingSDiv(
        *limit_val.begin() - *start_val.begin(), *delta_val.begin(),
        llvm::APInt::Rounding::DOWN);
    return RangeOp::build(
        builder, result,
        RankedTensorType::get(
            size.getSExtValue(),
            start.getType().cast<TensorType>().getElementType()),
        start, limit, delta);
  }
  return RangeOp::build(
      builder, result,
      RankedTensorType::get(
          {-1}, start.getType().cast<TensorType>().getElementType()),
      start, limit, delta);
}
//===----------------------------------------------------------------------===//
// RankOp
//===----------------------------------------------------------------------===//

void RankOp::build(OpBuilder &builder, OperationState &result, Value input) {
  return RankOp::build(builder, result,
                       RankedTensorType::get({}, builder.getIntegerType(32)),
                       input);
}

// This will create a constant value for RankOp of a ranked tensor.
OpFoldResult RankOp::fold(ArrayRef<Attribute> operands) {
  auto type = input().getType();
  auto ranked_type = type.dyn_cast<RankedTensorType>();
  if (!ranked_type) return {};

  auto output_type = getType().cast<ShapedType>();
  int32_t rank = ranked_type.getRank();
  return DenseIntElementsAttr::get(output_type, rank);
}

//===----------------------------------------------------------------------===//
// RealDivOp
//===----------------------------------------------------------------------===//

void RealDivOp::getCanonicalizationPatterns(OwningRewritePatternList &results,
                                            MLIRContext *context) {
  results.insert<RealDivWithSqrtDivisor>(context);
}

OpFoldResult RealDivOp::fold(ArrayRef<Attribute> operands) {
  return IdentityArithmeticOpFolder<RealDivOp>(*this, operands);
}

//===----------------------------------------------------------------------===//
// ReshapeOp
//===----------------------------------------------------------------------===//

// TODO(b/128020684): Verify the output type.
static LogicalResult Verify(ReshapeOp op) {
  auto shape_type = op.shape().getType().cast<TensorType>();
  if (!shape_type.hasRank()) return success();
  if (shape_type.getRank() != 1)
    return op.emitOpError("shape must be 1D tensor");
  auto rank_by_shape = shape_type.getShape()[0];
  auto type_of_tensor = op.tensor().getType().cast<TensorType>();
  // No compile time verification for unknown sized shape.
  if (rank_by_shape == -1 || !type_of_tensor.hasStaticShape()) return success();
  int64_t num_by_tensor = type_of_tensor.getNumElements();

  auto out_ty = op.getType().dyn_cast<RankedTensorType>();
  if (out_ty && out_ty.hasStaticShape()) {
    int64_t num_output_elements = out_ty.getNumElements();
    if (num_by_tensor != num_output_elements)
      return op.emitOpError()
             << "number of output elements (" << num_output_elements
             << ") does not match expected number of elements ("
             << num_by_tensor << ")";
  }

  // Check values if constant shape. No compiling time verification for
  // non-constant shape.
  auto *shape_op = op.shape().getDefiningOp();
  if (!shape_op) return success();
  Attribute shape_cst;
  if (!matchPattern(shape_op, m_Constant(&shape_cst))) return success();
  auto shape_cst_attr = shape_cst.dyn_cast<ElementsAttr>();
  if (!shape_cst_attr) return op.emitOpError("shape must be a valid tensor");

  if (auto opaque_attr = shape_cst_attr.dyn_cast<OpaqueElementsAttr>()) {
    opaque_attr.decode(shape_cst_attr);
  }

  // We know the shape is a 1-D Tensor, then let us get the number of
  // elements it implies.
  unsigned num_by_shape = 1;
  unsigned unknown_dim_count = 0;
  for (int i = 0, e = rank_by_shape; i != e; ++i) {
    auto num = shape_cst_attr.getValue<IntegerAttr>(i).getInt();
    // The dimension size value can be -1, and that the real size needs to
    // be computed so that the total size remains constant. At most one
    // component of shape can be -1.
    if (num == -1) {
      if (++unknown_dim_count > 1) {
        return op.emitOpError("more than one component of shape are -1");
      }
    } else {
      num_by_shape *= num;
    }
  }
  // If there is one component of shape is -1, the dimension should be
  // computed so that the total size remains constant.
  if (unknown_dim_count == 1) {
    if (num_by_tensor % num_by_shape != 0)
      return op.emitOpError(
          "one component of shape is -1 but couldn't infer the dimension");
    return success();
  }
  // If the elements by the tensor and implies by the shape don't match,
  // fail this static check.
  if (num_by_tensor != num_by_shape) {
    return op.emitOpError(
        "mismatch in tensor elements and shape implied elements");
  }
  return success();
}

void ReshapeOp::build(OpBuilder &builder, OperationState &result, Value tensor,
                      Value shape) {
  auto ttype = tensor.getType().cast<ShapedType>();
  auto etype = ttype.getElementType();

  auto unranked = [&builder, etype, &result, shape, tensor]() {
    return ReshapeOp::build(builder, result, UnrankedTensorType::get(etype),
                            tensor, shape);
  };

  // If tensor is unranked then we have no info about output of shape.
  if (!ttype.hasRank()) return unranked();

  DenseIntElementsAttr attr_shape;
  if (matchPattern(shape, m_Constant(&attr_shape))) {
    llvm::SmallVector<int64_t, 4> const_shape;
    const_shape.reserve(attr_shape.getNumElements());

    // Detect if reshape output shape is folded.
    bool flatten = false;
    int unknown_index = -1;
    // The product of constant shape argument excluding unknown dimension.
    int64_t product_cshape = 1;
    for (auto e : llvm::enumerate(attr_shape)) {
      int64_t val = e.value().getSExtValue();
      if (IsUnknownDimOrRank(val)) {
        if (flatten) {
          mlir::emitError(result.location)
              << "only one unknown dimension allowed";
          return;
        }
        flatten = true;
        unknown_index = e.index();
      } else {
        product_cshape *= val;
      }
      const_shape.push_back(val);
    }

    // Compute the value of the unknown dimension.
    if (flatten) {
      // Compute number of elements in tensor shape.
      auto tshape = ttype.getShape();
      int64_t product_tshape = std::accumulate(tshape.begin(), tshape.end(), 1,
                                               std::multiplies<int64_t>());
      // Set the unknown dimension such that total number of elements remain
      // constant.
      // Note: The case where the ratio is not integral, and so the total size
      // of reshape not constant, is checked in verify function.
      const_shape[unknown_index] = product_tshape / product_cshape;
    }
    return ReshapeOp::build(builder, result,
                            RankedTensorType::get(const_shape, etype), tensor,
                            shape);
  }
  return unranked();
}

//===----------------------------------------------------------------------===//
// SelectOp
//===----------------------------------------------------------------------===//

void SelectOp::getCanonicalizationPatterns(OwningRewritePatternList &results,
                                           MLIRContext *context) {
  results.insert<SelectToSelectV2>(context);
}

// Verifies a few extra requirements on SelectOp:
// (1) `then` and `else` must have same shape
// (2) At least one of the following must be true:
//     (a) `cond` has the same rank as `then` and `else`
//     (b) `cond` is a scalar
//     (c) `cond` is a vector AND `then` and `else` are non-scalar with their
//         first dimension equal to `cond`.
static LogicalResult Verify(SelectOp op) {
  auto then_tensor = op.t().getType().cast<TensorType>();
  auto else_tensor = op.e().getType().cast<TensorType>();
  // Check (1).
  if (!AreCastCompatible({then_tensor, else_tensor}))
    return op.emitOpError() << "requires t and e have compatible shapes";

  // Get data rank (if exists).
  int data_rank;
  // If data is unranked or data_rank is 0, this will remain -2. Otherwise
  // refers to first dimension of then and/or else.
  int data_first_dim = -2;
  bool then_has_rank = then_tensor.hasRank();
  bool else_has_rank = else_tensor.hasRank();
  if (then_has_rank && else_has_rank) {
    data_rank = then_tensor.getRank();
    if (then_tensor.getRank() > 0)
      data_first_dim = then_tensor.getShape().front();
    if (else_tensor.getRank() > 0)
      data_first_dim = std::max(
          static_cast<int>(else_tensor.getShape().front()), data_first_dim);
  } else if (then_has_rank) {
    data_rank = then_tensor.getRank();
    if (then_tensor.getRank() > 0)
      data_first_dim = then_tensor.getShape().front();
  } else if (else_has_rank) {
    data_rank = else_tensor.getRank();
    if (else_tensor.getRank() > 0)
      data_first_dim = else_tensor.getShape().front();
  } else {
    // Neither has a rank.
    return success();
  }

  auto cond_tensor = op.condition().getType().dyn_cast<RankedTensorType>();
  if (!cond_tensor) return success();
  auto cond_rank = cond_tensor.getRank();
  // Check (2a) and (2b).
  if (cond_rank == 0 || cond_rank == data_rank) return success();
  // Check (2c).
  if (cond_rank == 1) {
    auto cond_shape = cond_tensor.getShape().front();
    if (data_rank == 0) {
      return op.emitOpError()
             << "requires that t and e are nonscalar when pred is a vector";
    }
    // We know `data` tensor has a rank of at least 1.
    if (data_first_dim != -1 && cond_shape != -1 &&
        data_first_dim != cond_shape) {
      return op.emitOpError() << "requires that, when pred is a vector, the "
                                 "shape matches the first dimension of t and e";
    }
    return success();
  }
  // None of (2a,b,c) were true; fail.
  return op.emitOpError() << "requires that pred is a scalar OR has the same "
                             "rank as t and e OR is a vector";
}

//===----------------------------------------------------------------------===//
// SelectV2Op
//===----------------------------------------------------------------------===//

static Type InferSelectV2OpType(Value condition, Value e, Value t) {
  Type element_ty = e.getType().cast<TensorType>().getElementType();
  auto unranked_ty = UnrankedTensorType::get(element_ty);

  Type broadcasted_ty =
      OpTrait::util::getBroadcastedType(e.getType(), t.getType());
  if (!broadcasted_ty) return unranked_ty;

  auto cond_ranked_ty = condition.getType().dyn_cast<RankedTensorType>();
  auto broadcasted_ranked_ty = broadcasted_ty.dyn_cast<RankedTensorType>();
  if (!cond_ranked_ty || !broadcasted_ranked_ty) return unranked_ty;

  // Explicitly get broadcasted output type as element types of condition may
  // not be same as the broadcated type's element type.
  SmallVector<int64_t, 4> result_shape;
  if (!OpTrait::util::getBroadcastedShape(cond_ranked_ty.getShape(),
                                          broadcasted_ranked_ty.getShape(),
                                          result_shape))
    return unranked_ty;
  return RankedTensorType::get(result_shape, element_ty);
}

void SelectV2Op::build(OpBuilder &builder, OperationState &result,
                       Value condition, Value e, Value t) {
  build(builder, result, InferSelectV2OpType(condition, e, t), condition, e, t);
}

//===----------------------------------------------------------------------===//
// ShapeOp
//===----------------------------------------------------------------------===//

namespace {
// Validates Shape/ShapeN/VariableShape operand and associated result types.
LogicalResult VerifyShapeOperandAndResult(Operation *op, Type operand_type,
                                          Type result_type,
                                          int variadic_idx = -1) {
  std::string variadic_idx_str =
      variadic_idx < 0 ? "" : llvm::formatv(" #{0}", variadic_idx).str();

  auto result_ranked_type = result_type.dyn_cast<RankedTensorType>();
  if (!result_ranked_type) return success();
  if (result_ranked_type.getShape().size() != 1)
    return op->emitOpError("requires 1D type for result") << variadic_idx_str;

  auto operand_ranked_type = operand_type.dyn_cast_or_null<RankedTensorType>();
  if (operand_ranked_type) {
    // The operand is a ranked tensor.
    if (result_ranked_type.hasStaticShape() &&
        !operand_ranked_type.getShape().empty() &&
        result_ranked_type.getDimSize(0) !=
            operand_ranked_type.getShape().size())
      return op->emitOpError("requires dimension size of result")
             << variadic_idx_str << " to match rank of operand"
             << variadic_idx_str;
  } else if (result_ranked_type.hasStaticShape()) {
    // The operand is an unranked tensor, print a warning if the result
    // is static.
    // Note: We do not handle this situation as an error, this would be too
    // restrictive due to incompleteness of shape inference at this point.
    op->emitWarning("has static shape result")
        << variadic_idx_str << " for unranked operand" << variadic_idx_str;
  }

  Type element_type = result_ranked_type.getElementType();
  if (!element_type.isSignlessInteger(32) &&
      !element_type.isSignlessInteger(64))
    return op->emitOpError("requires int32 or int64 return type for result")
           << variadic_idx_str;

  return success();
}
}  // anonymous namespace

static LogicalResult Verify(ShapeOp op) {
  return VerifyShapeOperandAndResult(op, op.input().getType(), op.getType());
}

// Converts shape of the given type to attribute if it is of ranked tensor type.
// Returned attribute has integer elements of the given width.
static Attribute ConvertShapeToAttr(Type input_ty, int out_width) {
  auto ranked_ty = input_ty.dyn_cast<RankedTensorType>();
  if (!ranked_ty || !ranked_ty.hasStaticShape()) return {};

  auto shape = ranked_ty.getShape();
  int rank = shape.size();

  SmallVector<APInt, 4> dimensions;
  dimensions.reserve(rank);
  for (int i = 0; i < rank; ++i)
    dimensions.push_back(APInt(out_width, shape[i]));

  auto result_type = RankedTensorType::get(
      {rank}, IntegerType::get(out_width, input_ty.getContext()));
  return DenseElementsAttr::get(result_type, dimensions);
}

OpFoldResult ShapeOp::fold(ArrayRef<Attribute> operands) {
  int width =
      getType().cast<ShapedType>().getElementType().getIntOrFloatBitWidth();
  return ConvertShapeToAttr(getOperand().getType(), width);
}

void ShapeOp::build(OpBuilder &builder, OperationState &result, Value input,
                    BoolAttr use32Bit) {
  auto rankedTensorType = input.getType().dyn_cast<RankedTensorType>();
  int64_t rank = rankedTensorType ? rankedTensorType.getRank() : -1;
  auto out_type = use32Bit.getValue() ? builder.getIntegerType(32)
                                      : builder.getIntegerType(64);
  return ShapeOp::build(builder, result,
                        RankedTensorType::get({rank}, out_type), input);
}

//===----------------------------------------------------------------------===//
// ShapeNOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(ShapeNOp op) {
  const size_t num_tensors = op.N();

  if (op.getNumOperands() != num_tensors)
    return op.emitOpError() << "requires " << num_tensors << " operand(s), got "
                            << op.getNumOperands() << " operand(s)";

  if (op.getNumResults() != num_tensors)
    return op.emitOpError() << "requires " << num_tensors << " result(s), got "
                            << op.getNumResults() << " result(s)";

  for (auto i : llvm::seq<uint64_t>(0, num_tensors)) {
    auto verification = VerifyShapeOperandAndResult(
        op, op.getOperand(i).getType(), op.getResult(i).getType(), i);
    if (failed(verification)) return verification;
  }

  return success();
}

LogicalResult ShapeNOp::fold(ArrayRef<Attribute> operands,
                             SmallVectorImpl<OpFoldResult> &results) {
  if (getNumOperands() == 0) return success();
  int width =
      getType(0).cast<ShapedType>().getElementType().getIntOrFloatBitWidth();

  for (Type input_ty : getOperandTypes()) {
    OpFoldResult result = ConvertShapeToAttr(input_ty, width);
    if (!result) return failure();

    results.push_back(result);
  }
  return success();
}

// TODO(hinsu): Add canonicalization pattern for ShapeN ops that don't have all
// static input shapes. Replacing output values corresponding to static input
// types may enable optimizations in users of the values.

//===----------------------------------------------------------------------===//
// SizeOp
//===----------------------------------------------------------------------===//

// Verifies that,
//
// * Input type, if is a ranked tensor, has at most INT32_MAX dimensions.
//
static LogicalResult Verify(SizeOp op) {
  if (!HasRankAtMost(op.input(), std::numeric_limits<int32_t>::max()))
    return op.emitOpError(
        "requires ranked input tensor to be of rank INT32_MAX or less");

  return success();
}

//===----------------------------------------------------------------------===//
// SliceOp
//===----------------------------------------------------------------------===//

// Verifies that:
//
// - operands begin and size are 1D with the same number of elements.
// - if the input is a ranked tensor, the rank of the input equals the number
//   of elements in operands begin and size.
// - if begin are constants, that
//   0 <= begin[i] <= begin[i] + size[i] <= input_ty.getShape()[i]
// - if begins aren't constant but the input is a ranked tensor, that
//   size[i] <= input_ty.getShape()[i]
//
static LogicalResult Verify(SliceOp op) {
  RankedTensorType begin_ty = GetRankedTensorTypeForOperand(op.begin());
  if (begin_ty && begin_ty.getRank() != 1) {
    return op.emitOpError() << "requires begin operand to be 1D tensor";
  }

  RankedTensorType size_ty = GetRankedTensorTypeForOperand(op.size());
  if (size_ty && size_ty.getRank() != 1) {
    return op.emitOpError() << "requires size operand to be 1D tensor";
  }

  if (!begin_ty || !size_ty || !begin_ty.hasStaticShape() ||
      !size_ty.hasStaticShape())
    return success();

  if (begin_ty.getNumElements() != size_ty.getNumElements()) {
    return op.emitOpError() << "requires begin and size operands to have the"
                               " same number of elements";
  }

  auto input_ty = op.input().getType().dyn_cast<RankedTensorType>();
  if (input_ty && begin_ty.getNumElements() != input_ty.getRank()) {
    return op.emitOpError() << "requires number of elements in begin and size"
                               "are equal to input rank";
  }

  DenseIntElementsAttr begin_indices;
  if (matchPattern(op.begin(), m_Constant(&begin_indices))) {
    DenseIntElementsAttr slice_sizes;
    bool constant_slice_sizes =
        matchPattern(op.size(), m_Constant(&slice_sizes));
    int dim = 0;
    for (const APInt &raw_begin_index : begin_indices.getValues<APInt>()) {
      int64_t begin_index = raw_begin_index.getSExtValue();
      int64_t input_size = input_ty ? input_ty.getShape()[dim] : -1;
      int64_t slice_size = constant_slice_sizes
                               ? slice_sizes.getValue<APInt>(dim).getSExtValue()
                               : 0;
      if (slice_size == -1 && input_size != -1) {
        slice_size = input_size - begin_index;
      }
      if (begin_index < 0 ||
          (input_size != -1 && begin_index + slice_size > input_size)) {
        return op.emitOpError()
               << "requires 0 <= begin[i] <= begin[i] + size[i] <= Di";
      }
      ++dim;
    }
  } else if (input_ty) {
    // If the inputs are ranked, we can do a few more sanity checks.
    DenseIntElementsAttr slice_sizes;
    if (matchPattern(op.size(), m_Constant(&slice_sizes))) {
      auto input_shape = input_ty.getShape();
      for (int64_t i = 0; i < input_ty.getRank(); ++i) {
        int64_t slice_size = slice_sizes.getValue<IntegerAttr>(i).getInt();
        int64_t input_size = input_shape[i];
        if (slice_size != -1 && input_size != -1 && slice_size > input_size) {
          return op.emitOpError() << "requires size[i] <= Di, even if begin[i] "
                                     "is unknown at compile time";
        }
      }
    }
  }

  return success();
}

//===----------------------------------------------------------------------===//
// SoftmaxOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(SoftmaxOp op) {
  if (!HasRankAtLeast(op.logits(), 1)) {
    return op.emitOpError("requires operand to have rank at least 1");
  }
  return success();
}

//===----------------------------------------------------------------------===//
// SoftmaxCrossEntropyWithLogitsOp
//===----------------------------------------------------------------------===//

// Verifies that,
//
// * Input types are broadcast compatible and the broadcasted type has rank two.
//
static LogicalResult Verify(SoftmaxCrossEntropyWithLogitsOp op) {
  auto broadcasted_ty = OpTrait::util::getBroadcastedType(
                            op.features().getType(), op.labels().getType())
                            .dyn_cast_or_null<ShapedType>();
  if (!broadcasted_ty ||
      (broadcasted_ty.hasRank() && broadcasted_ty.getRank() != 2))
    return op.emitOpError(
        "requires features and labels to be broadcast compatible to rank two");

  return success();
}

//===----------------------------------------------------------------------===//
// SparseSoftmaxCrossEntropyWithLogitsOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(SparseSoftmaxCrossEntropyWithLogitsOp op) {
  if (!IsOfRankOrUnranked(op.features(), 2)) {
    return op.emitOpError("requires features operand of rank two");
  }
  if (!IsOfRankOrUnranked(op.labels(), 1)) {
    return op.emitOpError("requires labels operand of rank one");
  }
  auto features_ty = op.features().getType().dyn_cast<RankedTensorType>();
  auto labels_ty = op.labels().getType().dyn_cast<RankedTensorType>();
  if (features_ty && labels_ty) {
    int64_t features_batches = features_ty.getDimSize(0);
    int64_t labels_batches = labels_ty.getDimSize(0);
    if (!ShapedType::isDynamic(features_batches) &&
        !ShapedType::isDynamic(labels_batches) &&
        features_batches != labels_batches)
      return op.emitOpError(
          "requires features and labels with matching first dimension");
  }
  return success();
}

//===----------------------------------------------------------------------===//
// SplitOp
//===----------------------------------------------------------------------===//

// Verifies the input and split dimension operands for tf.Split/tf.SplitV.
// Writes the split dimension's index (adjusted with input rank) via `dim_index`
// if it's a constant.
template <class Op>
LogicalResult VerifySplitInputAndSplitDim(Op op, Optional<int64_t> *dim_index) {
  *dim_index = llvm::None;

  Value split_dim = op.split_dim();
  if (auto split_dim_type = split_dim.getType().dyn_cast<RankedTensorType>())
    if (split_dim_type.getRank() != 0)
      return op.emitOpError(
          "split dimension should be an integer scalar tensor");

  // We can perform further verification if the input tensor to be split has
  // known rank and the split dimension tensor is a constant.

  auto input_type = op.value().getType().template dyn_cast<RankedTensorType>();
  if (!input_type) return success();

  int64_t input_rank = input_type.getRank();
  if (input_rank == 0)
    return op.emitOpError("cannot split scalar input tensor");

  DenseIntElementsAttr split_dim_attr;
  if (!matchPattern(split_dim, m_Constant(&split_dim_attr))) return success();

  int64_t index = (*split_dim_attr.begin()).getSExtValue();

  if (index + input_rank < 0 || index >= input_rank) {
    return op.emitOpError("split dimension must be in range [-")
           << input_rank << ", " << input_rank << ")";
  }

  if (index < 0) index += input_rank;
  *dim_index = index;

  return success();
}

static LogicalResult Verify(SplitOp op) {
  Optional<int64_t> dim_index;
  if (failed(VerifySplitInputAndSplitDim(op, &dim_index))) return failure();
  if (!dim_index) return success();

  int64_t input_dim_size =
      op.value().getType().cast<RankedTensorType>().getDimSize(*dim_index);
  if (input_dim_size == ShapedType::kDynamicSize) return success();

  if (input_dim_size % op.getNumResults() != 0)
    return op.emitOpError("dimension #")
           << *dim_index << " not divisible by the number of result tensors";

  return success();
}

//===----------------------------------------------------------------------===//
// SplitVOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(SplitVOp op) {
  auto split_sizes_type =
      op.size_splits().getType().dyn_cast<RankedTensorType>();
  if (!split_sizes_type) return success();

  if (split_sizes_type.getRank() != 1 ||
      split_sizes_type.getDimSize(0) != op.getNumResults())
    return op.emitOpError("split sizes should be a 1D tensor of ")
           << op.getNumResults() << " elements";

  Optional<int64_t> dim_index = 0;
  if (failed(VerifySplitInputAndSplitDim(op, &dim_index))) return failure();
  if (!dim_index) return success();

  int64_t input_dim_size =
      op.value().getType().cast<RankedTensorType>().getDimSize(*dim_index);
  if (input_dim_size == ShapedType::kDynamicSize) return success();

  // If split sizes come from a constant, they must sum to the dimension size
  // along split_dim, and we can have no more than one dynamic dimension.
  DenseIntElementsAttr split_sizes_attr;
  if (!matchPattern(op.size_splits(), m_Constant(&split_sizes_attr)))
    return success();

  int64_t total_dim_size = 0;  // Total dimension size assigned to splits
  llvm::Optional<int> dynamic_dim_index;

  SmallVector<int64_t, 4> split_sizes;
  split_sizes.reserve(
      split_sizes_attr.getType().cast<ShapedType>().getNumElements());

  for (auto dim : llvm::enumerate(split_sizes_attr)) {
    int64_t dim_val = dim.value().getSExtValue();
    split_sizes.push_back(dim_val);
    if (dim_val == ShapedType::kDynamicSize) {
      // We cannot have more than one dynamic dimension.
      if (dynamic_dim_index)
        return op.emitOpError(
            "cannot have more than one dynamic dimension in split sizes");
      dynamic_dim_index = dim.index();
    } else {
      total_dim_size += dim_val;
    }
  }

  if (!dynamic_dim_index && total_dim_size != input_dim_size)
    return op.emitOpError(
               "split sizes must sum up to the dimension size along split "
               "dimension, found ")
           << total_dim_size << " vs " << input_dim_size;

  if (dynamic_dim_index && total_dim_size > input_dim_size)
    return op.emitOpError(
               "split sizes must sum up to be less than or equal to the "
               "dimension size along split dimension, found ")
           << total_dim_size << " vs " << input_dim_size;

  return success();
}

//===----------------------------------------------------------------------===//
// SquareOp
//===----------------------------------------------------------------------===//

void SquareOp::getCanonicalizationPatterns(OwningRewritePatternList &results,
                                           MLIRContext *context) {
  results.insert<SquareOfSub>(context);
}

//===----------------------------------------------------------------------===//
// SubOp
//===----------------------------------------------------------------------===//

void SubOp::getCanonicalizationPatterns(OwningRewritePatternList &results,
                                        MLIRContext *context) {
  results.insert<SubOfNeg>(context);
}

OpFoldResult SubOp::fold(ArrayRef<Attribute> operands) {
  return IdentityArithmeticOpFolder<SubOp>(*this, operands);
}

//===----------------------------------------------------------------------===//
// SumOp
//===----------------------------------------------------------------------===//

void SumOp::build(OpBuilder &builder, OperationState &result, Value input,
                  Value reduction_indices, BoolAttr keep_dims) {
  Type out_ty =
      InferReductionOpType(input, reduction_indices, keep_dims, &builder);
  build(builder, result, out_ty, input, reduction_indices, keep_dims);
}

//===----------------------------------------------------------------------===//
// StridedSliceOp
//===----------------------------------------------------------------------===//

// TODO(b/154160827): Add a canonicalization pattern from tf.StridedSliceOp to
// tf.SliceOp if both of the following are true:
// - All strides have a known value equal to 1
// - No masks are set (or masks can be applied by transforming the inputs to
//   Slice)

// Verifies that,
//
// - begin, end and strides operands are 1D and they have the same number of
//   elements. Here, the number of elements should be less than 32 to support
//   32-bit mask attributes.
// - None of the strides values are zero.
// - Ellipsis mask can have at most one bit set.

template <class OpTy>
static LogicalResult VerifyStridedSliceBase(OpTy op) {
  // Expected size for operands begin, end and strides vector operands.
  int64_t expected_size = -1;

  for (Value val : {op.begin(), op.end(), op.strides()}) {
    auto operand_ty = val.getType().dyn_cast<ShapedType>();
    if (!operand_ty || !operand_ty.hasStaticShape()) {
      // TensorFlow constant ops may have non-static shape because the shape is
      // not propagated during constant folding. If the defining op for this
      // operand is a constant op, use the constant op's attribute to get the
      // actual shape.
      DenseIntElementsAttr attr;
      if (!matchPattern(val, m_Constant(&attr))) continue;
      operand_ty = attr.getType();
    }

    if (operand_ty.getRank() != 1)
      return op.emitOpError()
             << "requires begin, end and strides to be 1D tensors";

    int64_t length = operand_ty.getDimSize(0);
    if (length == -1) continue;

    if (expected_size == -1) {
      // This op uses 32-bit masks.
      if (length >= 32)
        return op.emitOpError(
            "requires begin, end and strides operands with less than 32 "
            "elements");

      expected_size = length;
    } else if (length != expected_size) {
      return op.emitOpError() << "requires begin, end and strides to have the "
                                 "same number of elements";
    }
  }

  // If strides are constants, verify that none of the element is zero.
  DenseIntElementsAttr strides;
  if (matchPattern(op.strides(), m_Constant(&strides))) {
    if (llvm::is_contained(strides.getValues<APInt>(), 0))
      return op.emitOpError("requires non-zero strides");
  }

  // Use bit compares to ensure ellipsis_mask is 0 or a power of 2, i.e. there
  // exists only no more than one ellipsis.
  uint32_t ellipsis_mask = op.ellipsis_mask().getZExtValue();
  if (ellipsis_mask != 0 && !llvm::isPowerOf2_32(ellipsis_mask))
    return op.emitOpError("cannot have multiple ellipses");

  return success();
}

// Clamps the given `val`: returns `low` if `val` is less than `low`; returns
// `high` if `high` is less than `val`; otherwise returns `val`.
template <class T>
constexpr const T &Clamp(const T &val, const T &low, const T &high) {
  assert(!(high < low));
  return (val < low) ? low : (high < val) ? high : val;
}

// Checks if the `index` bit of `val` is set.
template <class T>
constexpr bool IsSet(const T &val, unsigned index) {
  return (val & (1 << index)) != 0;
}

// Sets the `index` bit of `val`.
template <class T>
constexpr void Set(T &val, unsigned index) {
  val |= (1 << index);
}

// Unset the `index` bit of `val`.
template <class T>
constexpr void Unset(T &val, unsigned index) {
  val &= ~(1 << index);
}

// Copy the `src_index` bit of `src` to `dst_index` bit of `dst`.
template <class T>
constexpr void CopyBit(const T &src, unsigned src_index, T &dst,
                       unsigned dst_index) {
  if (IsSet(src, src_index))
    Set(dst, dst_index);
  else
    Unset(dst, dst_index);
}

// The sparse spec of strided slice does not correspond to the number of
// dimensions. For example, sparse spec for foo[..., 3:10] for foo of shape (2,
// 4, 8) would have dims = 2.
struct SparseSliceSpec {
  int64_t dims;
  int32_t begin_mask, end_mask, ellipsis_mask, new_axis_mask, shrink_axis_mask;
  const ArrayRef<int64_t> &begin;
  const ArrayRef<int64_t> &end;
  const ArrayRef<int64_t> &strides;
};

// The dense spec of strided slice is the canonicalized version of sparse spec.
// The number of dimensions of dense spec correspond to the number of dimensions
// in operand tensor.
struct DenseSliceSpec {
  int64_t dims;
  int32_t begin_mask, end_mask, shrink_axis_mask;
  SmallVectorImpl<int64_t> &begin;
  SmallVectorImpl<int64_t> &end;
  SmallVectorImpl<int64_t> &strides;
};

// Make a sparse spec into a dense index spec.
// The sparse spec does not correspond to the number of dimensions
// Make a dense spec that corresponds to the number of dimensions
//
// For example suppose foo[...,3:, 2] on foo.shape=(2,2,3,4) then
// we need to produce the missing begin_mask, end_mask for the first two
// dimensions i.e. foo[:, :, 3:, 2].
static void BuildDenseSliceSpec(const SparseSliceSpec &sparse,
                                DenseSliceSpec *dense) {
  // Build expanded dense begin, end, strides, begin_mask, end_mask, and
  // shrink_axis_mask.
  dense->begin.resize(dense->dims);
  dense->end.resize(dense->dims);
  dense->strides.resize(dense->dims);
  dense->begin_mask = 0;
  dense->end_mask = 0;
  dense->shrink_axis_mask = 0;

  // Count number of new_axis after ellipsis. This helps in calculating the
  // number of dimensions ellipsis represents in the sparse spec.
  bool ellipsis_seen = false;
  int num_new_axis_after_ellipsis = 0;
  for (int sparse_index = 0; sparse_index < sparse.dims; ++sparse_index) {
    if (ellipsis_seen && IsSet(sparse.new_axis_mask, sparse_index))
      num_new_axis_after_ellipsis++;
    if (IsSet(sparse.ellipsis_mask, sparse_index)) ellipsis_seen = true;
  }

  int dense_index = 0;
  for (int sparse_index = 0; sparse_index < sparse.dims; ++sparse_index) {
    if (IsSet(sparse.new_axis_mask, sparse_index)) continue;
    if (IsSet(sparse.ellipsis_mask, sparse_index)) {
      auto next_index = std::min(dense->dims - (sparse.dims - sparse_index) +
                                     1 + num_new_axis_after_ellipsis,
                                 dense->dims);
      // Expand ellipsis into the appropriate dense indices. From current index
      // until next_index, all dimensions would have begin and end masks set and
      // stride 1, i.e., get all elements in those dimensions.
      for (; dense_index < next_index; ++dense_index) {
        dense->begin[dense_index] = dense->end[dense_index] = 0;
        dense->strides[dense_index] = 1;
        Set(dense->begin_mask, dense_index);
        Set(dense->end_mask, dense_index);
      }
      continue;
    }
    assert(dense_index < dense->dims);
    // Copy over the sparse indices to dense indices if ellipsis_mask and
    // new_axis_mask are not set.
    dense->begin[dense_index] = sparse.begin[sparse_index];
    dense->end[dense_index] = sparse.end[sparse_index];
    dense->strides[dense_index] = sparse.strides[sparse_index];
    CopyBit(sparse.begin_mask, sparse_index, dense->begin_mask, dense_index);
    CopyBit(sparse.end_mask, sparse_index, dense->end_mask, dense_index);
    CopyBit(sparse.shrink_axis_mask, sparse_index, dense->shrink_axis_mask,
            dense_index);
    dense_index++;
  }
}

// For the given `input_shape`, calculates the sliced shape using the given
// `begin`, `end`, and `stride` ranges and `begin_mask`, `end_mask`, and
// `shrink_axis_mask` masks. Updates the result back to `input_shape`. If
// `shrink_axis_mask` is not zero, this function will not drop the corresponding
// dimensions in `input_shape`; it will turn them into 1s. At the same time,
// canonicalizes `begin`, `end`, and `strides. The calculation follows
// tf.StridedSlice op semantics.
static void CalculateSlicedShapeFromDenseIndices(
    MutableArrayRef<int64_t> input_shape, int32_t begin_mask, int32_t end_mask,
    int32_t shrink_axis_mask, MutableArrayRef<int64_t> begin,
    MutableArrayRef<int64_t> end, MutableArrayRef<int64_t> stride) {
  assert(input_shape.size() <= 32);  // Only 32-bit masks are supported.

  // Make sure ranges' ranks are consistent with the input.
  assert(input_shape.size() == begin.size());
  assert(input_shape.size() == end.size());
  assert(input_shape.size() == stride.size());

  for (int i = 0, e = input_shape.size(); i < e; ++i) {
    if (ShapedType::isDynamic(input_shape[i])) continue;

    int64_t dim_i = input_shape[i];
    int64_t begin_i = begin[i];
    int64_t end_i = end[i];
    int64_t stride_i = stride[i];

    // [0]: mask for begin, [1]: mask for end
    int64_t masks[] = {begin_mask & (1 << i), end_mask & (1 << i)};
    // [0]: bound for begin, [1]: bound for end
    int64_t bounds[] = {stride_i > 0 ? 0 : -1,
                        stride_i > 0 ? dim_i : dim_i - 1};

    // Canonicalizes the given range `point` (begin/end) according to the
    // current dimension. `c` means case: 0 for begin, 1 for end.
    auto canonicalize = [&](int64_t point, int c) {
      if (masks[c]) return stride_i > 0 ? bounds[c] : bounds[(c + 1) & 1];

      // Add dim as offset to negative range point.
      point = point < 0 ? dim_i + point : point;
      return Clamp(point, bounds[0], bounds[1]);
    };

    begin_i = canonicalize(begin_i, 0);
    end_i = canonicalize(end_i, 1);

    int64_t interval_len = end_i - begin_i;
    int64_t size_i = 0;
    // If internal length is zero or has different sign from stride, it's a
    // degenerated case: we are slicing nothing. Otherwise, calculate the sliced
    // size.
    if (interval_len != 0 && (interval_len < 0) == (stride_i < 0))
      size_i = (interval_len / stride_i) + (interval_len % stride_i != 0);

    begin[i] = begin_i;
    if (IsSet(shrink_axis_mask, i)) {
      // Shrink this dimension. It means we only take the element at begin_i.
      input_shape[i] = 1;
      end[i] = begin_i + 1;
      stride[i] = 1;
    } else {
      input_shape[i] = size_i;
      end[i] = end_i;
      stride[i] = stride_i;
    }
  }
}

// For the given `input_shape`, calculates the sliced shape using the given
// `sparse_begin`, `sparse_end`, and `sparse_strides` ranges and `begin_mask`,
// `end_mask`, `ellipsis_mask` , `new_axis_mask` and `shrink_axis_mask` masks.
// Updates the result back to `input_shape`.
static void CalculateSlicedShapeFromSparseIndices(
    MutableArrayRef<int64_t> input_shape, ArrayRef<int64_t> sparse_begin,
    ArrayRef<int64_t> sparse_end, ArrayRef<int64_t> sparse_strides,
    int32_t begin_mask, int32_t end_mask, int32_t ellipsis_mask,
    int32_t new_axis_mask, int32_t shrink_axis_mask,
    SmallVectorImpl<int64_t> *begin, SmallVectorImpl<int64_t> *end,
    SmallVectorImpl<int64_t> *stride) {
  int64_t num_sparse_indices = sparse_begin.size();
  SparseSliceSpec sparse = {num_sparse_indices, begin_mask,    end_mask,
                            ellipsis_mask,      new_axis_mask, shrink_axis_mask,
                            sparse_begin,       sparse_end,    sparse_strides};

  // If no ellipsis_mask exists then an implicit ellipsis_mask at the end is
  // inserted. This handles cases where foo[2:4] (foo.shape() = [4, 8]) yields
  // a tensor of shape [2, 8], i.e., foo[2:4] is same as foo[2:4, ...].
  if (sparse.ellipsis_mask == 0) {
    Set(sparse.ellipsis_mask, sparse.dims);
    sparse.dims++;
  }

  int64_t dims = input_shape.size();
  DenseSliceSpec dense = {dims,
                          /*begin_mask = */ 0,
                          /*end_mask = */ 0,
                          /*shrink_axis_mask = */ 0,
                          *begin,
                          *end,
                          *stride};

  BuildDenseSliceSpec(sparse, &dense);
  CalculateSlicedShapeFromDenseIndices(input_shape, dense.begin_mask,
                                       dense.end_mask, dense.shrink_axis_mask,
                                       *begin, *end, *stride);
}

bool StridedSliceOp::GetSlicedBoundRanges(
    SmallVectorImpl<int64_t> *slice_begin, SmallVectorImpl<int64_t> *slice_end,
    SmallVectorImpl<int64_t> *slice_stride) {
  // TODO(hinsu): Support lowering for ops with dynamic begin and end values
  // when it is possible to derive indices based on mask attributes.
  DenseIntElementsAttr sparse_begin_attr, sparse_end_attr, sparse_strides_attr;
  if (!matchPattern(begin(), m_Constant(&sparse_begin_attr)) ||
      !matchPattern(end(), m_Constant(&sparse_end_attr)) ||
      !matchPattern(strides(), m_Constant(&sparse_strides_attr)))
    return false;

  auto input_ty = this->input().getType().dyn_cast<RankedTensorType>();
  if (!input_ty || !input_ty.hasStaticShape()) return false;
  auto input_shape = llvm::to_vector<4>(input_ty.getShape());

  SmallVector<int64_t, 4> sparse_begin, sparse_end, sparse_strides;

  for (const APInt &index : sparse_begin_attr)
    sparse_begin.push_back(index.getSExtValue());
  for (const APInt &index : sparse_end_attr)
    sparse_end.push_back(index.getSExtValue());
  for (const APInt &stride : sparse_strides_attr)
    sparse_strides.push_back(stride.getSExtValue());

  CalculateSlicedShapeFromSparseIndices(
      input_shape, sparse_begin, sparse_end, sparse_strides,
      begin_mask().getZExtValue(), end_mask().getZExtValue(),
      ellipsis_mask().getZExtValue(), new_axis_mask().getZExtValue(),
      shrink_axis_mask().getZExtValue(), slice_begin, slice_end, slice_stride);
  return true;
}

//===----------------------------------------------------------------------===//
// StridedSliceGradOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(StridedSliceGradOp op) {
  auto shape_type = op.shape().getType().dyn_cast<RankedTensorType>();
  if (shape_type && shape_type.getRank() != 1)
    return op.emitOpError("'shape' operand must be 1D tensor, but got ")
           << shape_type.getRank() << "D tensor";

  if (failed(VerifyStridedSliceBase(op))) return failure();

  // TODO(antiagainst): verify the gradient op.dy()'s shape is consistent with
  // the sliced type from StridedSlice.

  return success();
}

bool StridedSliceGradOp::GetSlicedShapeAndBoundRanges(
    SmallVectorImpl<int64_t> *input_shape,
    SmallVectorImpl<int64_t> *slice_begin, SmallVectorImpl<int64_t> *slice_end,
    SmallVectorImpl<int64_t> *slice_stride) {
  DenseIntElementsAttr shape_attr;
  DenseIntElementsAttr sparse_begin_attr, sparse_end_attr, sparse_strides_attr;
  if (!matchPattern(shape(), m_Constant(&shape_attr)) ||
      !matchPattern(begin(), m_Constant(&sparse_begin_attr)) ||
      !matchPattern(end(), m_Constant(&sparse_end_attr)) ||
      !matchPattern(strides(), m_Constant(&sparse_strides_attr)))
    return false;

  int rank = std::distance(shape_attr.begin(), shape_attr.end());

  input_shape->clear();
  input_shape->reserve(rank);
  for (const APInt &dim : shape_attr)
    input_shape->push_back(dim.getSExtValue());

  SmallVector<int64_t, 4> sparse_begin, sparse_end, sparse_strides;

  for (const APInt &index : sparse_begin_attr)
    sparse_begin.push_back(index.getSExtValue());
  for (const APInt &index : sparse_end_attr)
    sparse_end.push_back(index.getSExtValue());
  for (const APInt &stride : sparse_strides_attr)
    sparse_strides.push_back(stride.getSExtValue());

  CalculateSlicedShapeFromSparseIndices(
      *input_shape, sparse_begin, sparse_end, sparse_strides,
      begin_mask().getZExtValue(), end_mask().getZExtValue(),
      ellipsis_mask().getZExtValue(), new_axis_mask().getZExtValue(),
      shrink_axis_mask().getZExtValue(), slice_begin, slice_end, slice_stride);
  return true;
}

//===----------------------------------------------------------------------===//
// TensorListReserveOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(TensorListReserveOp op) {
  if (!IsOfRankOrUnranked(op.element_shape(), 0) &&
      !IsOfRankOrUnranked(op.element_shape(), 1)) {
    return op.emitOpError("requires element_shape operand to be 0D/1D tensor");
  }

  if (!IsOfRankOrUnranked(op.num_elements(), 0)) {
    return op.emitOpError("requires num_elements operand to be 0D tensor");
  }
  return success();
}

//===----------------------------------------------------------------------===//
// TensorListElementShapeOp
//===----------------------------------------------------------------------===//

OpFoldResult TensorListElementShapeOp::fold(ArrayRef<Attribute> operands) {
  int width =
      getType().cast<ShapedType>().getElementType().getIntOrFloatBitWidth();
  auto variant_type =
      getElementTypeOrSelf(getOperand().getType()).cast<TF::VariantType>();
  if (variant_type.getSubtypes().empty()) return {};
  return ConvertShapeToAttr(variant_type.getSubtypes()[0], width);
}

//===----------------------------------------------------------------------===//
// TensorListStackOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(TensorListStackOp op) {
  if (!IsOfRankOrUnranked(op.element_shape(), 0) &&
      !IsOfRankOrUnranked(op.element_shape(), 1)) {
    return op.emitOpError("requires element_shape operand to be 0D/1D tensor");
  }
  return success();
}

//===----------------------------------------------------------------------===//
// TensorScatterUpdateOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(TensorScatterUpdateOp op) {
  if (!HasRankAtLeast(op.tensor(), 1))
    return op.emitOpError(
        "requires tensor operand to have at least 1 dimension");
  if (!HasRankAtLeast(op.indices(), 1))
    return op.emitOpError(
        "requires indices operand to have at least 1 dimension");
  if (!HasRankAtLeast(op.updates(), 1))
    return op.emitOpError(
        "requires updates operand to have at least 1 dimension");

  auto tensor_ty = op.tensor().getType().dyn_cast<RankedTensorType>();
  auto indices_ty = op.indices().getType().dyn_cast<RankedTensorType>();
  if (!tensor_ty || !indices_ty) return success();

  int64_t num_index_dims = indices_ty.getShape().back();
  if (ShapedType::isDynamic(num_index_dims)) return success();

  if (num_index_dims > tensor_ty.getRank())
    return op.emitOpError(
        "requires tensor operand with rank greater than or equal to the "
        "indices operand's last dimensions");
  return success();
}

//===----------------------------------------------------------------------===//
// TopKV2Op
//===----------------------------------------------------------------------===//

static LogicalResult Verify(TopKV2Op op) {
  if (!HasRankAtLeast(op.input(), 1))
    return op.emitOpError(
        "requires input operand to have at least 1 dimension");

  if (!IsOfRankOrUnranked(op.k(), 0))
    return op.emitOpError("requires k operand to be 0D tensor");

  return success();
}

//===----------------------------------------------------------------------===//
// ToBoolOp
//===----------------------------------------------------------------------===//

namespace {
// If the input to ToBoolOp is a `tensor<i1>`, then the ToBoolOp is an identity
// function and can be removed.
class ToBoolOfZeroDBoolTensor : public OpRewritePattern<ToBoolOp> {
  using OpRewritePattern<ToBoolOp>::OpRewritePattern;
  LogicalResult matchAndRewrite(ToBoolOp op,
                                PatternRewriter &rewriter) const override {
    if (auto type = op.getOperand().getType().dyn_cast<RankedTensorType>()) {
      if (type.getRank() == 0 && type.getElementType().isInteger(1)) {
        rewriter.replaceOp(op, op.getOperand());
        return success();
      }
    }
    return failure();
  }
};
}  // namespace

void ToBoolOp::getCanonicalizationPatterns(OwningRewritePatternList &results,
                                           MLIRContext *context) {
  results.insert<ToBoolOfZeroDBoolTensor>(context);
}

//===----------------------------------------------------------------------===//
// TransposeOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(TransposeOp op) {
  // TODO(hinsu): Verify using a custom verifier that,
  // * Transpose permutation is 1-D of size equal to the rank of the first
  //   input, if the shapes are partially known. Requires use of a more
  //   restrictive type than TF_Tensor.
  // * Result shape dimensions are possible based on the input shape.
  return success();
}

// TODO(jpienaar): perm could be optional too.
void TransposeOp::build(OpBuilder &builder, OperationState &result, Value x,
                        Value perm) {
  auto x_type = x.getType().cast<TensorType>();
  // If value is unranked, then so is results.
  if (!x_type.hasRank())
    return TransposeOp::build(builder, result,
                              UnrankedTensorType::get(x_type.getElementType()),
                              x, perm);

  // TODO(jpienaar): Handle unknown perm case.

  // TODO(jpienaar): Extract utility function.
  auto etype = x_type.cast<ShapedType>().getElementType();
  DenseIntElementsAttr attr_shape;
  if (matchPattern(perm, m_Constant(&attr_shape))) {
    llvm::SmallVector<int64_t, 4> const_shape;
    if (attr_shape.isSplat()) {
      const_shape.assign(
          attr_shape.getNumElements(),
          x_type.getDimSize((*attr_shape.begin()).getSExtValue()));
    } else {
      const_shape.reserve(attr_shape.getNumElements());
      for (const auto &dim : attr_shape)
        const_shape.push_back(x_type.getDimSize(dim.getSExtValue()));
    }
    return TransposeOp::build(
        builder, result, RankedTensorType::get(const_shape, etype), x, perm);
  }
  return TransposeOp::build(builder, result, UnrankedTensorType::get(etype), x,
                            perm);
}

namespace {

OpFoldResult FoldIdentityTranspose(TransposeOp op) {
  auto const_perm = dyn_cast_or_null<TF::ConstOp>(op.perm().getDefiningOp());
  if (!const_perm) return {};

  auto const_value = const_perm.value();
  const auto elements = const_value.getValues<APInt>();

  for (auto it : llvm::enumerate(elements)) {
    if (it.index() != it.value()) return {};
  }

  // TODO(jpienaar): Remove if/when we handle this more generally.
  if (op.getType() != op.x().getType()) {
    // If the types don't match then only fold if all the operands are in the TF
    // dialect.
    for (auto user : op.getOperation()->getUsers())
      if (user->getDialect() != op.getDialect()) return {};
  }

  return op.x();
}

OpFoldResult FoldCancellableTranspose(TransposeOp op) {
  // Operand is a TransposeOp.
  auto transpose = dyn_cast_or_null<TF::TransposeOp>(op.x().getDefiningOp());
  if (!transpose) return {};

  // Permutations defined by constant operations.
  auto perm0 = dyn_cast_or_null<TF::ConstOp>(op.perm().getDefiningOp());
  auto perm1 = dyn_cast_or_null<TF::ConstOp>(transpose.perm().getDefiningOp());
  if (!perm0 || !perm1) return {};

  // With permutation indices that cancel each other
  auto perm0_value = perm0.value().cast<DenseIntElementsAttr>();
  auto perm1_value = perm1.value().cast<DenseIntElementsAttr>();
  if (!AreCancellablePermutations(perm0_value, perm1_value)) return {};

  return transpose.x();
}

}  // namespace

OpFoldResult TransposeOp::fold(ArrayRef<Attribute> operands) {
  if (auto folded = FoldIdentityTranspose(*this)) return folded;
  if (auto folded = FoldCancellableTranspose(*this)) return folded;
  return {};
}

//===----------------------------------------------------------------------===//
// TruncateDivOp
//===----------------------------------------------------------------------===//

void TruncateDivOp::getCanonicalizationPatterns(
    OwningRewritePatternList &results, MLIRContext *context) {
  results.insert<TruncateDivWithSqrtDivisor>(context);
}

//===----------------------------------------------------------------------===//
// UnpackOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(UnpackOp op) {
  auto value_type = op.value().getType().dyn_cast<RankedTensorType>();
  if (!value_type) return success();

  int64_t value_rank = value_type.getRank();
  int64_t axis = op.axis().getSExtValue();
  if (axis < -value_rank || axis >= value_rank)
    return op.emitOpError("axis attribute must be in the range of [-")
           << value_rank << ", " << value_rank << ')';

  axis = GetDimForAxis(axis, value_rank);
  int64_t dim_size = value_type.getDimSize(axis);
  if (ShapedType::isDynamic(dim_size)) return success();

  if (dim_size != op.getNumResults())
    return op.emitOpError("result count must be equal to ") << dim_size;

  return success();
}

//===----------------------------------------------------------------------===//
// Unsorted segment reduction ops
//===----------------------------------------------------------------------===//

template <class Op>
static LogicalResult VerifyUnsortedSegmentReduction(Op op) {
  if (!HasRankAtMost(op.num_segments(), 0))
    return op.emitOpError("number of segments should be a 0-D tensor");

  auto data_type = op.data().getType().template dyn_cast<RankedTensorType>();
  auto segment_ids_type =
      op.segment_ids().getType().template dyn_cast<RankedTensorType>();
  if (data_type && segment_ids_type) {
    if (data_type.getRank() < segment_ids_type.getRank())
      return op.emitOpError(
          "requires segment ids rank to be less than or equal to data's rank");

    int index = 0;
    for (auto shape_pair :
         llvm::zip_first(segment_ids_type.getShape(), data_type.getShape())) {
      int64_t segment_id_dim = std::get<0>(shape_pair);
      int64_t data_dim = std::get<1>(shape_pair);
      if (!ShapedType::isDynamic(segment_id_dim) &&
          !ShapedType::isDynamic(data_dim) && segment_id_dim != data_dim)
        return op.emitOpError(
                   "requires segment ids shape to be a prefix of data shape, "
                   "but dimension #")
               << index << " differs: " << segment_id_dim << " vs. "
               << data_dim;
      ++index;
    }
  }

  DenseIntElementsAttr num_segments_attr;
  if (matchPattern(op.num_segments(), m_Constant(&num_segments_attr))) {
    int64_t num_segments = (*num_segments_attr.begin()).getSExtValue();
    if (num_segments < 0)
      return op.emitOpError("num of segments cannot be negative");
  }

  return success();
}

//===----------------------------------------------------------------------===//
// VariableShapeOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(VariableShapeOp op) {
  auto input_type = op.input().getType().cast<TensorType>();
  if (input_type.hasStaticShape() && input_type.getNumElements() != 1)
    return op.emitOpError("requires input to have one resource");

  auto resource_type = input_type.getElementType().cast<TF::ResourceType>();
  auto subtypes = resource_type.getSubtypes();
  switch (subtypes.size()) {
    case 1:
      return VerifyShapeOperandAndResult(
          op, resource_type.getSubtypes().front(), op.getType());
    case 0:
      return VerifyShapeOperandAndResult(op, Type(), op.getType());
    default:
      return op.emitOpError(
          "requires resource input type to have at most 1 subtype");
  }
}

OpFoldResult VariableShapeOp::fold(ArrayRef<Attribute> operands) {
  int width =
      getType().cast<ShapedType>().getElementType().getIntOrFloatBitWidth();
  auto resource_type =
      getElementTypeOrSelf(getOperand().getType()).cast<TF::ResourceType>();
  if (resource_type.getSubtypes().empty()) return {};
  return ConvertShapeToAttr(resource_type.getSubtypes()[0], width);
}

//===----------------------------------------------------------------------===//
// WhileOp
//===----------------------------------------------------------------------===//

static LogicalResult Verify(WhileOp op) {
  auto module = op.getParentOfType<ModuleOp>();
  auto cond_fn = module.lookupSymbol<FuncOp>(op.cond());
  auto body_fn = module.lookupSymbol<FuncOp>(op.body());
  if (!cond_fn) {
    return op.emitOpError("cond refers to an undefined function : ")
           << op.cond();
  }
  if (!body_fn) {
    return op.emitOpError("body refers to an undefined function : ")
           << op.body();
  }

  auto cond_fn_type = cond_fn.getType();
  auto body_fn_type = body_fn.getType();

  // Verify that the cond function has exactly one result.
  if (cond_fn_type.getNumResults() != 1)
    return op.emitOpError("requires cond function to have exactly one result");

  SmallVector<Type, 4> operands(op.getOperandTypes());

  // Collect all the type lists for the op so that different pairs of type lists
  // can be compared for the compatibility.
  constexpr int kNumTypeLists = 5;
  const std::array<std::pair<std::string, ArrayRef<Type>>, kNumTypeLists>
      type_lists = {{
          {"operand", operands},
          {"body function result", body_fn_type.getResults()},
          {"result", op.getResultTypes()},
          {"cond function input", cond_fn_type.getInputs()},
          {"body function input", body_fn_type.getInputs()},
      }};

  // A pair of type lists should be cast compatible with each other if one is
  // converted to the another for a function call or assignment or there is a
  // common source of inputs for both.  Therefore, the While op requires the
  // following pairs of type lists to be cast compatible for the tensor_cast
  // operation:
  //
  // * Operands and cond inputs to call the cond function before the
  //   first iteration.
  // * Operands and body inputs to call the body function for the first
  //   iteration if the cond functions returns True or equivalent result.
  // * Operands and results to assign cond function arguments to op results if
  //   the cond function returns False or equivalent result.
  // * All three pairs using cond inputs, body inputs and results as operand is
  //   a common source for all three.
  // * Body result and cond inputs to call the cond function for the subsequent
  //   iterations. Similarly, Body result should be compatible with body inputs
  //   and op results.
  //
  // Note that the operands and body results need not be compatible as they are
  // never converted from one to the another nor there is a common source
  // tensors.  Compatibility requirement is not transitive.

  for (int i = 0; i < kNumTypeLists; ++i) {
    // Skip the first pair as the While op operands and body function results
    // does not need to be compatible with each other.
    for (int j = std::max(2, i + 1); j < kNumTypeLists; ++j) {
      auto &a = type_lists[i];
      auto &b = type_lists[j];

      int a_size = a.second.size();
      if (a_size != b.second.size())
        return op.emitOpError(
            llvm::formatv("requires the number of {0}s to be equal to the "
                          "number of {1}s. Found {2} and {3}, respectively",
                          a.first, b.first, a_size, b.second.size()));

      for (int idx = 0; idx < a_size; ++idx) {
        auto a_type = a.second[idx];
        auto b_type = b.second[idx];

        if (!AreCastCompatible({a_type, b_type}))
          return op.emitError(llvm::formatv(
              "{0} type {1} is incompatible with {2} type {3} at index {4}",
              a.first, a_type, b.first, b_type, idx));
      }
    }
  }
  return success();
}

//===----------------------------------------------------------------------===//
// XdivyOp
//===----------------------------------------------------------------------===//

void XdivyOp::getCanonicalizationPatterns(OwningRewritePatternList &results,
                                          MLIRContext *context) {
  results.insert<XdivyWithSqrtDivisor>(context);
}

//===----------------------------------------------------------------------===//
// TableGen'd op method definitions
//===----------------------------------------------------------------------===//

#define GET_OP_CLASSES
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.cc.inc"

//===----------------------------------------------------------------------===//
// TF Dialect Interfaces
//===----------------------------------------------------------------------===//

namespace {
struct TFInlinerInterface : public DialectInlinerInterface {
  using DialectInlinerInterface::DialectInlinerInterface;

  //===--------------------------------------------------------------------===//
  // Analysis Hooks
  //===--------------------------------------------------------------------===//

  // Defines the legality of inlinining 'src' region into the 'dest' region
  // attached to a TF operation
  bool isLegalToInline(Region *dest, Region *src,
                       BlockAndValueMapping &valueMapping) const final {
    // Allow inlining in regions attached to region based control flow
    // operations only if the src region is a single block region
    return isa<IfRegionOp>(dest->getParentOp()) && llvm::hasSingleElement(*src);
  }

  // Defines the legality of inlining TF operations.
  bool isLegalToInline(Operation *, Region *,
                       BlockAndValueMapping &) const final {
    // TODO(riverriddle) For now, enable inlining all operations. This isn't
    // correct in the face of operations that cannot be duplicated, but this
    // requires more intricate side-effect modeling.
    return true;
  }

  //===--------------------------------------------------------------------===//
  // Transformation Hooks
  //===--------------------------------------------------------------------===//

  // Attempts to materialize a conversion for a type mismatch between a call
  // from this dialect, and a callable region. This method should generate an
  // operation that takes 'input' as the only operand, and produces a single
  // result of 'resultType'. If a conversion can not be generated, nullptr
  // should be returned.
  Operation *materializeCallConversion(OpBuilder &builder, Value input,
                                       Type result_type,
                                       Location conversion_loc) const final {
    if (!result_type.isa<TensorType>() || !input.getType().isa<TensorType>())
      return nullptr;
    return builder.create<TF::CastOp>(conversion_loc, result_type, input,
                                      /*truncate=*/builder.getBoolAttr(false));
  }
};
}  // end anonymous namespace

//===----------------------------------------------------------------------===//
// TF Dialect
//===----------------------------------------------------------------------===//

#include "tensorflow/compiler/mlir/tensorflow/ir/tf_op_interfaces.cc.inc"

TensorFlowDialect::TensorFlowDialect(MLIRContext *context)
    : Dialect(/*name=*/"tf", context) {
  addOperations<
#define GET_OP_LIST
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.cc.inc"
      >();
  addTypes<
#define HANDLE_TF_TYPE(tftype, enumerant, name) tftype##Type,
#define HANDLE_LAST_TF_TYPE(tftype, enumerant, name) tftype##Type
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_types.def"
      >();
  addInterfaces<TFInlinerInterface>();
  addAttributes<ShapeAttr, FuncAttr>();

  // Support unknown operations because not all TensorFlow operations are
  // registered.
  allowUnknownOperations();
}

namespace {

ShapeAttr ParseShapeAttr(MLIRContext *context, StringRef spec, Location loc) {
  auto emit_error = [&, spec]() {
    emitError(loc, "invalid TensorFlow shape attribute: ") << spec;
    return nullptr;
  };

  if (!spec.consume_front("shape<")) return emit_error();

  if (spec.consume_front("*>"))
    return mlir::TF::ShapeAttr::get(context, llvm::None);

  SmallVector<int64_t, 4> shape;
  while (!spec.consume_front(">")) {
    int64_t dim;

    if (spec.consume_front("?"))
      dim = -1;
    else if (spec.consumeInteger(10, dim) || dim < 0)
      return emit_error();

    spec.consume_front("x");

    shape.push_back(dim);
  }

  return mlir::TF::ShapeAttr::get(context, llvm::makeArrayRef(shape));
}

void PrintShapeAttr(ShapeAttr attr, DialectAsmPrinter &os) {  // NOLINT
  os << "shape";

  os << "<";
  if (attr.hasRank()) {
    auto print_dim = [&](int64_t dim) {
      if (dim > -1)
        os << dim;
      else
        os << "?";
    };
    llvm::interleave(attr.getShape(), os, print_dim, "x");
  } else {
    os << "*";
  }
  os << ">";
}

// Parses a #tf.func attribute of the following format:
//
//   #tf.func<@symbol, {attr = "value"}>
//
// where the first element is a SymbolRefAttr and the second element is a
// DictionaryAttr.
FuncAttr ParseFuncAttr(MLIRContext *context, StringRef spec, Location loc) {
  auto emit_error = [&, spec]() {
    emitError(loc, "invalid TensorFlow func attribute: ") << spec;
    return nullptr;
  };

  if (!spec.consume_front("func<")) return emit_error();

  size_t func_name_num_read = 0;
  Attribute func_name_attr =
      mlir::parseAttribute(spec, context, func_name_num_read);
  if (!func_name_attr || !func_name_attr.isa<SymbolRefAttr>())
    return emit_error();
  spec = spec.drop_front(func_name_num_read);

  if (!spec.consume_front(", ")) return emit_error();

  size_t func_attrs_num_read = 0;
  Attribute func_attrs_attr =
      mlir::parseAttribute(spec, context, func_attrs_num_read);
  if (!func_attrs_attr || !func_attrs_attr.isa<DictionaryAttr>())
    return emit_error();
  spec = spec.drop_front(func_attrs_num_read);

  if (!spec.consume_front(">")) return emit_error();

  return mlir::TF::FuncAttr::get(context, func_name_attr.cast<SymbolRefAttr>(),
                                 func_attrs_attr.cast<DictionaryAttr>());
}

// Prints a #tf.func attribute of the following format:
//
//   #tf.func<@symbol, {attr = "value"}>
void PrintFuncAttr(FuncAttr attr, DialectAsmPrinter &os) {
  os << "func<" << attr.GetName() << ", " << attr.GetAttrs() << ">";
}

}  // namespace

Attribute TensorFlowDialect::parseAttribute(DialectAsmParser &parser,
                                            Type type) const {
  auto spec = parser.getFullSymbolSpec();
  Location loc = parser.getEncodedSourceLoc(parser.getNameLoc());

  if (spec.startswith("shape")) return ParseShapeAttr(getContext(), spec, loc);

  if (spec.startswith("func")) return ParseFuncAttr(getContext(), spec, loc);

  return (emitError(loc, "unknown TensorFlow attribute: " + spec), nullptr);
}

void TensorFlowDialect::printAttribute(Attribute attr,
                                       DialectAsmPrinter &os) const {
  switch (attr.getKind()) {
    case AttrKind::SHAPE:
      PrintShapeAttr(attr.cast<ShapeAttr>(), os);
      break;
    case AttrKind::FUNC:
      PrintFuncAttr(attr.cast<FuncAttr>(), os);
      break;
    default:
      llvm_unreachable("unexpected tensorflow attribute kind");
  }
}

// Parses a type registered to this dialect.
Type TensorFlowDialect::parseType(DialectAsmParser &parser) const {
  StringRef data;
  if (parser.parseKeyword(&data)) return Type();

  Location loc = parser.getEncodedSourceLoc(parser.getNameLoc());
  auto typeKind = llvm::StringSwitch<unsigned>(data)
#define HANDLE_TF_TYPE(tftype, enumerant, name) \
  .Case(name, TensorFlowTypes::enumerant)
// Custom TensorFlow types are handled separately at the end as they do partial
// match.
#define HANDLE_CUSTOM_TF_TYPE(tftype, enumerant, name)
// NOLINTNEXTLINE
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_types.def"
                      .StartsWith("resource", TensorFlowTypes::RESOURCE)
                      .StartsWith("variant", TensorFlowTypes::VARIANT)
                      .Default(0);
  switch (typeKind) {
    default:
      return (emitError(loc, "unknown TensorFlow type: " + data), nullptr);

#define HANDLE_TF_TYPE(tftype, enumerant, name) \
  case TensorFlowTypes::enumerant:              \
    return tftype##Type::get(getContext());
#define HANDLE_CUSTOM_TF_TYPE(tftype, enumerant, name)
// NOLINTNEXTLINE
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_types.def"
    case TensorFlowTypes::RESOURCE:
      return ParseResourceType(parser, loc);
    case TensorFlowTypes::VARIANT:
      return ParseVariantType(parser, loc);
  }
}

// Prints a type registered to this dialect.
void TensorFlowDialect::printType(Type ty, DialectAsmPrinter &os) const {
  assert(ty.isa<TensorFlowType>());
  switch (ty.getKind()) {
    default:
      llvm_unreachable("unexpected tensorflow type kind");
#define HANDLE_TF_TYPE(tftype, enumerant, name) \
  case TensorFlowTypes::enumerant:              \
    os << name;                                 \
    break;
#define HANDLE_CUSTOM_TF_TYPE(tftype, enumerant, name) \
  case TensorFlowTypes::enumerant:                     \
    Print##tftype##Type(ty.cast<tftype##Type>(), os);  \
    break;
// NOLINTNEXTLINE
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_types.def"
  }
}

namespace {
template <typename TypeWithSubtype>
Type ParseTypeWithSubtype(MLIRContext *context, DialectAsmParser &parser,
                          Location loc) {
  // Default type without inferred subtypes.
  if (failed(parser.parseOptionalLess())) return TypeWithSubtype::get(context);

  // Most types with subtypes have only one subtype.
  SmallVector<TensorType, 1> subtypes;
  do {
    TensorType tensor_ty;
    if (parser.parseType(tensor_ty)) return Type();
    subtypes.push_back(tensor_ty);
  } while (succeeded(parser.parseOptionalComma()));

  if (parser.parseGreater()) return Type();
  return TypeWithSubtype::getChecked(subtypes, context, loc);
}

template <typename TypeWithSubtype>
void PrintTypeWithSubtype(StringRef type, TypeWithSubtype ty,
                          DialectAsmPrinter &os) {
  os << type;
  ArrayRef<TensorType> subtypes = ty.getSubtypes();
  if (subtypes.empty()) return;

  os << "<";
  interleaveComma(subtypes, os);
  os << ">";
}
}  // anonymous namespace

Type TensorFlowDialect::ParseResourceType(DialectAsmParser &parser,
                                          Location loc) const {
  return ParseTypeWithSubtype<ResourceType>(getContext(), parser, loc);
}

void TensorFlowDialect::PrintResourceType(ResourceType ty,
                                          DialectAsmPrinter &os) const {
  return PrintTypeWithSubtype("resource", ty, os);
}

Type TensorFlowDialect::ParseVariantType(DialectAsmParser &parser,
                                         Location loc) const {
  return ParseTypeWithSubtype<VariantType>(getContext(), parser, loc);
}

void TensorFlowDialect::PrintVariantType(VariantType ty,
                                         DialectAsmPrinter &os) const {
  return PrintTypeWithSubtype("variant", ty, os);
}

Operation *TensorFlowDialect::materializeConstant(OpBuilder &builder,
                                                  Attribute value, Type type,
                                                  Location loc) {
  return builder.create<ConstOp>(loc, type, value);
}

}  // namespace TF
}  // namespace mlir
