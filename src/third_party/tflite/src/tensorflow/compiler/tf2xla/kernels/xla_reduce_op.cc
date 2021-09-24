/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

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

#include "absl/algorithm/container.h"
#include "tensorflow/compiler/tf2xla/shape_util.h"
#include "tensorflow/compiler/tf2xla/xla_compiler.h"
#include "tensorflow/compiler/tf2xla/xla_op_kernel.h"
#include "tensorflow/compiler/tf2xla/xla_op_registry.h"
#include "tensorflow/compiler/xla/client/xla_builder.h"
#include "tensorflow/compiler/xla/client/xla_computation.h"
#include "tensorflow/core/framework/function.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/platform/errors.h"

namespace tensorflow {
namespace {

class XlaReduceOp : public XlaOpKernel {
 public:
  explicit XlaReduceOp(OpKernelConstruction* context) : XlaOpKernel(context) {
    OP_REQUIRES_OK(context, context->GetAttr("reducer", &reducer_));
    OP_REQUIRES_OK(context, context->GetAttr("dimensions_to_reduce",
                                             &dimensions_to_reduce_));
    std::set<int64_t> dims_set(dimensions_to_reduce_.begin(),
                               dimensions_to_reduce_.end());
    OP_REQUIRES(
        context, dims_set.size() == dimensions_to_reduce_.size(),
        errors::InvalidArgument("Duplicate dimension in dimensions_to_reduce "
                                "argument to XlaReduce"));
    if (context->HasAttr("N")) {  // variadic reduce
      use_tuples_ = true;
      OP_REQUIRES_OK(context, context->GetAttr("N", &n_));
    } else {
      use_tuples_ = false;
      n_ = 1;
    }
  }

  void Compile(XlaOpKernelContext* context) override {
    OP_REQUIRES(context, n_ * 2 == context->num_inputs(),
                errors::InvalidArgument("Expected ", n_ * 2, " inputs but got ",
                                        context->num_inputs()));

    const TensorShape input_shape = context->InputShape(0);
    const TensorShape init_value_shape = context->InputShape(n_);
    const DataType dtype = context->input_type(0);

    const int rank = input_shape.dims();
    OP_REQUIRES(context, TensorShapeUtils::IsScalar(init_value_shape),
                errors::InvalidArgument("init_value must be a scalar but got ",
                                        init_value_shape.DebugString()));

    auto dim_in_range = [rank](int64_t dim) { return dim >= 0 && dim < rank; };
    OP_REQUIRES(context,
                rank >= dimensions_to_reduce_.size() &&
                    absl::c_all_of(dimensions_to_reduce_, dim_in_range),
                errors::InvalidArgument(
                    "Invalid dimensions_to_reduce argument to XlaReduce"));

    // Build the reducer function.
    XlaCompiler::Argument reducer_arg;
    reducer_arg.kind = XlaCompiler::Argument::kParameter;
    reducer_arg.type = dtype;
    reducer_arg.shape = TensorShape();

    XlaCompiler::CompileOptions compile_options;
    compile_options.use_tuple_arg = false;
    compile_options.always_return_tuple = false;
    compile_options.is_entry_computation = false;
    XlaCompiler::CompilationResult reducer;
    OP_REQUIRES_OK(
        context,
        context->compiler()->CompileFunction(
            compile_options, *reducer_,
            std::vector<XlaCompiler::Argument>(n_ * 2, reducer_arg), &reducer));

    xla::Shape expected_shape;
    OP_REQUIRES_OK(
        context, TensorShapeToXLAShape(dtype, TensorShape(), &expected_shape));
    if (use_tuples_) {
      expected_shape = xla::ShapeUtil::MakeTupleShape(
          std::vector<xla::Shape>(n_, expected_shape));
    }
    OP_REQUIRES(
        context,
        xla::ShapeUtil::Compatible(reducer.xla_output_shape, expected_shape),
        errors::InvalidArgument(
            "Invalid output shape of XlaReduce reducer. Expected ",
            xla::ShapeUtil::HumanString(expected_shape), " got ",
            xla::ShapeUtil::HumanString(reducer.xla_output_shape)));

    std::vector<xla::XlaOp> inputs;
    std::vector<xla::XlaOp> inits;
    inputs.reserve(n_);
    inits.reserve(n_);
    for (int i = 0; i < n_; i++) {
      inputs.emplace_back(context->Input(i));
      inits.emplace_back(context->Input(n_ + i));
    }
    xla::XlaOp output =
        xla::Reduce(context->builder(), inputs, inits, *reducer.computation,
                    dimensions_to_reduce_);
    if (use_tuples_) {
      for (int i = 0; i < n_; i++) {
        context->SetOutput(i, xla::GetTupleElement(output, i));
      }
    } else {
      context->SetOutput(0, output);
    }
  }

 private:
  const NameAttrList* reducer_;
  std::vector<int64_t> dimensions_to_reduce_;
  bool use_tuples_;
  int n_;

