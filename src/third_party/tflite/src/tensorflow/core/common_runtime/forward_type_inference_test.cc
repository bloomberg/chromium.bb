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

#include "tensorflow/core/common_runtime/forward_type_inference.h"

#include <gmock/gmock.h>
#include "tensorflow/cc/client/client_session.h"
#include "tensorflow/cc/framework/ops.h"
#include "tensorflow/cc/ops/standard_ops.h"
#include "tensorflow/core/common_runtime/graph_constructor.h"
#include "tensorflow/core/common_runtime/graph_runner.h"
#include "tensorflow/core/framework/full_type.pb.h"
#include "tensorflow/core/framework/node_def_util.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/tensor_shape.pb.h"
#include "tensorflow/core/framework/tensor_testutil.h"
#include "tensorflow/core/graph/graph_def_builder.h"
#include "tensorflow/core/lib/core/status_test_util.h"
#include "tensorflow/core/lib/strings/str_util.h"
#include "tensorflow/core/platform/test.h"

namespace tensorflow {
namespace {

Status Rewrite(std::unique_ptr<Graph>* graph) {
  FunctionLibraryDefinition flib_def((*graph)->flib_def());
  GraphOptimizationPassOptions opt_options;
  SessionOptions session_options;
  opt_options.session_options = &session_options;
  opt_options.graph = graph;
  opt_options.flib_def = &flib_def;
  ForwardTypeInferencePass pass;
  return pass.Run(opt_options);
}

TEST(ForwardTypeInferenceTest, BasicStraightline) {
  std::unique_ptr<Graph> graph(new Graph(OpRegistry::Global()));

  Scope root = Scope::NewRootScope().ExitOnError();

  auto start = ops::Placeholder(root.WithOpName("start"), DT_INT64);
  auto stop = ops::Placeholder(root.WithOpName("stop"), DT_INT64);
  auto step = ops::Placeholder(root.WithOpName("step"), DT_INT64);

  Node* ds;
  TensorShapeProto shape;
  shape.mutable_dim();
  shape.set_unknown_rank(false);
  TF_ASSERT_OK(NodeBuilder("ds", "RangeDataset", &root.graph()->flib_def())
                   .Input({NodeBuilder::NodeOut(start.node())})
                   .Input({NodeBuilder::NodeOut(stop.node())})
                   .Input({NodeBuilder::NodeOut(step.node())})
                   .Attr("output_types", {DT_INT32})
                   .Attr("output_shapes", {shape})
                   .Finalize(root.graph(), &ds));

  Node* id;
  TF_ASSERT_OK(NodeBuilder("id", "Identity", &root.graph()->flib_def())
                   .Input({NodeBuilder::NodeOut(ds)})
                   .Attr("T", DT_VARIANT)
                   .Finalize(root.graph(), &id));

  TF_ASSERT_OK(root.ToGraph(graph.get()));
  TF_ASSERT_OK(Rewrite(&graph));

  for (const auto& node : graph->nodes()) {
    if ((node->name() == "ds") || ((node->name() == "id"))) {
      const auto& t = node->def().experimental_type();
      EXPECT_EQ(t.type_id(), TFT_PRODUCT) << node->def().DebugString();
      EXPECT_EQ(t.args(0).type_id(), TFT_DATASET) << node->def().DebugString();
    }
  }
}

TEST(ForwardTypeInferenceTest, CyclicGraphWithV1ControlFlow) {
  std::unique_ptr<Graph> graph(new Graph(OpRegistry::Global()));

  Scope root = Scope::NewRootScope().ExitOnError();

  auto start = ops::Placeholder(root.WithOpName("start"), DT_INT64);
  auto stop = ops::Placeholder(root.WithOpName("stop"), DT_INT64);
  auto step = ops::Placeholder(root.WithOpName("step"), DT_INT64);
  auto cond = ops::Placeholder(root.WithOpName("cond"), DT_BOOL);

  Node* ds;
  TensorShapeProto shape;
  shape.mutable_dim();
  shape.set_unknown_rank(false);
  TF_ASSERT_OK(NodeBuilder("ds", "RangeDataset", &root.graph()->flib_def())
                   .Input({NodeBuilder::NodeOut(start.node())})
                   .Input({NodeBuilder::NodeOut(stop.node())})
                   .Input({NodeBuilder::NodeOut(step.node())})
                   .Attr("output_types", {DT_INT32})
                   .Attr("output_shapes", {shape})
                   .Finalize(root.graph(), &ds));

  Node* enter;
  TF_ASSERT_OK(NodeBuilder("enter", "Enter", &root.graph()->flib_def())
                   .Input({NodeBuilder::NodeOut(ds)})
                   .Attr("frame_name", "loop")
                   .Finalize(root.graph(), &enter));

  Node* loop_cond;
  TF_ASSERT_OK(NodeBuilder("loop_cond", "Enter", &root.graph()->flib_def())
                   .Input({NodeBuilder::NodeOut(cond.node())})
                   .Attr("frame_name", "loop")
                   .Finalize(root.graph(), &loop_cond));

  Node* merge;
  TF_ASSERT_OK(
      NodeBuilder("merge", "Merge", &root.graph()->flib_def())
          .Input({NodeBuilder::NodeOut(enter), NodeBuilder::NodeOut(enter)})
          .Finalize(root.graph(), &merge));

  Node* sw;
  TF_ASSERT_OK(NodeBuilder("sw", "Switch", &root.graph()->flib_def())
                   .Input({NodeBuilder::NodeOut(merge)})
                   .Input({NodeBuilder::NodeOut(loop_cond)})
                   .Finalize(root.graph(), &sw));

  Node* id;
  TF_ASSERT_OK(NodeBuilder("id", "Identity", &root.graph()->flib_def())
                   .Input({NodeBuilder::NodeOut(sw)})
                   .Finalize(root.graph(), &id));

  Node* next;
  TF_ASSERT_OK(NodeBuilder("next", "NextIteration", &root.graph()->flib_def())
                   .Input({NodeBuilder::NodeOut(id)})
                   .Finalize(root.graph(), &next));

  TF_ASSERT_OK(root.graph()->UpdateEdge(next, 0, merge, 1));

  Node* exit;
  TF_ASSERT_OK(NodeBuilder("exit", "Exit", &root.graph()->flib_def())
                   .Input({NodeBuilder::NodeOut(sw)})
                   .Finalize(root.graph(), &exit));

  TF_ASSERT_OK(root.ToGraph(graph.get()));
  TF_ASSERT_OK(Rewrite(&graph));

  for (const auto& node : graph->nodes()) {
    if ((node->name() == "ds") || (node->name() == "id") ||
        (node->name() == "enter") || (node->name() == "exit") ||
        (node->name() == "sw") || (node->name() == "merge") ||
        (node->name() == "next")) {
      const auto& t = node->def().experimental_type();
      ASSERT_EQ(t.type_id(), TFT_PRODUCT) << node->def().DebugString();
      EXPECT_EQ(t.args(0).type_id(), TFT_DATASET) << node->def().DebugString();
    }
  }
}

REGISTER_OP("TestSourceOp").Output("o: variant");

REGISTER_OP("TestTensorUnaryOp")
    .Input("i: variant")
    .Output("o: variant")
    .SetForwardTypeFn(
        [](const std::vector<std::reference_wrapper<const FullTypeDef>>&
               input_types) {
          FullTypeDef t;
          t.set_type_id(TFT_PRODUCT);
          t.add_args()->set_type_id(TFT_TENSOR);
          return t;
        });

REGISTER_OP("TestArrayUnaryOp")
    .Input("i: variant")
    .Output("o: variant")
    .SetForwardTypeFn(
        [](const std::vector<std::reference_wrapper<const FullTypeDef>>&
               input_types) {
          FullTypeDef t;
          t.set_type_id(TFT_PRODUCT);
          t.add_args()->set_type_id(TFT_ARRAY);
          return t;
        });

REGISTER_OP("TestMergeOp")
    .Input("i1: variant")
    .Input("i2: variant")
    .Output("o: variant")
    .SetForwardTypeFn(
        [](const std::vector<std::reference_wrapper<const FullTypeDef>>&
               input_types) {
          EXPECT_EQ(input_types.size(), 2);
          FullTypeDef t;
          t.set_type_id(TFT_PRODUCT);
          if ((input_types[0].get().type_id() == TFT_TENSOR) &&
              (input_types[1].get().type_id() == TFT_ARRAY)) {
            t.add_args()->set_type_id(TFT_ARRAY);
          } else {
            t.add_args()->set_type_id(TFT_TENSOR);
          }
          return t;
        });

TEST(ForwardTypeInferenceTest,
     BinaryNodeWithUnbalancedPathsFromCommonAncestor) {
  std::unique_ptr<Graph> graph(new Graph(OpRegistry::Global()));

  Scope root = Scope::NewRootScope().ExitOnError();

  Node* s;
  TF_ASSERT_OK(NodeBuilder("s", "TestSourceOp", &root.graph()->flib_def())
                   .Finalize(root.graph(), &s));

  Node* tn;
  TF_ASSERT_OK(NodeBuilder("tn", "TestTensorUnaryOp", &root.graph()->flib_def())
                   .Input({NodeBuilder::NodeOut(s)})
                   .Finalize(root.graph(), &tn));

  Node* id;
  TF_ASSERT_OK(NodeBuilder("id", "Identity", &root.graph()->flib_def())
                   .Input({NodeBuilder::NodeOut(s)})
                   .Finalize(root.graph(), &id));

  Node* an;
  TF_ASSERT_OK(NodeBuilder("an", "TestArrayUnaryOp", &root.graph()->flib_def())
                   .Input({NodeBuilder::NodeOut(id)})
                   .Finalize(root.graph(), &an));

  // This node has an unbalanced path from s, and its type inference can produce
  // different results if the ancestors have incomplete type information.
  // This test is designed that the more complete type inference still takes
  // place, even though the node would be first visited with incomplete type
  // info under a naive BFS walk.
  Node* m;
  TF_ASSERT_OK(NodeBuilder("m", "TestMergeOp", &root.graph()->flib_def())
                   .Input({NodeBuilder::NodeOut(tn)})
                   .Input({NodeBuilder::NodeOut(an)})
                   .Finalize(root.graph(), &m));

  TF_ASSERT_OK(root.ToGraph(graph.get()));
  TF_ASSERT_OK(Rewrite(&graph));

  for (const auto& node : graph->nodes()) {
    if (node->name() == "m") {
      const auto& t = node->def().experimental_type();
      ASSERT_EQ(t.type_id(), TFT_PRODUCT) << node->def().DebugString();
      // We want complete type info (ARRAY), not incomplete one (TENSOR).
      EXPECT_EQ(t.args(0).type_id(), TFT_ARRAY) << node->def().DebugString();
    }
  }
}

TEST(ForwardTypeInferenceTest, BinaryNodeWithCycleInput) {
  std::unique_ptr<Graph> graph(new Graph(OpRegistry::Global()));

  Scope root = Scope::NewRootScope().ExitOnError();

  auto cond = ops::Placeholder(root.WithOpName("cond"), DT_BOOL);

  Node* s;
  TF_ASSERT_OK(NodeBuilder("s", "TestSourceOp", &root.graph()->flib_def())
                   .Finalize(root.graph(), &s));

  Node* an;
  TF_ASSERT_OK(NodeBuilder("an", "TestArrayUnaryOp", &root.graph()->flib_def())
                   .Input({NodeBuilder::NodeOut(s)})
                   .Finalize(root.graph(), &an));

  Node* enter;
  TF_ASSERT_OK(NodeBuilder("enter", "Enter", &root.graph()->flib_def())
                   .Input({NodeBuilder::NodeOut(an)})
                   .Attr("frame_name", "loop")
                   .Finalize(root.graph(), &enter));

  Node* loop_cond;
  TF_ASSERT_OK(NodeBuilder("loop_cond", "Enter", &root.graph()->flib_def())
                   .Input({NodeBuilder::NodeOut(cond.node())})
                   .Attr("frame_name", "loop")
                   .Finalize(root.graph(), &loop_cond));

  // TODO(mdan): Is there any way to create a cycle without using a Merge node?
  Node* merge;
  TF_ASSERT_OK(
      NodeBuilder("merge", "Merge", &root.graph()->flib_def())
          .Input({NodeBuilder::NodeOut(enter), NodeBuilder::NodeOut(enter)})
          .Finalize(root.graph(), &merge));

  Node* sw;
  TF_ASSERT_OK(NodeBuilder("sw", "Switch", &root.graph()->flib_def())
                   .Input({NodeBuilder::NodeOut(merge)})
                   .Input({NodeBuilder::NodeOut(loop_cond)})
                   .Finalize(root.graph(), &sw));

  Node* tn;
  TF_ASSERT_OK(NodeBuilder("tn", "TestTensorUnaryOp", &root.graph()->flib_def())
                   .Input({NodeBuilder::NodeOut(sw)})
                   .Finalize(root.graph(), &tn));

  Node* next;
  TF_ASSERT_OK(NodeBuilder("next", "NextIteration", &root.graph()->flib_def())
                   .Input({NodeBuilder::NodeOut(tn)})
                   .Finalize(root.graph(), &next));

  TF_ASSERT_OK(root.graph()->UpdateEdge(next, 0, merge, 1));

  Node* exit;
  TF_ASSERT_OK(NodeBuilder("exit", "Exit", &root.graph()->flib_def())
                   .Input({NodeBuilder::NodeOut(sw)})
                   .Finalize(root.graph(), &exit));

  TF_ASSERT_OK(root.ToGraph(graph.get()));
  const auto& status = Rewrite(&graph);
  ASSERT_FALSE(status.ok());

  // We always expect the merge node to raise a type inference error, because
  // the type in the loop (TENSOR, from TestTensorUnaryOp) doesn't match the
  // type upon loop entry (ARRAY, from TestArrayUnaryOp).
  // This error is only raised when both types are resolved, otherwise Merge
  // will deduct the type from its partial inputs.
  // In effect, the assertion verifies that the merge node is always visited
  // at least once after both its inputs have been resolved, so the graph always
  // has complete type information.
  EXPECT_THAT(status.error_message(),
              ::testing::HasSubstr("expected identical input types"));
}

}  // namespace
}  // namespace tensorflow
