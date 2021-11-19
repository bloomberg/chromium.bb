/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/compiler/mlir/tfrt/jit/transforms/tf_cpurt_clustering.h"

#include <functional>
#include <utility>

#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringSet.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops_a_m.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops_n_z.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_remaining_ops.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/cluster_ops_by_policy.h"
#include "tfrt/cpu/jit/cpurt_support.h"  // from @tf_runtime

namespace tensorflow {

using mlir::failure;
using mlir::LogicalResult;
using mlir::Operation;
using mlir::success;
using mlir::TensorType;
using mlir::Type;
using mlir::Value;

using mlir::TFDevice::Cluster;
using mlir::TFDevice::ClusteringPolicy;
using mlir::TFDevice::ClusteringPolicySet;
using mlir::TFDevice::ValueConstraint;
using mlir::TFDevice::ValuesConstraintSet;

using mlir::TF::_FusedMatMulOp;
using mlir::TF::BatchMatMulV2Op;
using mlir::TF::BroadcastToOp;
using mlir::TF::ConcatV2Op;
using mlir::TF::ConstOp;
using mlir::TF::ExpandDimsOp;
using mlir::TF::FillOp;
using mlir::TF::MatMulOp;
using mlir::TF::OneHotOp;
using mlir::TF::PackOp;
using mlir::TF::RangeOp;
using mlir::TF::ReshapeOp;
using mlir::TF::ShapeOp;
using mlir::TF::SliceOp;
using mlir::TF::SqueezeOp;
using mlir::TF::StopGradientOp;
using mlir::TF::StridedSliceOp;
using mlir::TF::TransposeOp;

namespace {

// A set of clustering constraints that allow TF -> CPURT compilation pipeline
// to lower Tensorflow operations to MHLO and then to Linalg. Tensorflow
// dynamism is not fully representable at Linalg level, so by providing a
// clustering policy we ensure that we can successfully compile all clustered
// operations (we have enough static information to lower to MHLO, or build
// static Linalg indexing maps).
//
// Some of these constraints gets resolved at constant folding time, and
// operations are completely removed from the IR, and some constraints just
// enable TF->MHLO or MHLO->Linalg lowering.

// Returns true if all types are supported by the Tensorflow -> CPURT
// compilation pipeline and TFRT JIT runtime integration (see cpurt.h).
template <typename TypeRange>
static bool IsSupportedDataTypes(TypeRange&& types) {
  return llvm::all_of(types, [](Type type) -> bool {
    if (auto tensor = type.dyn_cast<TensorType>()) {
      auto elt_type = tensor.getElementType();
      return elt_type.isF32() || elt_type.isInteger(1) ||
             elt_type.isInteger(32) || elt_type.isInteger(64);
    }
    return false;
  });
}

static bool IsSupportedOperandTypes(Operation* op) {
  return IsSupportedDataTypes(op->getOperandTypes());
}

static bool IsSupportedResultTypes(Operation* op) {
  return IsSupportedDataTypes(op->getResultTypes());
}

static bool IsSupportedOperandAndResultTypes(Operation* op) {
  return IsSupportedOperandTypes(op) && IsSupportedResultTypes(op);
}

// Clustering policy for a specific Tensorflow operation type that verifies
// that operation operands and results data types are supported.
template <typename OpTy>
class TensorflowOpClusteringPolicy : public ClusteringPolicy {
 public:
  LogicalResult MatchAndUpdateConstraints(
      Operation* operation, const ValuesConstraintSet& results,
      ValuesConstraintSet& operands) const final {
    auto op = mlir::dyn_cast<OpTy>(operation);
    if (op && IsSupportedOperandAndResultTypes(op))
      return MatchAndUpdateConstraints(op, results, operands);
    return failure();
  }

