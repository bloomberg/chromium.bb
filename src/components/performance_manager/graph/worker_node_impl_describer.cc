// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/worker_node_impl_describer.h"

#include "base/strings/string_number_conversions.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"

namespace performance_manager {

namespace {
const char kDescriberName[] = "WorkerNode";

const char* WorkerTypeToString(WorkerNode::WorkerType state) {
  switch (state) {
    case WorkerNode::WorkerType::kDedicated:
      return "kDedicated";
    case WorkerNode::WorkerType::kShared:
      return "kShared";
    case WorkerNode::WorkerType::kService:
      return "kService";
  }
}

}  // namespace

void WorkerNodeImplDescriber::OnPassedToGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
}

void WorkerNodeImplDescriber::OnTakenFromGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
}

base::Value WorkerNodeImplDescriber::DescribeWorkerNodeData(
    const WorkerNode* node) const {
  const WorkerNodeImpl* impl = WorkerNodeImpl::FromNode(node);
  if (!impl)
    return base::Value();

  base::Value ret(base::Value::Type::DICTIONARY);
  ret.SetKey("browser_context_id", base::Value(impl->browser_context_id()));
  ret.SetKey("dev_tools_token",
             base::Value(impl->dev_tools_token().ToString()));
  ret.SetKey("url", base::Value(impl->url().spec()));
  ret.SetKey("worker_type",
             base::Value(WorkerTypeToString(impl->worker_type())));

  return ret;
}

}  // namespace performance_manager
