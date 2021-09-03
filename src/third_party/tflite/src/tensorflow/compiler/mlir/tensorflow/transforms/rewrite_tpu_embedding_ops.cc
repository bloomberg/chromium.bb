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

#include "llvm/ADT/SmallVector.h"
#include "mlir/IR/Attributes.h"  // from @llvm-project
#include "mlir/IR/StandardTypes.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "mlir/Pass/PassRegistry.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.h"

namespace mlir {
namespace TF {

namespace {

// Rewrites RecvTPUEmbeddingActivationsOp and SendTPUEmbeddingGradients ops to
// internal variants by introducing _RecvTPUEmbeddingDeduplicationData op.
struct RewriteTPUEmbeddingOps
    : public PassWrapper<RewriteTPUEmbeddingOps, FunctionPass> {
  void runOnFunction() override;
};

// Rewrites the given op to `OpT` op after adding the given operand at the end.
template <typename OpT>
OpT AddOperandAndRewriteAs(Operation* op, Value operand, OpBuilder* builder) {
  builder->setInsertionPoint(op);
  auto operands = llvm::to_vector<4>(op->getOperands());
  operands.push_back(operand);
  auto new_op = builder->create<OpT>(op->getLoc(), op->getResultTypes(),
                                     operands, op->getAttrs());
  op->replaceAllUsesWith(new_op.getOperation()->getResults());
  op->erase();
  return new_op;
}

// Returns success if the function has at most one op of the template type and
// assigns it to `result`, if present. If there are multiple such ops, returns
// failure.
template <typename OpT>
LogicalResult GetOp(FuncOp func, OpT* result) {
  *result = {};
  for (auto op : func.getOps<OpT>()) {
    if (*result) return op.emitError("should be unique within a function");
    *result = op;
  }
  return success();
}

void RewriteTPUEmbeddingOps::runOnFunction() {
  FuncOp func = getFunction();

  RecvTPUEmbeddingActivationsOp recv_op;
  if (failed(GetOp(func, &recv_op))) return signalPassFailure();

  SendTPUEmbeddingGradientsOp send_op;
  if (failed(GetOp(func, &send_op))) return signalPassFailure();

  // No TPU embedding ops.
  if (!recv_op && !send_op) return;

  Location loc = recv_op ? recv_op.getLoc() : send_op.getLoc();
  StringRef config = recv_op ? recv_op.config() : send_op.config();

  // Create _RecvTPUEmbeddingDeduplicationData op.
  OpBuilder builder(func.getBody());
  auto output_ty = RankedTensorType::get({}, VariantType::get(&getContext()));
  auto dedup_op = builder.create<_RecvTPUEmbeddingDeduplicationDataOp>(
      loc, output_ty, config);

  // Rewrite RecvTPUEmbeddingActivations op to the corresponding internal op.
  if (recv_op)
    AddOperandAndRewriteAs<_RecvTPUEmbeddingActivationsOp>(recv_op, dedup_op,
                                                           &builder);

  // Rewrite SendTPUEmbeddingGradients op to the corresponding internal op and
  // then update the OperandSegmentSize attribute.
  if (send_op) {
    int32_t operand_sizes[] = {static_cast<int32_t>(send_op.N()),
                               static_cast<int32_t>(send_op.NN()), 1};
    auto attr_ty = VectorType::get(3, builder.getI32Type());
    auto operand_size_attr = DenseIntElementsAttr::get(attr_ty, operand_sizes);

    auto new_send_op = AddOperandAndRewriteAs<_SendTPUEmbeddingGradientsOp>(
        send_op, dedup_op, &builder);
    new_send_op.setAttr(new_send_op.getOperandSegmentSizeAttr(),
                        operand_size_attr);
  }
}

}  // anonymous namespace

std::unique_ptr<OperationPass<FuncOp>> CreateRewriteTPUEmbeddingOpsPass() {
  return std::make_unique<RewriteTPUEmbeddingOps>();
}

static PassRegistration<RewriteTPUEmbeddingOps> pass(
    "tf-rewrite-tpu-embedding-ops",
    "Rewrites TPU embedding send/recv ops by adding TPU embedding "
    "deduplication data");

}  // namespace TF
}  // namespace mlir
