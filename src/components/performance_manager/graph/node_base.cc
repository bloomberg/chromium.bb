// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/node_base.h"

#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/public/graph/node.h"

namespace performance_manager {

// static
const uintptr_t NodeBase::kNodeBaseType =
    reinterpret_cast<uintptr_t>(&kNodeBaseType);

NodeBase::NodeBase(NodeTypeEnum node_type) : type_(node_type) {}

NodeBase::~NodeBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The node must have been removed from the graph before destruction.
  DCHECK(!graph_);
}

// static
const NodeBase* NodeBase::FromNode(const Node* node) {
  CHECK_EQ(kNodeBaseType, node->GetImplType());
  return reinterpret_cast<const NodeBase*>(node->GetImpl());
}

// static
NodeBase* NodeBase::FromNode(Node* node) {
  CHECK_EQ(kNodeBaseType, node->GetImplType());
  return reinterpret_cast<NodeBase*>(const_cast<void*>(node->GetImpl()));
}

void NodeBase::JoinGraph(GraphImpl* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!graph_);
  DCHECK(graph->NodeInGraph(this));

  graph_ = graph;

  OnJoiningGraph();
}

void NodeBase::LeaveGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(graph_);
  DCHECK(graph_->NodeInGraph(this));

  OnBeforeLeavingGraph();

  graph_ = nullptr;
}

void NodeBase::OnJoiningGraph() {}

void NodeBase::OnBeforeLeavingGraph() {}

}  // namespace performance_manager
