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

#include "tensorflow/compiler/mlir/tensorflow/utils/dump_graph.h"

#include "tensorflow/core/graph/graph.h"
#include "tensorflow/core/graph/node_builder.h"
#include "tensorflow/core/lib/core/status_test_util.h"
#include "tensorflow/core/lib/io/path.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/test.h"
#include "tensorflow/core/util/dump_graph.h"

namespace tensorflow {
namespace {

void ExpectHasSubstr(const string& s, const string& expected) {
  EXPECT_TRUE(absl::StrContains(s, expected))
      << "'" << s << "' does not contain '" << expected << "'";
}

// WritableFile that simply concats into string.
class StringWritableFile : public WritableFile {
 public:
  explicit StringWritableFile(string* str) : str_(*str) {}

  Status Append(StringPiece data) override {
    absl::StrAppend(&str_, data);
    return Status::OK();
  }

  Status Close() override { return Status::OK(); }

  Status Flush() override { return Status::OK(); }

  Status Name(StringPiece* result) const override {
    *result = "(string)";
    return Status::OK();
  }

  Status Sync() override { return Status::OK(); }

  Status Tell(int64* position) override {
    return errors::Unimplemented("Stream not seekable");
  }

 private:
  string& str_;
};

TEST(Dump, TexualIrToFileSuccess) {
  Graph graph(OpRegistry::Global());
  Node* node;
  TF_CHECK_OK(NodeBuilder("A", "NoOp").Finalize(&graph, &node));

  setenv("TF_DUMP_GRAPH_PREFIX", testing::TmpDir().c_str(), 1);
  UseMlirForGraphDump(MlirDumpConfig());
  string ret = DumpGraphToFile("tir", graph);
  ASSERT_EQ(ret, io::JoinPath(testing::TmpDir(), "tir.mlir"));

  string actual;
  TF_ASSERT_OK(ReadFileToString(Env::Default(), ret, &actual));
  string expected_substr = R"(tf_executor.island)";
  ExpectHasSubstr(actual, expected_substr);
}

TEST(Dump, TexualIrWithOptions) {
  Graph graph(OpRegistry::Global());
  Node* node;
  TF_ASSERT_OK(NodeBuilder("A", "Placeholder")
                   .Attr("dtype", DT_FLOAT)
                   .Finalize(&graph, &node));

  string actual;
  StringWritableFile file(&actual);
  TF_ASSERT_OK(DumpTextualIRToFile(MlirDumpConfig().emit_location_information(),
                                   graph, /*flib_def=*/nullptr, &file));

  string expected_substr = R"(loc("A"))";
  ExpectHasSubstr(actual, expected_substr);
}

}  // namespace
}  // namespace tensorflow
