/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

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

// XLA implementations of Random ops
// TODO(misard,phawkins): handle random number generator seeds/states correctly.
// TODO(misard,phawkins): add tests.

#include "tensorflow/compiler/tf2xla/kernels/gather_op_helpers.h"
#include "tensorflow/compiler/tf2xla/lib/broadcast.h"
#include "tensorflow/compiler/tf2xla/lib/random.h"
#include "tensorflow/compiler/tf2xla/lib/util.h"
#include "tensorflow/compiler/tf2xla/mlir_xla_op_kernel.h"
#include "tensorflow/compiler/tf2xla/shape_util.h"
#include "tensorflow/compiler/tf2xla/xla_helpers.h"
#include "tensorflow/compiler/tf2xla/xla_op_kernel.h"
#include "tensorflow/compiler/tf2xla/xla_op_registry.h"
#include "tensorflow/compiler/xla/client/lib/arithmetic.h"
#include "tensorflow/compiler/xla/client/lib/comparators.h"
#include "tensorflow/compiler/xla/client/lib/constants.h"
#include "tensorflow/compiler/xla/client/lib/dynamic_shaped_ops.h"
#include "tensorflow/compiler/xla/client/lib/loops.h"
#include "tensorflow/compiler/xla/client/value_inference.h"
#include "tensorflow/compiler/xla/client/xla_builder.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_shape.h"

namespace tensorflow {
namespace {

class RandomUniformOp : public XlaOpKernel {
 public:
  explicit RandomUniformOp(OpKernelConstruction* ctx) : XlaOpKernel(ctx) {}

  void Compile(XlaOpKernelContext* ctx) override {
    TensorShape shape;
    OP_REQUIRES_OK(ctx, ctx->ConstantInputAsShape(
                            0, &shape, xla::ValueInferenceMode::kUpperBound));

    const DataType dtype = output_type(0);
    xla::Shape xla_shape;
    OP_REQUIRES_OK(ctx, TensorShapeToXLAShape(dtype, shape, &xla_shape));

    xla::XlaBuilder* b = ctx->builder();
    LOG_FIRST_N(WARNING, 1)
        << "Warning: Using tf.random.uniform with XLA compilation will ignore "
           "seeds; consider using tf.random.stateless_uniform instead if "
           "reproducible behavior is desired. "
        << name();
    xla::XlaOp result = xla::RngUniform(XlaHelpers::Zero(b, dtype),
                                        XlaHelpers::One(b, dtype), xla_shape);
    auto result_status_or =
        SetAllDimensionSizes(&ctx->value_inference(), result, ctx->Input(0));
    OP_REQUIRES_OK(ctx, result_status_or.status());
    result = result_status_or.ValueOrDie();
    ctx->SetOutput(0, result);
  }

 private:
  TF_DISALLOW_COPY_AND_ASSIGN(RandomUniformOp);
};

REGISTER_XLA_OP(Name("RandomUniform").CompileTimeConstantInput("shape"),
                RandomUniformOp);

REGISTER_XLA_OP(Name("RandomShuffle"), MlirXlaOpKernel);

class RandomUniformIntOp : public XlaOpKernel {
 public:
  explicit RandomUniformIntOp(OpKernelConstruction* ctx) : XlaOpKernel(ctx) {}

  void Compile(XlaOpKernelContext* ctx) override {
    TensorShape shape;
    OP_REQUIRES_OK(ctx, ctx->ConstantInputAsShape(0, &shape));
    xla::Shape xla_shape;
    OP_REQUIRES_OK(ctx,
                   TensorShapeToXLAShape(input_type(1), shape, &xla_shape));

    const TensorShape minval_shape = ctx->InputShape(1);
    const TensorShape maxval_shape = ctx->InputShape(2);
    OP_REQUIRES(ctx, TensorShapeUtils::IsScalar(minval_shape),
                errors::InvalidArgument("minval must be 0-D, got shape ",
                                        minval_shape.DebugString()));
    OP_REQUIRES(ctx, TensorShapeUtils::IsScalar(maxval_shape),
                errors::InvalidArgument("maxval must be 0-D, got shape ",
                                        maxval_shape.DebugString()));

    auto minval = ctx->Input(1);
    auto maxval = ctx->Input(2);
    LOG_FIRST_N(WARNING, 1)
        << "Warning: Using tf.random.uniform with XLA compilation will ignore "
           "seeds; consider using tf.random.stateless_uniform instead if "
           "reproducible behavior is desired. "
        << name();
    ctx->SetOutput(0, xla::RngUniform(minval, maxval, xla_shape));
  }

 private:
  TF_DISALLOW_COPY_AND_ASSIGN(RandomUniformIntOp);
};

REGISTER_XLA_OP(Name("RandomUniformInt").CompileTimeConstantInput("shape"),
                RandomUniformIntOp);

class RandomStandardNormalOp : public XlaOpKernel {
 public:
  explicit RandomStandardNormalOp(OpKernelConstruction* ctx)
      : XlaOpKernel(ctx) {}

  void Compile(XlaOpKernelContext* ctx) override {
    const DataType dtype = output_type(0);

    TensorShape shape;
    OP_REQUIRES_OK(ctx, ctx->ConstantInputAsShape(
                            0, &shape, xla::ValueInferenceMode::kUpperBound));
    xla::Shape xla_shape;
    OP_REQUIRES_OK(ctx, TensorShapeToXLAShape(dtype, shape, &xla_shape));

    xla::XlaBuilder* b = ctx->builder();

    // Normal distribution with a mean of 0 and a standard deviation of 1:
    xla::XlaOp result = xla::RngNormal(XlaHelpers::Zero(b, dtype),
                                       XlaHelpers::One(b, dtype), xla_shape);
    auto result_status_or =
        SetAllDimensionSizes(&ctx->value_inference(), result, ctx->Input(0));
    OP_REQUIRES_OK(ctx, result_status_or.status());
    result = result_status_or.ValueOrDie();
    ctx->SetOutput(0, result);
  }

