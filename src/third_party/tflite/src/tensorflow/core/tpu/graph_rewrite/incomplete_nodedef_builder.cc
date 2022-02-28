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

#include "tensorflow/core/tpu/graph_rewrite/incomplete_nodedef_builder.h"

#include "tensorflow/compiler/xla/status_macros.h"
#include "tensorflow/core/common_runtime/function.h"

namespace tensorflow {

IncompleteNodeDefBuilder::IncompleteNodeDefBuilder(const string& name,
                                                   const string& op,
                                                   const NodeDebugInfo& debug) {
  nodedef_.set_name(name);
  nodedef_.set_op(op);
  MergeDebugInfo(debug, &nodedef_);
}

IncompleteNodeDefBuilder& IncompleteNodeDefBuilder::AddAttr(
    const string& attr, const DataType& type) {
  AddNodeAttr(attr, type, &nodedef_);
  return *this;
}

IncompleteNodeDefBuilder& IncompleteNodeDefBuilder::AddAttr(const string& attr,
                                                            int val) {
  AddNodeAttr(attr, val, &nodedef_);
  return *this;
}

IncompleteNodeDefBuilder& IncompleteNodeDefBuilder::Device(
    const string& device) {
  nodedef_.set_device(device);
  return *this;
}

Status IncompleteNodeDefBuilder::Build(Graph* graph, Node** n) {
  Status status;
  *n = graph->AddNode(nodedef_, &status);
  return status;
}

IncompleteNodeDefBuilder IncompleteNodeDefBuilder::Identity(
    const string& name, const DataType& type, const NodeDebugInfo& debug) {
  return IncompleteNodeDefBuilder(name, "Identity", debug).AddAttr("T", type);
}

IncompleteNodeDefBuilder IncompleteNodeDefBuilder::Merge(
    const string& name, const DataType& type, const NodeDebugInfo& debug,
    int n) {
  return IncompleteNodeDefBuilder(name, "Merge", debug)
      .AddAttr("T", type)
      .AddAttr("N", n);
}

IncompleteNodeDefBuilder IncompleteNodeDefBuilder::Switch(
    const string& name, const DataType& type, const NodeDebugInfo& debug) {
  return IncompleteNodeDefBuilder(name, "Switch", debug).AddAttr("T", type);
}

}  // namespace tensorflow