  virtual LogicalResult MatchAndUpdateConstraints(
      OpTy op, const ValuesConstraintSet& results,
      ValuesConstraintSet& operands) const = 0;
};

// -------------------------------------------------------------------------- //
// Default clustering policy for TF -> CPURT compilation.
// -------------------------------------------------------------------------- //

// Default clustering policy for Tensorflow -> TFRT JIT compilation propagates
// the most restrictive constraint from the results to all operands. If results
// do not have any constraints it adds default constraint to all operands if it
// is provided, otherwise just returns `success` without adding any constraints.
class DefaultClusteringPolicy : public ClusteringPolicy {
 public:
  explicit DefaultClusteringPolicy(
      std::function<bool(Operation*)> filter,
      llvm::Optional<ValueConstraint> default_constraint = llvm::None)
      : filter_(std::move(filter)), default_constraint_(default_constraint) {}

  LogicalResult MatchAndUpdateConstraints(
      Operation* op, const ValuesConstraintSet& results,
      ValuesConstraintSet& operands) const final;

 private:
  // A filter for operations that are supported.
  std::function<bool(Operation*)> filter_;
  // Default constraint for all operands.
  llvm::Optional<ValueConstraint> default_constraint_;
};

template <typename OpTy>
class OpDefaultClusteringPolicy : public DefaultClusteringPolicy {
 public:
  explicit OpDefaultClusteringPolicy(
      llvm::Optional<ValueConstraint> default_constraint = llvm::None)
      : DefaultClusteringPolicy(
            [](Operation* op) -> bool { return mlir::isa<OpTy>(op); },
            default_constraint) {}
};

LogicalResult DefaultClusteringPolicy::MatchAndUpdateConstraints(
    Operation* op, const ValuesConstraintSet& results,
    ValuesConstraintSet& operands) const {
  if (!filter_(op)) return failure();

  if (!IsSupportedOperandAndResultTypes(op)) return failure();

  // Find the most restrictive constraint from the operation results.
  llvm::Optional<ValueConstraint> default_constraint = default_constraint_;

  for (mlir::Value result : op->getResults()) {
    if (auto result_constraint = results.GetConstraint(result)) {
      // TODO(ezhulenev): We can safely propagate value constraints if we know
      // that the value is an integer scalar or a small vector, however in
      // practice all values that we are interested in are defined by constant
      // operations directly. Revisit if this becomes a problem.
      if (*result_constraint == ValueConstraint::kValue) return failure();

      default_constraint = default_constraint.hasValue()
                               ? Merge(*default_constraint, *result_constraint)
                               : *result_constraint;
    }
  }

  // No constraints to propagate.
  if (!default_constraint.hasValue()) return success();

  // Propage constraint to all operands.
  for (unsigned i = 0; i < op->getNumOperands(); ++i)
    operands.Insert(op->getOperand(i), *default_constraint);
  return success();
}

// -------------------------------------------------------------------------- //
// tf.BatchMatMulV2
// -------------------------------------------------------------------------- //

class BatchMatMulV2OpClusteringPolicy
    : public OpDefaultClusteringPolicy<BatchMatMulV2Op> {};

// -------------------------------------------------------------------------- //
// tf.BroadcastTo
// -------------------------------------------------------------------------- //

class BroadcastToOpClusteringPolicy
    : public TensorflowOpClusteringPolicy<BroadcastToOp> {
  LogicalResult MatchAndUpdateConstraints(
      BroadcastToOp op, const ValuesConstraintSet& results,
      ValuesConstraintSet& operands) const final {
    // Only ranked inputs are supported.
    operands.Insert(op.input(), ValueConstraint::kRank);

    if (auto result_constraint = results.GetConstraint(op.getResult())) {
      if (*result_constraint == ValueConstraint::kValue) return failure();
      // For a static output shape we need a constant shape operand.
      if (*result_constraint == ValueConstraint::kShape) {
        operands.Insert(op.shape(), ValueConstraint::kValue);
        return success();
      }
    }

    // Producing a ranked output requires a known shape for the shape operand.
    operands.Insert(op.shape(), ValueConstraint::kShape);

    return success();
  }
};

// -------------------------------------------------------------------------- //
// Cwise Binary Operations.
// -------------------------------------------------------------------------- //

class CwiseBinaryOpClusteringPolicy : public DefaultClusteringPolicy {
 public:
  CwiseBinaryOpClusteringPolicy()
      : DefaultClusteringPolicy(IsBinaryOp(), ValueConstraint::kRank) {}

