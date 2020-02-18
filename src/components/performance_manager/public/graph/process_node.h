// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PROCESS_NODE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PROCESS_NODE_H_

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "base/task/task_traits.h"
#include "components/performance_manager/public/graph/node.h"

namespace base {
class Process;
}  // namespace base

namespace performance_manager {

class FrameNode;
class ProcessNodeObserver;
class RenderProcessHostProxy;

// A process node follows the lifetime of a RenderProcessHost.
// It may reference zero or one processes at a time, but during its lifetime, it
// may reference more than one process. This can happen if the associated
// renderer crashes, and an associated frame is then reloaded or re-navigated.
// The state of the process node goes through:
// 1. Created, no PID.
// 2. Process started, have PID - in the case where the associated render
//    process fails to start, this state may not occur.
// 3. Process died or failed to start, have exit status.
// 4. Back to 2.
//
// It is only valid to access this object on the sequence of the graph that owns
// it.
class ProcessNode : public Node {
 public:
  using FrameNodeVisitor = base::RepeatingCallback<bool(const FrameNode*)>;
  using Observer = ProcessNodeObserver;
  class ObserverDefaultImpl;

  ProcessNode();
  ~ProcessNode() override;

  // Returns the process ID associated with this process. Use this in preference
  // to querying GetProcess.Pid(). It's always valid to access, but will return
  // kNullProcessId if the process has yet started. It will also retain the
  // process ID for a process that has exited (at least until the underlying
  // RenderProcessHost gets reused in the case of a crash). Refrain from using
  // this as a unique identifier as on some platforms PIDs are reused
  // aggressively. See GetLaunchTime for more information.
  virtual base::ProcessId GetProcessId() const = 0;

  // Returns the base::Process backing this process. This will be an invalid
  // process if it has not yet started, or if it has exited.
  virtual const base::Process& GetProcess() const = 0;

  // Returns the launch time associated with the process. Combined with the
  // process ID this can be used as a unique identifier for the process.
  virtual base::Time GetLaunchTime() const = 0;

  // Returns the exit status of this process. This will be empty if the process
  // has not yet exited.
  virtual base::Optional<int32_t> GetExitStatus() const = 0;

  // Visits the frame nodes that are hosted in this process. The iteration is
  // halted if the visitor returns false.
  virtual void VisitFrameNodes(const FrameNodeVisitor& visitor) const = 0;

  // Returns the set of frame nodes that are hosted in this process. Note that
  // calling this causes the set of nodes to be generated.
  virtual base::flat_set<const FrameNode*> GetFrameNodes() const = 0;

  // Returns the current expected task queuing duration in the process. This is
  // measure of main thread latency. See
  // ProcessNodeObserver::OnExpectedTaskQueueingDurationSample.
  virtual base::TimeDelta GetExpectedTaskQueueingDuration() const = 0;

  // Returns true if the main thread task load is low (below some threshold
  // of usage). See ProcessNodeObserver::OnMainThreadTaskLoadIsLow.
  virtual bool GetMainThreadTaskLoadIsLow() const = 0;

  // Returns the current renderer process CPU usage. A value of 1.0 can mean 1
  // core at 100%, or 2 cores at 50% each, for example.
  virtual double GetCpuUsage() const = 0;

  // Returns the cumulative CPU usage of the renderer process over its entire
  // lifetime, expressed as CPU seconds.
  virtual base::TimeDelta GetCumulativeCpuUsage() const = 0;

  // Returns the most recently measured private memory footprint of the process.
  // This is roughly private, anonymous, non-discardable, resident or swapped
  // memory in kilobytes. For more details, see https://goo.gl/3kPb9S.
  virtual uint64_t GetPrivateFootprintKb() const = 0;

  // Returns the most recently measured resident set of the process, in
  // kilobytes.
  virtual uint64_t GetResidentSetKb() const = 0;

  // Returns a proxy to the RenderProcessHost associated with this node. The
  // proxy may only be dereferenced on the UI thread.
  virtual const RenderProcessHostProxy& GetRenderProcessHostProxy() const = 0;

  // Returns the current priority of the process.
  virtual base::TaskPriority GetPriority() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProcessNode);
};

// Pure virtual observer interface. Derive from this if you want to be forced to
// implement the entire interface.
class ProcessNodeObserver {
 public:
  ProcessNodeObserver();
  virtual ~ProcessNodeObserver();

  // Node lifetime notifications.

  // Called when a |process_node| is added to the graph.
  virtual void OnProcessNodeAdded(const ProcessNode* process_node) = 0;

  // The process associated with |process_node| has been started or has exited.
  // This implies some or all of the process, process_id, launch time and/or
  // exit status properties have changed.
  virtual void OnProcessLifetimeChange(const ProcessNode* process_node) = 0;

  // Called before a |process_node| is removed from the graph.
  virtual void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) = 0;

  // Notifications of property changes.

  // Invoked when a new |expected_task_queueing_duration| sample is available.
  virtual void OnExpectedTaskQueueingDurationSample(
      const ProcessNode* process_node) = 0;

  // Invoked when the |main_thread_task_load_is_low| property changes.
  virtual void OnMainThreadTaskLoadIsLow(const ProcessNode* process_node) = 0;

  // Invoked when the process priority changes.
  virtual void OnPriorityChanged(const ProcessNode* process_node,
                                 base::TaskPriority previous_value) = 0;

  // Events with no property changes.

  // Fired when all frames in a process have transitioned to being frozen.
  virtual void OnAllFramesInProcessFrozen(const ProcessNode* process_node) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProcessNodeObserver);
};

// Default implementation of observer that provides dummy versions of each
// function. Derive from this if you only need to implement a few of the
// functions.
class ProcessNode::ObserverDefaultImpl : public ProcessNodeObserver {
 public:
  ObserverDefaultImpl();
  ~ObserverDefaultImpl() override;

  // ProcessNodeObserver implementation:
  void OnProcessNodeAdded(const ProcessNode* process_node) override {}
  void OnProcessLifetimeChange(const ProcessNode* process_node) override {}
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override {}
  void OnExpectedTaskQueueingDurationSample(
      const ProcessNode* process_node) override {}
  void OnMainThreadTaskLoadIsLow(const ProcessNode* process_node) override {}
  void OnPriorityChanged(const ProcessNode* process_node,
                         base::TaskPriority previous_value) override {}
  void OnAllFramesInProcessFrozen(const ProcessNode* process_node) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ObserverDefaultImpl);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PROCESS_NODE_H_
