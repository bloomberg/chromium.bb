// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/graph_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/graph/system_node_impl.h"
#include "chrome/browser/performance_manager/observers/graph_observer.h"

namespace ukm {
class UkmEntryBuilder;
}  // namespace ukm

namespace performance_manager {

namespace {

// A unique type ID for this implementation.
const uintptr_t kGraphImplType = reinterpret_cast<uintptr_t>(&kGraphImplType);

}  // namespace

GraphImpl::GraphImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

GraphImpl::~GraphImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // All observers should have been removed before the graph is deleted.
  DCHECK(observers_.empty());
  // All process nodes should have been removed already.
  DCHECK(processes_by_pid_.empty());

  // Remove the system node from the graph, this should be the only node left.
  if (system_node_.get()) {
    RemoveNode(system_node_.get());
    system_node_.reset();
  }

  DCHECK(nodes_.empty());
}

uintptr_t GraphImpl::GetImplType() const {
  return kGraphImplType;
}

const void* GraphImpl::GetImpl() const {
  return this;
}

// static
GraphImpl* GraphImpl::FromGraph(const Graph* graph) {
  CHECK_EQ(kGraphImplType, graph->GetImplType());
  return reinterpret_cast<GraphImpl*>(const_cast<void*>(graph->GetImpl()));
}

void GraphImpl::RegisterObserver(GraphObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer->SetGraph(this);
  observers_.push_back(observer);
  observer->OnRegistered();
}

void GraphImpl::UnregisterObserver(GraphObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool removed = false;
  for (auto it = observers_.begin(); it != observers_.end(); ++it) {
    if (*it == observer) {
      observers_.erase(it);
      removed = true;
      observer->OnUnregistered();
      observer->SetGraph(nullptr);
      break;
    }
  }
  DCHECK(removed);
}

void GraphImpl::OnNodeAdded(NodeBase* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto* observer : observers_) {
    if (observer->ShouldObserve(node)) {
      // TODO(chrisha): Remove this logic once all observers have been migrated.
      switch (node->type()) {
        case NodeTypeEnum::kFrame: {
          FrameNodeImpl::FromNodeBase(node)->AddObserver(observer);
        } break;
        case NodeTypeEnum::kPage: {
          PageNodeImpl::FromNodeBase(node)->AddObserver(observer);
        } break;
        case NodeTypeEnum::kProcess: {
          ProcessNodeImpl::FromNodeBase(node)->AddObserver(observer);
        } break;
        case NodeTypeEnum::kSystem: {
          SystemNodeImpl::FromNodeBase(node)->AddObserver(observer);
        } break;
        case NodeTypeEnum::kInvalidType: {
          NOTREACHED();
        } break;
      }
      observer->OnNodeAdded(node);
    }
  }
}

void GraphImpl::OnBeforeNodeRemoved(NodeBase* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(chrisha): Kill this logic once observer implementations use distinct
  // interfaces.
  switch (node->type()) {
    case NodeTypeEnum::kFrame: {
      OnBeforeNodeRemovedImpl(FrameNodeImpl::FromNodeBase(node));
    } break;
    case NodeTypeEnum::kPage: {
      OnBeforeNodeRemovedImpl(PageNodeImpl::FromNodeBase(node));
    } break;
    case NodeTypeEnum::kProcess: {
      OnBeforeNodeRemovedImpl(ProcessNodeImpl::FromNodeBase(node));
    } break;
    case NodeTypeEnum::kSystem: {
      OnBeforeNodeRemovedImpl(SystemNodeImpl::FromNodeBase(node));
    } break;
    case NodeTypeEnum::kInvalidType: {
      NOTREACHED();
    } break;
  }

  // Leave the graph only after the OnBeforeNodeRemoved notification so that the
  // node still observes the graph invariant during that callback.
  node->LeaveGraph();
}

template <typename NodeType>
void GraphImpl::OnBeforeNodeRemovedImpl(NodeType* node) {
  // The current observer logic ensures that OnBeforeNodeRemoved is only fired
  // for nodes that had an observer added via ShouldObserve. This logic will
  // be disappearing entirely, but emulate it for correctness right now.

  base::flat_set<typename NodeType::Observer*> node_observers;
  for (auto& observer : node->observers())
    node_observers.insert(&observer);

  for (auto* observer : observers_) {
    typename NodeType::Observer* node_observer = observer;
    if (base::ContainsKey(node_observers, node_observer))
      observer->OnBeforeNodeRemoved(node);
  }
}