 private:
  // TODO(ezhulenev): Use mlir::isa<>() to filter operations.
  std::function<bool(Operation* op)> IsBinaryOp() {
    llvm::StringSet<> binary_ops = {
        "tf.Add",
        "tf.AddV2",
        "tf.ApproximateEqual",
        "tf.Atan2",
        "tf.BiasAdd",
        "tf.BitwiseAnd",
        "tf.BitwiseOr",
        "tf.BitwiseXor",
        "tf.Div",
        "tf.DivNoNan",
        "tf.Equal",
        "tf.FloorDiv",
        "tf.FloorMod",
        "tf.Greater",
        "tf.GreaterEqual",
        "tf.Less",
        "tf.LessEqual",
        "tf.LogicalAnd",
        "tf.LogicalOr",
        "tf.Maximum",
        "tf.Minimum",
        "tf.Mod",
        "tf.Mul",
        "tf.MulNoNan",
        "tf.NotEqual",
        "tf.Pow",
        "tf.RealDiv",
        "tf.SquaredDifference",
        "tf.Sub",
        "tf.Xdivy",
        "tf.Xlogy",
    };
    return [binary_ops = std::move(binary_ops)](Operation* op) {
      return binary_ops.contains(op->getName().getStringRef());
    };
  }
};

// -------------------------------------------------------------------------- //
// Cwise Unary Operations.
// -------------------------------------------------------------------------- //

class CwiseUnaryOpClusteringPolicy : public DefaultClusteringPolicy {
 public:
  CwiseUnaryOpClusteringPolicy()
      : DefaultClusteringPolicy(IsUnaryOp(), ValueConstraint::kRank) {}

 private:
  std::function<bool(Operation* op)> IsUnaryOp() {
    // TODO(ezhulenev): Use mlir::isa<>() to filter operations.
    llvm::StringSet<> unary_ops = {
        "tf.Abs",      "tf.Acos",        "tf.Acosh",      "tf.Asin",
        "tf.Asinh",    "tf.Atan",        "tf.Atanh",      "tf.Cast",
        "tf.Ceil",     "tf.ClipByValue", "tf.ComplexAbs", "tf.Conj",
        "tf.Cos",      "tf.Cosh",        "tf.Elu",        "tf.Erf",
        "tf.Exp",      "tf.Floor",       "tf.Inv",        "tf.Invert",
        "tf.IsFinite", "tf.IsInf",       "tf.IsNan",      "tf.LeakyRelu",
        "tf.Log",      "tf.Log1p",       "tf.LogicalNot", "tf.Neg",
        "tf.Real",     "tf.Reciprocal",  "tf.Relu",       "tf.Relu6",
        "tf.Rint",     "tf.Round",       "tf.Rsqrt",      "tf.Selu",
        "tf.Sigmoid",  "tf.Sign",        "tf.Sin",        "tf.Sinh",
        "tf.Softplus", "tf.Softsign",    "tf.Sqrt",       "tf.Square",
        "tf.Tan",      "tf.Tanh",        "tf.ZerosLike",
    };
    return [unary_ops = std::move(unary_ops)](Operation* op) {
      return unary_ops.contains(op->getName().getStringRef());
    };
  }
};

// -------------------------------------------------------------------------- //
// Cwise Ternary Operations.
// -------------------------------------------------------------------------- //

class CwiseTernaryOpClusteringPolicy : public DefaultClusteringPolicy {
 public:
  CwiseTernaryOpClusteringPolicy()
      : DefaultClusteringPolicy(IsTernaryOp(), ValueConstraint::kRank) {}

