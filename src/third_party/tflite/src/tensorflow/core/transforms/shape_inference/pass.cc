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

#include "tensorflow/core/transforms/shape_inference/pass.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "mlir/IR/BuiltinTypes.h"  // from @llvm-project
#include "mlir/IR/Operation.h"  // from @llvm-project
#include "mlir/IR/Value.h"  // from @llvm-project
#include "mlir/Interfaces/InferTypeOpInterface.h"  // from @llvm-project
#include "mlir/Support/LLVM.h"  // from @llvm-project
#include "tensorflow/core/framework/shape_inference.h"
#include "tensorflow/core/ir/ops.h"
#include "tensorflow/core/ir/tf_op_wrapper.h"
#include "tensorflow/core/ir/types/dialect.h"
#include "tensorflow/core/ir/utils/shape_inference_utils.h"
#include "tensorflow/core/transforms/pass_detail.h"

namespace mlir {
namespace tfg {

using tensorflow::shape_inference::DimensionHandle;
using tensorflow::shape_inference::InferenceContext;
using tensorflow::shape_inference::ShapeHandle;

// Only non-static shape or type with subtype can be refined.
static bool CanBeRefined(Type type) {
  auto shape_type = type.dyn_cast<ShapedType>();
  if (!shape_type) return false;

  // Returns whether type with subtypes can be further refined.
  auto can_refine_subtypes = [](tf_type::TensorFlowTypeWithSubtype tws) {
    return tws.GetSubtypes().empty() ||
           llvm::any_of(tws.GetSubtypes(), CanBeRefined);
  };
  auto type_with_subtype = shape_type.getElementType()
                               .dyn_cast<tf_type::TensorFlowTypeWithSubtype>();
  if (type_with_subtype && can_refine_subtypes(type_with_subtype)) return true;

  return !shape_type.hasStaticShape();
}

static bool CanBeRefined(Operation *op) {
  return llvm::any_of(op->getResultTypes(),
                      static_cast<bool (*)(Type)>(CanBeRefined));
}

class ShapeInference : public ShapeInferenceBase<ShapeInference> {
 public:
  void runOnOperation() override;

 private:
  // Cache the tensor value if possible. After inferring the shape, some
  // operations may also be able to construct the tensor value, e.g., an
  // IdentityOp with Const operand, in these cases, cache the tensor value which
  // may be useful for their users' shape inference.
  void TryToCacheResultsTensorValue(Operation *op);

  // Get the tensor value if possible, return nullptr otherwise.
  DenseElementsAttr GetTensorValue(Value result) {
    OpResult op_result = result.dyn_cast<OpResult>();
    if (op_result) {
      auto it = cached_tensor_values_.find(op_result);
      if (it != cached_tensor_values_.end()) return it->second;
    }
    return nullptr;
  }

