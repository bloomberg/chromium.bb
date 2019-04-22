// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/graph.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/graph/system_node_impl.h"
#include "chrome/browser/performance_manager/observers/graph_observer.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"

namespace ukm {
class UkmEntryBuilder;
}  // namespace ukm

namespace performance_manager {

Graph::Graph() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

Graph::~Graph() {
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

void Graph::RegisterObserver(GraphObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer->set_node_graph(this);
  observers_.push_back(observer);
  observer->OnRegistered();
}

void Graph::UnregisterObserver(GraphObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool removed = false;
  for (auto it = observers_.begin(); it != observers_.end(); ++it) {
    if (*it == observer) {
      observers_.erase(it);
      removed = true;
      observer->OnUnregistered();
      break;
    }
  }
  DCHECK(removed);
}

void Graph::OnNodeAdded(NodeBase* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto* observer : observers_) {
    if (observer->ShouldObserve(node)) {
      node->AddObserver(observer);
      observer->OnNodeAdded(node);
    }
  }
}

void Graph::OnBeforeNodeRemoved(NodeBase* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  node->LeaveGraph();
}

SystemNodeImpl* Graph::FindOrCreateSystemNode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!system_node_) {
    // Create the singleton SystemCU instance. Ownership is taken by the graph.
    system_node_ = std::make_unique<SystemNodeImpl>(this);
    AddNewNode(system_node_.get());
  }

  return system_node_.get();
}

NodeBase* Graph::GetNodeByID(
    const resource_coordinator::CoordinationUnitID cu_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto& it = nodes_.find(cu_id);
  if (it == nodes_.end())
    return nullptr;
  return it->second;
}

ProcessNodeImpl* Graph::GetProcessNodeByPid(base::ProcessId pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = processes_by_pid_.find(pid);
  if (it == processes_by_pid_.end())
    return nullptr;

  return ProcessNodeImpl::FromNodeBase(it->second);
}

std::vector<ProcessNodeImpl*> Graph::GetAllProcessNodes() {
  return GetAllNodesOfType<ProcessNodeImpl>();
}

std::vector<FrameNodeImpl*> Graph::GetAllFrameNodes() {
  return GetAllNodesOfType<FrameNodeImpl>();
}

std::vector<PageNodeImpl*> Graph::GetAllPageNodes() {
  return GetAllNodesOfType<PageNodeImpl>();
}

size_t Graph::GetNodeAttachedDataCountForTesting(NodeBase* node,
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

void Graph::AddNewNode(NodeBase* new_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = nodes_.emplace(new_node->id(), new_node);
  DCHECK(it.second);  // Inserted successfully

  // Allow the node to initialize itself now that it's been added.
  new_node->JoinGraph();
  OnNodeAdded(new_node);
}

void Graph::RemoveNode(NodeBase* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnBeforeNodeRemoved(node);

  // Remove any node attached data affiliated with this node.
  auto lower =
      node_attached_data_map_.lower_bound(std::make_pair(node, nullptr));
  auto upper =
      node_attached_data_map_.lower_bound(std::make_pair(node + 1, nullptr));
  node_attached_data_map_.erase(lower, upper);

  // Before removing the node itself.
  size_t erased = nodes_.erase(node->id());
  DCHECK_EQ(1u, erased);
}

void Graph::BeforeProcessPidChange(ProcessNodeImpl* process,
                                   base::ProcessId new_pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // On Windows, PIDs are aggressively reused, and because not all process
  // creation/death notifications are synchronized, it's possible for more than
  // one CU to have the same PID. To handle this, the second and subsequent
  // registration override earlier registrations, while unregistration will only
  // unregister the current holder of the PID.
  if (process->process_id() != base::kNullProcessId) {
    auto it = processes_by_pid_.find(process->process_id());
    if (it != processes_by_pid_.end() && it->second == process)
      processes_by_pid_.erase(it);
  }
  if (new_pid != base::kNullProcessId)
    processes_by_pid_[new_pid] = process;
}

template <typename CUType>
std::vector<CUType*> Graph::GetAllNodesOfType() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto type = CUType::Type();
  std::vector<CUType*> ret;
  for (const auto& el : nodes_) {
    if (el.first.type == type)
      ret.push_back(CUType::FromNodeBase(el.second));
  }
  return ret;
}

}  // namespace performance_manager