int64_t GraphImpl::GetNextNodeSerializationId() {
  return ++current_node_serialization_id_;
}

SystemNodeImpl* GraphImpl::FindOrCreateSystemNode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!system_node_) {
    // Create the singleton system node instance. Ownership is taken by the
    // graph.
    system_node_ = std::make_unique<SystemNodeImpl>(this);
    AddNewNode(system_node_.get());
  }

  return system_node_.get();
}

bool GraphImpl::NodeInGraph(const NodeBase* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto& it = nodes_.find(const_cast<NodeBase*>(node));
  return it != nodes_.end();
}

ProcessNodeImpl* GraphImpl::GetProcessNodeByPid(base::ProcessId pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = processes_by_pid_.find(pid);
  if (it == processes_by_pid_.end())
    return nullptr;

  return ProcessNodeImpl::FromNodeBase(it->second);
}

std::vector<ProcessNodeImpl*> GraphImpl::GetAllProcessNodes() {
  return GetAllNodesOfType<ProcessNodeImpl>();
}

std::vector<FrameNodeImpl*> GraphImpl::GetAllFrameNodes() {
  return GetAllNodesOfType<FrameNodeImpl>();
}

std::vector<PageNodeImpl*> GraphImpl::GetAllPageNodes() {
  return GetAllNodesOfType<PageNodeImpl>();
}

size_t GraphImpl::GetNodeAttachedDataCountForTesting(NodeBase* node,
                                                     const void* key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!node && !key)
    return node_attached_data_map_.size();

  size_t count = 0;
  for (auto& node_data : node_attached_data_map_) {
    if (node && node_data.first.first != node)
      continue;
    if (key && node_data.first.second != key)
      continue;
    ++count;
  }

  return count;
}

void GraphImpl::AddNewNode(NodeBase* new_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = nodes_.insert(new_node);
  DCHECK(it.second);  // Inserted successfully

  // Allow the node to initialize itself now that it's been added.
  new_node->JoinGraph();
  OnNodeAdded(new_node);
}

void GraphImpl::RemoveNode(NodeBase* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnBeforeNodeRemoved(node);

  // Remove any node attached data affiliated with this node.
  auto lower =
      node_attached_data_map_.lower_bound(std::make_pair(node, nullptr));
  auto upper =
      node_attached_data_map_.lower_bound(std::make_pair(node + 1, nullptr));
  node_attached_data_map_.erase(lower, upper);

  // Before removing the node itself.
  size_t erased = nodes_.erase(node);
  DCHECK_EQ(1u, erased);
}

void GraphImpl::BeforeProcessPidChange(ProcessNodeImpl* process,
                                       base::ProcessId new_pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // On Windows, PIDs are aggressively reused, and because not all process
  // creation/death notifications are synchronized, it's possible for more than
  // one process node to have the same PID. To handle this, the second and
  // subsequent registration override earlier registrations, while
  // unregistration will only unregister the current holder of the PID.
  if (process->process_id() != base::kNullProcessId) {
    auto it = processes_by_pid_.find(process->process_id());
    if (it != processes_by_pid_.end() && it->second == process)
      processes_by_pid_.erase(it);
  }
  if (new_pid != base::kNullProcessId)
    processes_by_pid_[new_pid] = process;
}

template <typename NodeType>
std::vector<NodeType*> GraphImpl::GetAllNodesOfType() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto type = NodeType::Type();
  std::vector<NodeType*> ret;
  for (auto* node : nodes_) {
    if (node->type() == type)
      ret.push_back(NodeType::FromNodeBase(node));
  }
  return ret;
}

GraphImpl::Observer::Observer() = default;
GraphImpl::Observer::~Observer() = default;

GraphImpl::ObserverDefaultImpl::ObserverDefaultImpl() = default;
GraphImpl::ObserverDefaultImpl::~ObserverDefaultImpl() = default;

void GraphImpl::ObserverDefaultImpl::SetGraph(GraphImpl* graph) {
  graph_ = graph;
}

}  // namespace performance_manager