 private:
  TF_DISALLOW_COPY_AND_ASSIGN(RandomStandardNormalOp);
};

REGISTER_XLA_OP(Name("RandomStandardNormal").CompileTimeConstantInput("shape"),
                RandomStandardNormalOp);

class TruncatedNormalOp : public XlaOpKernel {
 public:
  explicit TruncatedNormalOp(OpKernelConstruction* ctx) : XlaOpKernel(ctx) {}

  void Compile(XlaOpKernelContext* ctx) override {
    const DataType dtype = output_type(0);

    TensorShape shape;
    OP_REQUIRES_OK(ctx, ctx->ConstantInputAsShape(0, &shape));
    xla::Shape xla_shape;
    OP_REQUIRES_OK(ctx, TensorShapeToXLAShape(dtype, shape, &xla_shape));

    xla::XlaBuilder* b = ctx->builder();

    xla::XlaOp one = xla::One(b, xla_shape.element_type());
    xla::XlaOp min_positive =
        xla::MinPositiveNormalValue(b, xla_shape.element_type());
    LOG_FIRST_N(WARNING, 1)
        << "Warning: Using tf.random.truncated_normal with XLA "
           "compilation will ignore seeds; consider using "
           "tf.random.stateless_truncated_normal instead if "
           "reproducible behavior is desired. "
        << name();
    auto uniform = xla::RngUniform(min_positive, one, xla_shape);
    ctx->SetOutput(0, TruncatedNormal(uniform));
  }
};

REGISTER_XLA_OP(Name("TruncatedNormal")
                    .CompileTimeConstantInput("shape")
                    .TypeConstraint("dtype", {DT_FLOAT, DT_DOUBLE}),
                TruncatedNormalOp);

// Broadcast a ParameterizedTruncatedNormal parameter to the output shape. If
// the parameter is a vector of shape [num_batches], then it is broadcast along
// dimension 0 to ([num_batches] x samples_per_batch). Otherwise it is a scalar
// or has shape [1], in which case the single value is broadcast.
static StatusOr<xla::XlaOp> BroadcastParameters(xla::XlaOp params,
                                                TensorShape& output_shape) {
  // broadcast to [samples1, ..., num_batches]
  int rank = output_shape.dims();
  std::vector<int64_t> bcast_shape;
  for (int i = 1; i < rank; ++i) {
    bcast_shape.push_back(output_shape.dim_size(i));
  }
  bcast_shape.push_back(output_shape.dim_size(0));
  TF_ASSIGN_OR_RETURN(xla::XlaOp bcast_params,
                      BroadcastTo(params, bcast_shape));

  // transpose to [num_batches, samples1, ...]
  std::vector<int64_t> permutation;
  permutation.push_back(rank - 1);
  for (int i = 0; i < rank - 1; ++i) {
    permutation.push_back(i);
  }
  return xla::Transpose(bcast_params, permutation);
}

class ParameterizedTruncatedNormalOp : public XlaOpKernel {
 public:
  explicit ParameterizedTruncatedNormalOp(OpKernelConstruction* ctx)
      : XlaOpKernel(ctx) {}

  void Compile(XlaOpKernelContext* ctx) override {
    const DataType dtype = output_type(0);

    TensorShape shape;
    OP_REQUIRES_OK(ctx, ctx->ConstantInputAsShape(0, &shape));
    xla::Shape xla_shape;
    OP_REQUIRES_OK(ctx, TensorShapeToXLAShape(dtype, shape, &xla_shape));
    OP_REQUIRES(ctx, xla_shape.rank() >= 1,
                errors::InvalidArgument(
                    "shape parameter must have rank >= 1, received (",
                    xla::ShapeUtil::HumanString(xla_shape), ")"));

    xla::XlaBuilder* b = ctx->builder();

    xla::XlaOp one = xla::One(b, xla_shape.element_type());
    xla::XlaOp min_positive =
        xla::MinPositiveNormalValue(b, xla_shape.element_type());
    LOG_FIRST_N(WARNING, 1)
        << "Warning: Using tf.random.truncated_normal with XLA "
           "compilation will ignore seeds; consider using "
           "tf.random.stateless_truncated_normal instead if "
           "reproducible behavior is desired. "
        << name();
    xla::XlaOp uniform = xla::RngUniform(min_positive, one, xla_shape);

    auto result = b->ReportErrorOrReturn([&]() -> StatusOr<xla::XlaOp> {
      TF_ASSIGN_OR_RETURN(xla::XlaOp means,
                          BroadcastParameters(ctx->Input(1), shape));
      TF_ASSIGN_OR_RETURN(xla::XlaOp stddevs,
                          BroadcastParameters(ctx->Input(2), shape));
      TF_ASSIGN_OR_RETURN(xla::XlaOp minvals,
                          BroadcastParameters(ctx->Input(3), shape));
      TF_ASSIGN_OR_RETURN(xla::XlaOp maxvals,
                          BroadcastParameters(ctx->Input(4), shape));
      return ParameterizedTruncatedNormal(uniform, means, stddevs, minvals,
                                          maxvals);
    });

    ctx->SetOutput(0, result);
  }
};

REGISTER_XLA_OP(Name("ParameterizedTruncatedNormal")
                    .CompileTimeConstantInput("shape")
                    .TypeConstraint("dtype", {DT_FLOAT, DT_DOUBLE}),
                ParameterizedTruncatedNormalOp);

}  // namespace
}  // namespace tensorflow