 private:
  std::function<bool(Operation* op)> IsTernaryOp() {
    return [](Operation* op) {
      return mlir::isa<mlir::TF::SelectOp, mlir::TF::SelectV2Op>(op);
    };
  }
};

// -------------------------------------------------------------------------- //
// Reduction Operations.
// -------------------------------------------------------------------------- //

// Clustering policy for Tensorflow reduction operations:
//   - shape constraint can be propagated from the result to the input
//   - reduction indices value must be known at compile time
//
// All operations that use this policy must have two operands (input and
// reduction indices) and a single result.
class ReductionOpClusteringPolicy : public ClusteringPolicy {
 public:
  LogicalResult MatchAndUpdateConstraints(
      Operation* op, const ValuesConstraintSet& results,
      ValuesConstraintSet& operands) const final;

 private:
  bool IsSupported(Operation* op) const;
};

LogicalResult ReductionOpClusteringPolicy::MatchAndUpdateConstraints(
    Operation* op, const ValuesConstraintSet& results,
    ValuesConstraintSet& operands) const {
  // Verify that the operation is a reduction with supported operands
  // and results data types.
  if (!IsSupported(op) || !IsSupportedOperandAndResultTypes(op))
    return failure();

  assert(op->getNumOperands() == 2 && "expected two operands");
  assert(op->getNumResults() == 1 && "expected one result");

  // Propagate constraint from the result to the input.
  if (auto result_constraint = results.GetConstraint(op->getResult(0))) {
    if (*result_constraint == ValueConstraint::kValue) return failure();
    operands.Insert(op->getOperand(0), *result_constraint);
  } else {
    operands.Insert(op->getOperand(0), ValueConstraint::kRank);
  }

  // Reduction indices must be known at compile time.
  operands.Insert(op->getOperand(1), ValueConstraint::kValue);

  return success();
}

bool ReductionOpClusteringPolicy::IsSupported(Operation* op) const {
  return mlir::isa<mlir::TF::AllOp,   //
                   mlir::TF::AnyOp,   //
                   mlir::TF::MaxOp,   //
                   mlir::TF::MeanOp,  //
                   mlir::TF::MinOp,   //
                   mlir::TF::ProdOp,  //
                   mlir::TF::SumOp>(op);
}

// -------------------------------------------------------------------------- //
// tf.ConcatV2
// -------------------------------------------------------------------------- //

class ConcatV2OpClusteringPolicy
    : public TensorflowOpClusteringPolicy<ConcatV2Op> {
  LogicalResult MatchAndUpdateConstraints(
      ConcatV2Op op, const ValuesConstraintSet& results,
      ValuesConstraintSet& operands) const final {
    auto result_constraint = results.GetConstraint(op->getResult(0));
    if (result_constraint && *result_constraint == ValueConstraint::kValue)
      return failure();

    // Propagate constraint from the result to the input. All inputs always need
    // a known rank.
    for (auto value : op.values()) {
      operands.Insert(value,
                      result_constraint.getValueOr(ValueConstraint::kRank));
    }

    // Force axis to be a constant.
    operands.Insert(op.axis(), ValueConstraint::kValue);

    return success();
  }
};

// -------------------------------------------------------------------------- //
// tf.Const
// -------------------------------------------------------------------------- //

class ConstOpClusteringPolicy : public TensorflowOpClusteringPolicy<ConstOp> {
  LogicalResult MatchAndUpdateConstraints(
      ConstOp op, const ValuesConstraintSet& results,
      ValuesConstraintSet& operands) const final {
    // We cluster constant operation only if it is required to resolve some of
    // the constraints.
    auto result_constraint = results.GetConstraint(op.getResult());
    if (!result_constraint.hasValue()) return failure();

    return IsCompilableConstant(op.value());
  }
};

// -------------------------------------------------------------------------- //
// tf.ExpandDims
// -------------------------------------------------------------------------- //

class ExpandDimsOpClusteringPolicy
    : public TensorflowOpClusteringPolicy<ExpandDimsOp> {
  LogicalResult MatchAndUpdateConstraints(
      ExpandDimsOp op, const ValuesConstraintSet& results,
      ValuesConstraintSet& operands) const final {
    // Propagate constraint from the result to the input.
    if (auto result_constraint = results.GetConstraint(op->getResult(0))) {
      if (*result_constraint == ValueConstraint::kValue) return failure();
      operands.Insert(op.input(), *result_constraint);
    } else {
      operands.Insert(op.input(), ValueConstraint::kRank);
    }

    // The inserted dimension must be always known at compile time.
    operands.Insert(op.dim(), ValueConstraint::kValue);

    return success();
  }
};

// -------------------------------------------------------------------------- //
// tf._FusedMatMul
// -------------------------------------------------------------------------- //

class FusedMatMulOpClusteringPolicy
    : public TensorflowOpClusteringPolicy<_FusedMatMulOp> {
  LogicalResult MatchAndUpdateConstraints(
      _FusedMatMulOp op, const ValuesConstraintSet& results,
      ValuesConstraintSet& operands) const final {
    // Check if the default policy accepts the operation.
    OpDefaultClusteringPolicy<_FusedMatMulOp> default_policy;
    if (failed(default_policy.MatchAndUpdateConstraints(op, results, operands)))
      return failure();

    // Check if we do support a set of fused operations.
    size_t n = op.fused_ops().size();

    auto fusion =
        n > 0 ? op.fused_ops()[0].dyn_cast<mlir::StringAttr>() : nullptr;
    auto activation =
        n > 1 ? op.fused_ops()[1].dyn_cast<mlir::StringAttr>() : nullptr;

    if ((n > 0 && !fusion) || (n > 1 && !activation)) return failure();

    // TODO(ezhulenev): Update fission pass to support more fusions and
    // activations.

    // We only support BiasAdd fusion ...
    if (fusion && fusion.getValue() != "BiasAdd") return failure();

    // ... with Relu activation.
    if (activation && activation.getValue() != "Relu") return failure();

    return success();
  }
};

// -------------------------------------------------------------------------- //
// tf.Fill
// -------------------------------------------------------------------------- //

class FillOpClusteringPolicy : public TensorflowOpClusteringPolicy<FillOp> {
  LogicalResult MatchAndUpdateConstraints(
      FillOp op, const ValuesConstraintSet& results,
      ValuesConstraintSet& operands) const final {
    // Fill operation does not have any default constraints.
    auto result_constraint = results.GetConstraint(op->getResult(0));
    if (!result_constraint.hasValue()) return success();

    // To know the result shape we need to know the shape operand value.
    if (*result_constraint == ValueConstraint::kShape)
      operands.Insert(op.dims(), ValueConstraint::kValue);

    // To know the result rank we need to know the shape operand shape.
    if (*result_constraint == ValueConstraint::kRank)
      operands.Insert(op.dims(), ValueConstraint::kShape);

    // Value constraint propagation is not supported.
    if (*result_constraint == ValueConstraint::kValue) return failure();

    return success();
  }
};

// -------------------------------------------------------------------------- //
// tf.MatMul
// -------------------------------------------------------------------------- //

class MatMulOpClusteringPolicy : public OpDefaultClusteringPolicy<MatMulOp> {};

// -------------------------------------------------------------------------- //
// tf.OneHot
// -------------------------------------------------------------------------- //

class OneHotOpClusteringPolicy : public TensorflowOpClusteringPolicy<OneHotOp> {
  LogicalResult MatchAndUpdateConstraints(
      OneHotOp op, const ValuesConstraintSet& results,
      ValuesConstraintSet& operands) const final {
    // Value constraint propagation is not supported.
    if (auto constraint = results.GetConstraint(op.getResult()))
      if (*constraint == ValueConstraint::kValue) return failure();

    // MHLO lowering needs a static shape for the indices and a constant depth.
    operands.Insert(op.indices(), ValueConstraint::kShape);
    operands.Insert(op.depth(), ValueConstraint::kValue);

    return success();
  }
};

// -------------------------------------------------------------------------- //
// tf.Pack
// -------------------------------------------------------------------------- //

class PackOpClusteringPolicy : public OpDefaultClusteringPolicy<PackOp> {};

// -------------------------------------------------------------------------- //
// tf.Range
// -------------------------------------------------------------------------- //

class RangeOpClusteringPolicy : public TensorflowOpClusteringPolicy<RangeOp> {
  LogicalResult MatchAndUpdateConstraints(
      RangeOp op, const ValuesConstraintSet& results,
      ValuesConstraintSet& operands) const final {
    // Range operation does not have any default constraints.
    auto result_constraint = results.GetConstraint(op.getResult());
    if (!result_constraint.hasValue()) return success();

    // To know the result shape we need the input values.
    if (*result_constraint == ValueConstraint::kShape) {
      operands.Insert({op.start(), op.limit(), op.delta()},
                      ValueConstraint::kValue);
    }

    // Value constraint propagation is not supported.
    if (*result_constraint == ValueConstraint::kValue) return failure();

    return success();
  }
};

// -------------------------------------------------------------------------- //
// tf.Reshape
// -------------------------------------------------------------------------- //

class ReshapeOpClusteringPolicy
    : public TensorflowOpClusteringPolicy<ReshapeOp> {
  LogicalResult MatchAndUpdateConstraints(
      ReshapeOp op, const ValuesConstraintSet& results,
      ValuesConstraintSet& operands) const final {
    // The runtime only supports ranked tensors.
    operands.Insert(op.tensor(), ValueConstraint::kRank);

    // Reshape operation does not have any default constraints.
    auto result_constraint = results.GetConstraint(op.getResult());
    if (!result_constraint.hasValue()) return success();

    // To know the result shape we need to know the shape operand value. We also
    // require a static shape on the input in case there's a -1 in the shape.
    if (*result_constraint == ValueConstraint::kShape) {
      operands.Insert(op.shape(), ValueConstraint::kValue);
      operands.Insert(op.tensor(), ValueConstraint::kShape);
    }

    // To know the result rank we need to know the shape operand shape.
    if (*result_constraint == ValueConstraint::kRank)
      operands.Insert(op.shape(), ValueConstraint::kShape);

    // Value constraint propagation is not supported.
    if (*result_constraint == ValueConstraint::kValue) return failure();

    return success();
  }
};

// -------------------------------------------------------------------------- //
// tf.Shape
// -------------------------------------------------------------------------- //

class ShapeOpClusteringPolicy : public TensorflowOpClusteringPolicy<ShapeOp> {
  LogicalResult MatchAndUpdateConstraints(
      ShapeOp op, const ValuesConstraintSet& results,
      ValuesConstraintSet& operands) const final {
    // Check constraint on the result value.
    auto result_constraint = results.GetConstraint(op.getResult());
    if (!result_constraint.hasValue()) return success();

    // To know the result shape we need only the rank of the input.
    if (*result_constraint == ValueConstraint::kShape)
      operands.Insert(op.input(), ValueConstraint::kRank);

    // To know the result value we need to know the shape of the input.
    if (*result_constraint == ValueConstraint::kValue)
      operands.Insert(op.input(), ValueConstraint::kShape);

    return success();
  }
};

// -------------------------------------------------------------------------- //
// tf.Softmax
// -------------------------------------------------------------------------- //

class SoftmaxOpClusteringPolicy : public DefaultClusteringPolicy {
 public:
  SoftmaxOpClusteringPolicy()
      : DefaultClusteringPolicy(IsSoftmaxOp(), ValueConstraint::kRank) {}

