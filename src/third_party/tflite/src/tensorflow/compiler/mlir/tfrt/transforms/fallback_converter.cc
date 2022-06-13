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
#include "tensorflow/compiler/mlir/tfrt/transforms/fallback_converter.h"

#include "tensorflow/compiler/mlir/tensorflow/ir/tf_types.h"
#include "tensorflow/core/runtime_fallback/opdefs/tfrt_fallback.h"
#include "tensorflow/core/runtime_fallback/opdefs/tfrt_fallback_async.h"
#include "tfrt/basic_kernels/opdefs/types.h"  // from @tf_runtime
#include "tfrt/core_runtime/opdefs/types.h"  // from @tf_runtime

namespace tensorflow {
namespace tfrt_compiler {
namespace {
constexpr char kCpuDeviceName[] =
    "/job:localhost/replica:0/task:0/device:CPU:0";
}

FallbackConverter::FallbackConverter(mlir::MLIRContext *context)
    : builder_(context) {
  addConversion([](tfrt::compiler::ChainType type) { return type; });
  addConversion([](tfrt::fallback::TFTensorType type) { return type; });
  addConversion([=](mlir::TensorType type) -> llvm::Optional<mlir::Type> {
    // Ref types are not supported in both compiler and runtime.
    if (type.getElementType().isa<mlir::TF::TensorFlowRefType>()) {
      return llvm::None;
    }

    return builder_.getType<tfrt::fallback::TFTensorType>();
  });
  addConversion([=](mlir::Type type) -> llvm::Optional<mlir::Type> {
    if (type == builder_.getI1Type()) return type;
    return llvm::None;
  });
}

mlir::Value ConvertCoreRTTensorHandleToFallbackTensor(
    mlir::Location loc, llvm::StringRef device, mlir::Value value,
    mlir::ConversionPatternRewriter &rewriter) {
  if (value.getType().isa<tfrt::fallback::TFTensorType>()) return value;

  if (!value.getType().isa<tfrt::corert::TensorHandleType>()) return {};

  mlir::OpBuilder::InsertionGuard guard(rewriter);

  auto *def = value.getDefiningOp();
  if (def) {
    rewriter.setInsertionPointAfter(def);
  } else {
    rewriter.setInsertionPointToStart(value.getParentBlock());
  }

  return rewriter
      .create<tfrt::fallback_async::CoreRTTensorHandleToFallbackTensorOp>(
          loc, rewriter.getType<tfrt::fallback::TFTensorType>(), value, device)
      .getResult(0);
}

mlir::Value ConvertFallbackTensorToCoreRTTensorHandle(
    mlir::Location loc, mlir::Value value,
    mlir::ConversionPatternRewriter &rewriter) {
  if (value.getType().isa<tfrt::corert::TensorHandleType>()) return value;

  if (!value.getType().isa<tfrt::fallback::TFTensorType>()) return {};

  // Use CPU device by default if no device is specified.
  std::string device = kCpuDeviceName;
  if (auto *def = value.getDefiningOp()) {
    if (auto device_attr = def->getAttrOfType<mlir::StringAttr>("device")) {
      // NOTE: The TPU_SYSTEM check is just a short term workaround. The long
      // term solution should be checking the HostMemory annotation of the
      // defining op (it should be defined in TF OpKernel). If HostMemory
      // annotation is set for an output tensor, we should use CPU device here.
      // TODO(b/200896904): Support HostMemory annotation.
      if (!device_attr.getValue().endswith("TPU_SYSTEM:0")) {
        device = device_attr.getValue().str();
      }
    }
  }

  return rewriter
      .create<tfrt::fallback_async::FallbackTensorToCoreRTTensorHandleOp>(
          loc, rewriter.getType<tfrt::corert::TensorHandleType>(), value,
          device)
      .getResult(0);
}

mlir::LogicalResult ConvertCoreRTOperands(
    mlir::Operation *op, mlir::ValueRange operands,
    llvm::SmallVectorImpl<mlir::Value> *new_operands,
    mlir::ConversionPatternRewriter &rewriter) {
  mlir::OpBuilder::InsertionGuard guard(rewriter);
  // Insert before the current op.
  rewriter.setInsertionPoint(op);

  for (auto operand : operands) {
    auto value = ConvertFallbackTensorToCoreRTTensorHandle(op->getLoc(),
                                                           operand, rewriter);
    if (!value) {
      return op->emitWarning("failed to convert to !corert.tensorhandle")
             << operand.getType();
    }

    new_operands->push_back(value);
  }
  return success();
}

mlir::LogicalResult ConvertFallbackOperands(
    mlir::Operation *op, llvm::StringRef device, mlir::ValueRange operands,
    llvm::SmallVectorImpl<mlir::Value> *new_operands,
    mlir::ConversionPatternRewriter &rewriter) {
  for (auto operand : operands) {
    if (!operand.getType().isa<tfrt::fallback::TFTensorType>()) {
      auto new_operand = ConvertCoreRTTensorHandleToFallbackTensor(
          op->getLoc(), device, operand, rewriter);
      if (!new_operand)
        return op->emitWarning(
            "failed to convert the operand to fallback tensor.");
      new_operands->push_back(new_operand);
    } else {
      new_operands->push_back(operand);
    }
  }
  return success();
}

}  // namespace tfrt_compiler
}  // namespace tensorflow