  TF_DISALLOW_COPY_AND_ASSIGN(XlaReduceOp);
};

REGISTER_XLA_OP(Name("XlaReduce"), XlaReduceOp);
REGISTER_XLA_OP(Name("XlaVariadicReduce"), XlaReduceOp);

class XlaVariadicReduceOpV2 : public XlaOpKernel {
 public:
  explicit XlaVariadicReduceOpV2(OpKernelConstruction* context)
      : XlaOpKernel(context) {
    OP_REQUIRES_OK(context, context->GetAttr("T", &input_types_));
    OP_REQUIRES_OK(context, context->GetAttr("reducer", &reducer_));
    OP_REQUIRES_OK(context, context->GetAttr("dimensions_to_reduce",
                                             &dimensions_to_reduce_));
    std::set<int64_t> dims_set(dimensions_to_reduce_.begin(),
                               dimensions_to_reduce_.end());
    OP_REQUIRES(
        context, dims_set.size() == dimensions_to_reduce_.size(),
        errors::InvalidArgument("Duplicate dimension in dimensions_to_reduce "
                                "argument to XlaReduce"));
  }

  void Compile(XlaOpKernelContext* context) override {
    std::vector<xla::XlaOp> inputs;
    std::vector<TensorShape> input_shapes;
    OP_REQUIRES_OK(context,
                   context->InputList("inputs", &inputs, &input_shapes));
    int nr_inputs = inputs.size();

    std::vector<xla::XlaOp> inits;
    std::vector<TensorShape> init_shapes;
    OP_REQUIRES_OK(context,
                   context->InputList("init_values", &inits, &init_shapes));
    OP_REQUIRES(context, nr_inputs >= 1, errors::InvalidArgument("No inputs"));
    OP_REQUIRES(
        context, nr_inputs == inits.size(),
        errors::InvalidArgument("Number of inputs (", nr_inputs,
                                ") is different than number of init_values (",
                                inits.size(), ")"));

    const int rank = input_shapes[0].dims();
    for (int i = 0; i < nr_inputs; ++i) {
      if (i != 0) {
        OP_REQUIRES(
            context, input_shapes[i].IsSameSize(input_shapes[0]),
            errors::InvalidArgument("input[", i, "] has shape ",
                                    input_shapes[i].DebugString(),
                                    " different than the shape of input[0]: ",
                                    input_shapes[0].DebugString()));
      }
      OP_REQUIRES(context, TensorShapeUtils::IsScalar(init_shapes[i]),
                  errors::InvalidArgument("init_value[", i,
                                          "] must be a scalar but got ",
                                          init_shapes[i].DebugString()));
    }

    std::vector<XlaCompiler::Argument> reducer_args(2 * nr_inputs);
    std::vector<xla::Shape> expected_result_shapes(nr_inputs);

    for (int i = 0; i < nr_inputs; ++i) {
      XlaCompiler::Argument reducer_arg;
      reducer_arg.kind = XlaCompiler::Argument::kParameter;
      reducer_arg.type = input_types_[i];
      reducer_arg.shape = TensorShape();
      reducer_args[i] = reducer_arg;
      reducer_args[nr_inputs + i] = reducer_arg;
      OP_REQUIRES_OK(context,
                     TensorShapeToXLAShape(input_types_[i], TensorShape(),
                                           &expected_result_shapes[i]));
    }

    XlaCompiler::CompileOptions compile_options;
    compile_options.use_tuple_arg = false;
    compile_options.always_return_tuple = true;
    compile_options.is_entry_computation = false;
    XlaCompiler::CompilationResult reducer;
    OP_REQUIRES_OK(context,
                   context->compiler()->CompileFunction(
                       compile_options, *reducer_, reducer_args, &reducer));

    xla::Shape expected_shape =
        xla::ShapeUtil::MakeTupleShape(expected_result_shapes);
    OP_REQUIRES(
        context,
        xla::ShapeUtil::Compatible(reducer.xla_output_shape, expected_shape),
        errors::InvalidArgument(
            "Invalid output shape of XlaReduce reducer. Expected ",
            xla::ShapeUtil::HumanString(expected_shape), " got ",
            xla::ShapeUtil::HumanString(reducer.xla_output_shape)));

    auto dim_in_range = [rank](int64_t dim) { return dim >= 0 && dim < rank; };
    OP_REQUIRES(context,
                rank >= dimensions_to_reduce_.size() &&
                    absl::c_all_of(dimensions_to_reduce_, dim_in_range),
                errors::InvalidArgument(
                    "Invalid dimensions_to_reduce argument to XlaReduce"));

    xla::XlaOp output =
        xla::Reduce(context->builder(), inputs, inits, *reducer.computation,
                    dimensions_to_reduce_);
    for (int i = 0; i < nr_inputs; i++) {
      context->SetOutput(i, xla::GetTupleElement(output, i));
    }
  }

 private:
  DataTypeVector input_types_;
  const NameAttrList* reducer_;
  std::vector<int64_t> dimensions_to_reduce_;

  TF_DISALLOW_COPY_AND_ASSIGN(XlaVariadicReduceOpV2);
};

REGISTER_XLA_OP(Name("XlaVariadicReduceV2"), XlaVariadicReduceOpV2);

}  // namespace
}  // namespace tensorflow