 private:
  std::function<bool(Operation* op)> IsSoftmaxOp() {
    return [](Operation* op) {
      return mlir::isa<mlir::TF::SoftmaxOp, mlir::TF::LogSoftmaxOp>(op);
    };
  }
};

// -------------------------------------------------------------------------- //
// tf.Squeeze
// -------------------------------------------------------------------------- //

class SqueezeOpClusteringPolicy
    : public TensorflowOpClusteringPolicy<SqueezeOp> {
  LogicalResult MatchAndUpdateConstraints(
      SqueezeOp op, const ValuesConstraintSet& results,
      ValuesConstraintSet& operands) const final {
    // Propagate static shape constraints.
    auto input_constraint = ValueConstraint::kRank;
    if (auto result_constraint = results.GetConstraint(op.getResult())) {
      if (*result_constraint == ValueConstraint::kValue) return failure();
      input_constraint = *result_constraint;
    }

    // If squeeze_dims is not present we need a static shape.
    if (op.squeeze_dims().empty()) input_constraint = ValueConstraint::kShape;

    operands.Insert(op.input(), input_constraint);
    return success();
  }
};

// -------------------------------------------------------------------------- //
// tf.StopGradient
// -------------------------------------------------------------------------- //

class StopGradientOpClusteringPolicy
    : public OpDefaultClusteringPolicy<StopGradientOp> {};

// -------------------------------------------------------------------------- //
// tf.Transpose
// -------------------------------------------------------------------------- //

class TransposeOpClusteringPolicy
    : public TensorflowOpClusteringPolicy<TransposeOp> {
  LogicalResult MatchAndUpdateConstraints(
      TransposeOp op, const ValuesConstraintSet& results,
      ValuesConstraintSet& operands) const final {
    // Propagate result constraints to the input, at minimum require known rank.
    if (auto constraint = results.GetConstraint(op.getResult())) {
      operands.Insert(op.x(), *constraint);
    } else {
      operands.Insert(op.x(), ValueConstraint::kRank);
    }

    // Permutation must be always known at compile time.
    operands.Insert(op.perm(), ValueConstraint::kValue);

    return success();
  }
};

// -------------------------------------------------------------------------- //
// tf.Slice
// -------------------------------------------------------------------------- //

class SliceOpClusteringPolicy : public TensorflowOpClusteringPolicy<SliceOp> {
  LogicalResult MatchAndUpdateConstraints(
      SliceOp op, const ValuesConstraintSet& results,
      ValuesConstraintSet& operands) const final {
    // Value constraint propagation is not supported.
    if (auto constraint = results.GetConstraint(op.getResult()))
      if (*constraint == ValueConstraint::kValue) return failure();

    // We must know the shape of the input.
    operands.Insert(op.input(), ValueConstraint::kShape);

    // Force begin and size to be constants. The restriction on begin could be
    // lifted if we know that there are no `-1` sizes.
    // TODO(kramerb): Revisit this when mhlo.real_dynamic_slice stabilizes.
    operands.Insert({op.begin(), op.size()}, ValueConstraint::kValue);

    return success();
  }
};

// -------------------------------------------------------------------------- //
// tf.StridedSlice
// -------------------------------------------------------------------------- //

class StridedSliceOpClusteringPolicy
    : public TensorflowOpClusteringPolicy<StridedSliceOp> {
  LogicalResult MatchAndUpdateConstraints(
      StridedSliceOp op, const ValuesConstraintSet& results,
      ValuesConstraintSet& operands) const final {
    // We must know the shape of the input.
    operands.Insert(op.input(), ValueConstraint::kShape);

    // And values of operands that control the slice size.
    operands.Insert({op.begin(), op.end(), op.strides()},
                    ValueConstraint::kValue);

    return success();
  }
};

}  // namespace

void populateTfCpurtClusteringPolicies(ClusteringPolicySet& policies,
                                       CpurtClusteringTier tier) {
  // Returns true if the given cpurt compilation tier is enabled.
  auto is_enabled = [&](CpurtClusteringTier requested) -> bool {
    return static_cast<uint8_t>(requested) <= static_cast<uint8_t>(tier);
  };

  if (is_enabled(CpurtClusteringTier::kTier1)) {
    policies.Add<CwiseBinaryOpClusteringPolicy,   //
                 CwiseUnaryOpClusteringPolicy,    //
                 CwiseTernaryOpClusteringPolicy,  //
                 StopGradientOpClusteringPolicy,  //
                 TransposeOpClusteringPolicy>();
  }

  if (is_enabled(CpurtClusteringTier::kAll)) {
    policies.Add<BatchMatMulV2OpClusteringPolicy,  //
                 BroadcastToOpClusteringPolicy,    //
                 ConcatV2OpClusteringPolicy,       //
                 ExpandDimsOpClusteringPolicy,     //
                 FillOpClusteringPolicy,           //
                 FusedMatMulOpClusteringPolicy,    //
                 MatMulOpClusteringPolicy,         //
                 OneHotOpClusteringPolicy,         //
                 PackOpClusteringPolicy,           //
                 RangeOpClusteringPolicy,          //
                 ReductionOpClusteringPolicy,      //
                 ReshapeOpClusteringPolicy,        //
                 ShapeOpClusteringPolicy,          //
                 SliceOpClusteringPolicy,          //
                 SoftmaxOpClusteringPolicy,        //
                 SqueezeOpClusteringPolicy,        //
                 StridedSliceOpClusteringPolicy>();
  }
}

void populateTfCpurtConstraintsPolicies(ClusteringPolicySet& policies,
                                        CpurtClusteringTier tier) {
  populateTfCpurtClusteringPolicies(policies, tier);
  policies.Add<ConstOpClusteringPolicy>();
}

// -------------------------------------------------------------------------- //
// Helper functions.
// -------------------------------------------------------------------------- //

mlir::LogicalResult IsCompilableConstant(mlir::ElementsAttr value) {
  return success(value.getNumElements() <= 16 &&
                 value.getType().getElementType().isIntOrIndexOrFloat());
}

mlir::LogicalResult VerifyCluster(const Cluster& cluster) {
  llvm::SmallDenseSet<Operation*> ops;
  for (Operation* op : cluster.operations) {
    auto inserted = ops.insert(op);
    assert(inserted.second && "clustered operations must be unique");
    (void)inserted;
  }

  // TODO(b/196192286): This is a temporary workaround to disable excessive
  // recompilation for dynamic shapes in one particular model. Remove this once
  // specialization will be done based on shape constraints.
  for (Operation* op : ops) {
    for (Value value : op->getOperands()) {
      Operation* defining_op = value.getDefiningOp();
      if (!defining_op) continue;

      // Check if value will be sunk into the cluster body.
      auto const_op = mlir::dyn_cast<mlir::TF::ConstOp>(defining_op);
      if (const_op && succeeded(IsCompilableConstant(const_op.value())))
        continue;

      // Skip clusters with non-f32 inputs.
      if (!ops.contains(defining_op) &&
          !mlir::getElementTypeOrSelf(value.getType()).isF32())
        return failure();
    }
  }

  for (auto& pair : cluster.constraints) {
    Value value = pair.getFirst();
    ValueConstraint constraint = pair.getSecond();

    // We can satisfy shape and rank constraints on the compiled function
    // operands.
    if (constraint == ValueConstraint::kRank ||
        constraint == ValueConstraint::kShape)
      continue;

    if (constraint == ValueConstraint::kValue &&
        tfrt::cpu::jit::SupportsValueSpecialization(value.getType()))
      continue;

    Operation* op = value.getDefiningOp();
    if (!op) return failure();  // we do not support block arguments

    // Operations defined inside the cluster will be constant folded before the
    // compilation. This property is guaranteed by the clustering policy.
    if (ops.contains(op)) continue;

    // Small constants will be sunk into the compiled function body.
    auto const_op = mlir::dyn_cast<mlir::TF::ConstOp>(op);
    if (!const_op || failed(IsCompilableConstant(const_op.value())))
      return failure();
  }

  return success();
}

}  // namespace tensorflow
