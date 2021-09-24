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

#include "tensorflow/compiler/tf2xla/mlir_xla_op_kernel.h"

#include "tensorflow/compiler/jit/xla_compilation_cache.h"
#include "tensorflow/compiler/mlir/tensorflow/utils/compile_mlir_util.h"
#include "tensorflow/compiler/mlir/utils/array_container_utils.h"

namespace tensorflow {

namespace {

Status ContextToXlaArgs(XlaOpKernelContext* ctx,
                        std::vector<XlaCompiler::Argument>& xla_args) {
  int num_inputs = ctx->num_inputs();
  xla_args.reserve(num_inputs);
  for (int i = 0; i < num_inputs; ++i) {
    // TODO(b/180448676): If the input `XlaExpression` kind is `kConstant`, then
    // create a constant `XlaArgument`.
    // TODO(b/180448774): Handle kResource and kTensorList.
    XlaExpression::Kind ctx_kind_i = ctx->InputExpression(i).kind();
    if (ctx_kind_i != XlaExpression::Kind::kXlaOp &&
        ctx_kind_i != XlaExpression::Kind::kConstant)
      return tensorflow::errors::InvalidArgument(
          absl::StrCat("Input ", i, " to an MlirXlaOpKernel is invalid: ",
                       ctx->InputExpression(i).HumanString()));
    XlaCompiler::Argument arg;
    arg.kind = XlaCompiler::Argument::kParameter;
    arg.type = ctx->input_type(i);
    arg.shape = ctx->InputXlaShape(i).ValueOrDie();
    arg.name = absl::StrCat("_arg", i);
    xla_args.push_back(arg);
  }
  return Status::OK();
}

}  // namespace

Status MlirXlaOpKernel::ConstructXlaOp(XlaOpKernelContext* ctx) {
  // Create input XlaArguments.
  std::vector<XlaCompiler::Argument> xla_args;
  TF_RETURN_IF_ERROR(ContextToXlaArgs(ctx, xla_args));

  // Create input XlaOps.
  llvm::SmallVector<xla::XlaOp, 4> xla_params(ctx->num_inputs());
  for (int i = 0, end = ctx->num_inputs(); i < end; ++i) {
    xla_params[i] = ctx->Input(i);
  }

  // Create outputs.
  std::vector<DataType> result_dtypes(ctx->num_outputs());
  for (int i = 0, end = result_dtypes.size(); i < end; ++i) {
    result_dtypes[i] = ctx->expected_output_dtype(i);
  }

  // When there are no data-flow outputs from the node, the node is used as a
  // control output by the graph to TensorflowDialect importer.
  std::vector<std::string> control_rets;
  if (result_dtypes.empty()) {
    control_rets.push_back(def().name());
  }

  // Get the context's device.
  auto device = dynamic_cast<Device*>(ctx->op_kernel_context()->device());
  if (!device) {
    return tensorflow::errors::InvalidArgument(
        "Expected the XlaOpKernelContext argument's device to have type "
        "Device.");
  }

  // Create a graph that wraps the kernel.
  TF_ASSIGN_OR_RETURN(auto graph, CreateGraph(def(), xla_args, result_dtypes));

  // Compile the graph to HLO.
  GraphDebugInfo debug_info;
  std::vector<xla::XlaOp> returns(1);
  TF_RETURN_IF_ERROR(BuildHloFromGraph(
      *graph, *ctx->builder(), xla_params, returns,
      mlir::SpanToArrayRef<XlaCompiler::Argument>(xla_args), control_rets,
      device->device_type(),
      *ctx->function_library()->GetFunctionLibraryDefinition(), debug_info,
      {}));

  // Set context outputs.
  for (int i = 0, end = returns.size(); i < end; ++i) {
    ctx->SetOutput(i, returns[i]);
  }

  return Status::OK();
}

void MlirXlaOpKernel::Compile(XlaOpKernelContext* ctx) {
  auto status = ConstructXlaOp(ctx);
  if (!status.ok()) {
    errors::AppendToMessage(&status, "Failure to legalize ", def().name(),
                            " using MlirXlaOpKernel in the tf2xla bridge.");
  }
  OP_REQUIRES_OK(ctx, status);
}

}  // namespace tensorflow
