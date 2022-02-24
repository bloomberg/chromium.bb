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
#include "tensorflow/core/tfrt/graph_executor/graph_executor.h"

#include <string>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include "testing/base/public/gunit.h"
#include "tensorflow/cc/ops/array_ops.h"
#include "tensorflow/cc/ops/const_op.h"
#include "tensorflow/cc/ops/functional_ops.h"
#include "tensorflow/cc/ops/standard_ops.h"
#include "tensorflow/cc/ops/while_loop.h"
#include "tensorflow/core/framework/graph.pb.h"
#include "tensorflow/core/framework/tensor_testutil.h"
#include "tensorflow/core/framework/types.pb.h"
#include "tensorflow/core/grappler/utils/grappler_test.h"
#include "tensorflow/core/lib/core/status_test_util.h"
#include "tensorflow/core/platform/statusor.h"
#include "tensorflow/core/protobuf/rewriter_config.pb.h"
#include "tensorflow/core/tfrt/saved_model/saved_model_testutil.h"

namespace tensorflow {
namespace tfrt_stub {
namespace {

class GraphExecutorTest : public grappler::GrapplerTest {};

TEST_F(GraphExecutorTest, Vanilla) {
  GraphDef graph_def;
  {
    auto scope = tensorflow::Scope::NewRootScope().WithDevice("/device:CPU:0");

    auto input = ops::Placeholder(scope.WithOpName("input"), DT_INT32);
    auto rank = ops::Rank(scope.WithOpName("rank"), input);

    TF_ASSERT_OK(scope.ToGraphDef(&graph_def));
  }

  auto runtime = DefaultTfrtRuntime(/*num_threads=*/1);
  GraphExecutor::Options options(runtime.get());
  auto statusor_fallback_state = tensorflow::tfrt_stub::FallbackState::Create(
      CreateDefaultSessionOptions(options), graph_def.library());
  ASSERT_TRUE(statusor_fallback_state.ok());
  tensorflow::tfrt_stub::FallbackState* fallback_state =
      statusor_fallback_state.ValueOrDie().get();
  auto tpu_model_resource = std::make_unique<tfrt::tpu::TpuModelResource>();

  auto status_or_graph_executor = GraphExecutor::Create(
      std::move(options), *fallback_state, tpu_model_resource.get(), graph_def);
  ASSERT_TRUE(status_or_graph_executor.ok());
  GraphExecutor* graph_executor = status_or_graph_executor.ValueOrDie().get();

  // Set input 'x' to [[1, 1, 1]]
  std::vector<std::pair<std::string, tensorflow::Tensor>> inputs;
  inputs.push_back({"input", CreateTfTensor<int32_t>(
                                 /*shape=*/{1, 3}, /*data=*/{1, 1, 1})});

  std::vector<tensorflow::Tensor> outputs;

  TF_ASSERT_OK(graph_executor->Run(/*run_options=*/{}, inputs,
                                   /*output_tensor_names=*/{"rank"},
                                   /*target_tensor_names=*/{}, &outputs));
  ASSERT_EQ(outputs.size(), 1);

  EXPECT_THAT(GetTfTensorData<int32_t>(outputs[0]),
              ::testing::ElementsAreArray({2}));
}

}  // namespace
}  // namespace tfrt_stub
}  // namespace tensorflow