  DenseMap<OpResult, DenseElementsAttr> cached_tensor_values_;
};

void ShapeInference::TryToCacheResultsTensorValue(Operation *op) {
  // Only op with static shape is able to construct the tensor value.
  if (llvm::all_of(op->getResults().drop_back(), [this](Value value) {
        auto shape = value.getType().cast<ShapedType>();
        /// NOMUTANTS -- shape.hasStaticShape is a cheaper operation than
        /// GetTensorValue
        return (!shape.hasStaticShape() || GetTensorValue(value) != nullptr);
      })) {
    return;
  }

  StringRef op_name = op->getName().stripDialect();
  if (op_name == "Const") {
    cached_tensor_values_[op->getResult(0)] =
        op->getAttrOfType<DenseElementsAttr>("value");
  } else if (op_name == "Identity" ||
             (op_name == "IdentityN" &&
              TFOp(op).getNonControlOperands().size() == 1)) {
    DenseElementsAttr operand_tensor_value = GetTensorValue(op->getOperand(0));
    if (!operand_tensor_value) return;
    cached_tensor_values_[op->getResult(0)] = operand_tensor_value;
  } else if (op_name == "Rank") {
    ShapedType operand_shape = op->getOperand(0).getType().cast<ShapedType>();
    if (!operand_shape.hasRank()) return;
    ShapedType return_shape = op->getResultTypes()[0];
    DenseElementsAttr tensor_value;
    if (return_shape.getElementType().isInteger(32)) {
      tensor_value = DenseElementsAttr::get(
          op->getResultTypes()[0], ArrayRef<int>(operand_shape.getRank()));
    } else {
      tensor_value = DenseElementsAttr::get(
          op->getResultTypes()[0], ArrayRef<int64_t>(operand_shape.getRank()));
    }
    cached_tensor_values_[op->getResult(0)] = tensor_value;
  } else if (op_name == "Size") {
    ShapedType operand_shape = op->getOperand(0).getType().cast<ShapedType>();
    if (!operand_shape.hasStaticShape()) return;
    ShapedType return_shape = op->getResultTypes()[0];
    DenseElementsAttr tensor_value;
    if (return_shape.getElementType().isInteger(32)) {
      tensor_value =
          DenseElementsAttr::get(op->getResultTypes()[0],
                                 ArrayRef<int>(operand_shape.getNumElements()));
    } else {
      tensor_value = DenseElementsAttr::get(
          op->getResultTypes()[0],
          ArrayRef<int64_t>(operand_shape.getNumElements()));
    }
    cached_tensor_values_[op->getResult(0)] = tensor_value;
  } else if (op_name == "Shape" || op_name == "ShapeN") {
    for (OpOperand &operand : op->getOpOperands()) {
      ShapedType operand_shape = operand.get().getType().cast<ShapedType>();
      if (!operand_shape.hasStaticShape()) continue;
      int idx = operand.getOperandNumber();
      ShapedType return_shape = op->getResultTypes()[idx];
      DenseElementsAttr tensor_value;
      if (return_shape.getElementType().isInteger(32)) {
        tensor_value = DenseElementsAttr::get<int>(
            op->getResultTypes()[idx],
            SmallVector<int>(llvm::map_range(
                operand_shape.getShape(),
                [](int64_t dim) { return static_cast<int>(dim); })));
      } else {
        tensor_value = DenseElementsAttr::get(op->getResultTypes()[idx],
                                              operand_shape.getShape());
      }
      cached_tensor_values_[op->getResult(idx)] = tensor_value;
    }
  }

  // TODO(chiahungduan): In Grappler, it has cases for
  // ConcatV2/Pack/Slice/StrideSlice which has their shape inference logic
  // similar to how we do constant folding on them. I think constant folding
  // will cover most of the cases. Handle them on demand later.
}

void ShapeInference::runOnOperation() {
  auto operand_as_constant_fn = [this](Value operand) -> Attribute {
    return GetTensorValue(operand);
  };

  auto op_result_as_shape_fn = [this](InferenceContext &ic,
                                      OpResult op_result) -> ShapeHandle {
    auto rt = op_result.getType().dyn_cast<RankedTensorType>();
    // NOMUTANTS -- TODO(chiahungduan): Review this condition to see if shape
    // with known rank but unknown dimension is acceptable.
    if (!rt || rt.getRank() != 1 || !rt.hasStaticShape()) return {};

    std::vector<DimensionHandle> dims(rt.getDimSize(0), ic.UnknownDim());

    DenseElementsAttr attr = GetTensorValue(op_result);
    if (!attr) return {};

    for (const auto &element : llvm::enumerate(attr.getValues<APInt>()))
      dims[element.index()] = ic.MakeDim(element.value().getSExtValue());
    return ic.MakeShape(dims);
  };

  auto infer_and_update_shapes = [&](Operation *op) -> bool {
    auto result_element_type_fn = [&](int idx) -> Type {
      return op->getResult(idx).getType().cast<ShapedType>().getElementType();
    };

    SmallVector<ShapedTypeComponents> results;
    if (failed(InferReturnTypeComponentsForTFOp(
            op->getLoc(), op, TFOp(op).getNonControlOperands(), graph_version_,
            operand_as_constant_fn, op_result_as_shape_fn,
            result_element_type_fn,
            /*get_attr_values_fn=*/nullptr, results)))
      return false;

    bool updated = false;
    for (auto it : llvm::zip(op->getResults().drop_back(), results)) {
      OpResult op_result = std::get<0>(it);
      ShapedTypeComponents result = std::get<1>(it);
      TensorType inferred_type;
      if (result.hasRank()) {
        inferred_type =
            RankedTensorType::get(result.getDims(), result.getElementType());
      } else {
        inferred_type = UnrankedTensorType::get(result.getElementType());
      }

      inferred_type = tf_type::GetCastCompatibleType(
                          op_result.getType().cast<ShapedType>(), inferred_type)
                          .cast<TensorType>();

      if (inferred_type == op_result.getType()) continue;

      op_result.setType(inferred_type);
      updated = true;
    }

    if (updated) TryToCacheResultsTensorValue(op);

    return updated;
  };

  // Traverse all the operations and do the first time inference. We don't
  // record any operations that need to be updated because most of them may lack
  // shape information.
  getOperation()->walk([&](Operation *op) {
    if (isa<ModuleOp, GraphOp, GraphFuncOp>(op) || op->getNumResults() == 0)
      return;
    if (!CanBeRefined(op)) {
      TryToCacheResultsTensorValue(op);
      return;
    }
    (void)infer_and_update_shapes(op);
  });

  // It's possible that we enqueue the same operations multiple times because of
  // every shape update of its operands. Given that the cases won't be too much
  // in general, we don't optimize it for now.
  SmallVector<Operation *> may_need_update;

  // Collect operations that have the chance to infer the more precise shape
  // information.
  getOperation()->walk([&](Operation *op) {
    if (isa<ModuleOp, tfg::GraphOp, tfg::GraphFuncOp>(op) ||
        op->getNumResults() == 0)
      return;
    for (OpResult res : op->getResults().drop_back()) {
      for (Operation *user : res.getUsers())
        if (CanBeRefined(user)) may_need_update.push_back(user);
    }
  });

  // TODO(chiahungduan): We may need to limit the iterations.
  while (!may_need_update.empty()) {
    Operation *op = may_need_update.pop_back_val();
    bool updated = infer_and_update_shapes(op);
    if (!updated) continue;

    // The users may be able to refine their shapes.
    for (Value v : op->getResults().drop_back()) {
      for (Operation *user : v.getUsers()) {
        if (CanBeRefined(user)) may_need_update.push_back(user);
      }
    }
  }
}

std::unique_ptr<Pass> CreateShapeInferencePass() {
  return std::make_unique<ShapeInference>();
}

}  // namespace tfg
}  // namespace mlir
