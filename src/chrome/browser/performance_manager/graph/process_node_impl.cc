// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/process_node_impl.h"

#include "base/logging.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_impl.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"

namespace performance_manager {

ProcessNodeImplObserver::ProcessNodeImplObserver() = default;
ProcessNodeImplObserver::~ProcessNodeImplObserver() = default;

ProcessNodeImpl::ProcessNodeImpl(GraphImpl* graph)
    : TypedNodeBase(graph), binding_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ProcessNodeImpl::~ProcessNodeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ProcessNodeImpl::AddFrame(FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool inserted = frame_nodes_.insert(frame_node).second;
  DCHECK(inserted);
}

void ProcessNodeImpl::RemoveFrame(FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::ContainsKey(frame_nodes_, frame_node));
  frame_nodes_.erase(frame_node);
}

void ProcessNodeImpl::SetCPUUsage(double cpu_usage) {
  cpu_usage_ = cpu_usage;
}

void ProcessNodeImpl::Bind(
    resource_coordinator::mojom::ProcessCoordinationUnitRequest request) {
  if (binding_.is_bound())
    binding_.Close();
  binding_.Bind(std::move(request));
}

void ProcessNodeImpl::SetExpectedTaskQueueingDuration(
    base::TimeDelta duration) {
  expected_task_queueing_duration_.SetAndNotify(this, duration);
}

void ProcessNodeImpl::SetMainThreadTaskLoadIsLow(
    bool main_thread_task_load_is_low) {
  main_thread_task_load_is_low_.SetAndMaybeNotify(this,
                                                  main_thread_task_load_is_low);
}

void ProcessNodeImpl::SetProcessExitStatus(int32_t exit_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This may occur as the first event seen in the case where the process
  // fails to start or suffers a startup crash.
  exit_status_ = exit_status;

  // Close the process handle to kill the zombie.
  process_.Close();

  // No more message should be received from this process.
  binding_.Close();
}

void ProcessNodeImpl::SetProcess(base::Process process,
                                 base::Time launch_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(process.IsValid());
  // Either this is the initial process associated with this process node,
  // or it's a subsequent process. In the latter case, there must have been
  // an exit status associated with the previous process.
  DCHECK(!process_.IsValid() || exit_status_.has_value());

  base::ProcessId pid = process.Pid();
  SetProcessImpl(std::move(process), pid, launch_time);
}

const base::flat_set<FrameNodeImpl*>& ProcessNodeImpl::GetFrameNodes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return frame_nodes_;
}

// There is currently not a direct relationship between processes and
// pages. However, frames are children of both processes and frames, so we
// find all of the pages that are reachable from the process's child
// frames.
base::flat_set<PageNodeImpl*> ProcessNodeImpl::GetAssociatedPageNodes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::flat_set<PageNodeImpl*> page_nodes;
  for (auto* frame_node : frame_nodes_)
    page_nodes.insert(frame_node->page_node());
  return page_nodes;
}

PageNodeImpl* ProcessNodeImpl::GetPageNodeIfExclusive() const {
  PageNodeImpl* page_node = nullptr;
  for (auto* frame_node : frame_nodes_) {
    if (!page_node)
      page_node = frame_node->page_node();
    if (page_node != frame_node->page_node())
      return nullptr;
  }
  return page_node;
}

void ProcessNodeImpl::SetProcessImpl(base::Process process,
                                     base::ProcessId new_pid,
                                     base::Time launch_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  graph()->BeforeProcessPidChange(this, new_pid);

  process_ = std::move(process);
  process_id_ = new_pid;
  launch_time_ = launch_time;

  // Clear the exit status for the previous process (if any).
  exit_status_.reset();

  // Also clear the measurement data (if any), as it references the previous
  // process.
  private_footprint_kb_ = 0;
  cumulative_cpu_usage_ = base::TimeDelta();
}

void ProcessNodeImpl::LeaveGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NodeBase::LeaveGraph();

  // Make as if we're transitioning to the null PID before we die to clear this
  // instance from the PID map.
  if (process_id_ != base::kNullProcessId)
    graph()->BeforeProcessPidChange(this, base::kNullProcessId);

  // All child frames should have been removed before the process is removed.
  DCHECK(frame_nodes_.empty());
}

ProcessNodeImpl::ObserverDefaultImpl::ObserverDefaultImpl() = default;
ProcessNodeImpl::ObserverDefaultImpl::~ObserverDefaultImpl() = default;

}  // namespace performance_manager
