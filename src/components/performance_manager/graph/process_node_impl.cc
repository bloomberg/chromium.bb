// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/process_node_impl.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/graph_impl_util.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/v8_memory/v8_context_tracker.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {

namespace {

void FireBackgroundTracingTriggerOnUI(
    const std::string& trigger_name,
    content::BackgroundTracingManager* manager) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Don't fire a trigger unless we're in an active tracing scenario.
  // Renderer-initiated background tracing triggers are always "preemptive"
  // traces so we expect a scenario to be active.
  if (!manager)
    manager = content::BackgroundTracingManager::GetInstance();
  if (!manager->HasActiveScenario())
    return;

  static content::BackgroundTracingManager::TriggerHandle trigger_handle = -1;
  if (trigger_handle == -1) {
    trigger_handle = manager->RegisterTriggerType(
        content::BackgroundTracingManager::kContentTriggerConfig);
  }

  // Actually fire the trigger. We don't need to know when the trace is being
  // finalized so pass an empty callback.
  manager->TriggerNamedEvent(
      trigger_handle,
      content::BackgroundTracingManager::StartedFinalizingCallback());
}

}  // namespace

ProcessNodeImpl::ProcessNodeImpl(content::ProcessType process_type,
                                 RenderProcessHostProxy render_process_proxy)
    : process_type_(process_type),
      render_process_host_proxy_(std::move(render_process_proxy)) {
  weak_this_ = weak_factory_.GetWeakPtr();
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ProcessNodeImpl::~ProcessNodeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Crash if this process node is destroyed while still hosting a worker node.
  // TODO(https://crbug.com/1058705): Turn this into a DCHECK once the issue is
  //                                  resolved.
  CHECK(worker_nodes_.empty());
  DCHECK(!frozen_frame_data_);
  DCHECK(!process_priority_data_);
}

void ProcessNodeImpl::Bind(
    mojo::PendingReceiver<mojom::ProcessCoordinationUnit> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // A RenderProcessHost can be reused if the backing process suddenly dies, in
  // which case we will receive a new receiver from the newly spawned process.
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void ProcessNodeImpl::SetMainThreadTaskLoadIsLow(
    bool main_thread_task_load_is_low) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  main_thread_task_load_is_low_.SetAndMaybeNotify(this,
                                                  main_thread_task_load_is_low);
}

void ProcessNodeImpl::OnV8ContextCreated(
    mojom::V8ContextDescriptionPtr description,
    mojom::IframeAttributionDataPtr iframe_attribution_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (auto* tracker = v8_memory::V8ContextTracker::GetFromGraph(graph())) {
    tracker->OnV8ContextCreated(PassKey(), this, *description,
                                std::move(iframe_attribution_data));
  }
}

void ProcessNodeImpl::OnV8ContextDetached(
    const blink::V8ContextToken& v8_context_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (auto* tracker = v8_memory::V8ContextTracker::GetFromGraph(graph()))
    tracker->OnV8ContextDetached(PassKey(), this, v8_context_token);
}

void ProcessNodeImpl::OnV8ContextDestroyed(
    const blink::V8ContextToken& v8_context_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (auto* tracker = v8_memory::V8ContextTracker::GetFromGraph(graph()))
    tracker->OnV8ContextDestroyed(PassKey(), this, v8_context_token);
}

void ProcessNodeImpl::OnRemoteIframeAttached(
    const blink::LocalFrameToken& parent_frame_token,
    const blink::RemoteFrameToken& remote_frame_token,
    mojom::IframeAttributionDataPtr iframe_attribution_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (auto* tracker = v8_memory::V8ContextTracker::GetFromGraph(graph())) {
    auto* ec_registry =
        execution_context::ExecutionContextRegistry::GetFromGraph(graph());
    DCHECK(ec_registry);
    auto* parent_frame_node =
        ec_registry->GetFrameNodeByFrameToken(parent_frame_token);
    if (parent_frame_node) {
      tracker->OnRemoteIframeAttached(
          PassKey(), FrameNodeImpl::FromNode(parent_frame_node),
          remote_frame_token, std::move(iframe_attribution_data));
    }
  }
}

void ProcessNodeImpl::OnRemoteIframeDetached(
    const blink::LocalFrameToken& parent_frame_token,
    const blink::RemoteFrameToken& remote_frame_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (auto* tracker = v8_memory::V8ContextTracker::GetFromGraph(graph())) {
    auto* ec_registry =
        execution_context::ExecutionContextRegistry::GetFromGraph(graph());
    DCHECK(ec_registry);
    auto* parent_frame_node =
        ec_registry->GetFrameNodeByFrameToken(parent_frame_token);
    if (parent_frame_node) {
      tracker->OnRemoteIframeDetached(
          PassKey(), FrameNodeImpl::FromNode(parent_frame_node),
          remote_frame_token);
    }
  }
}

void ProcessNodeImpl::FireBackgroundTracingTrigger(
    const std::string& trigger_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FireBackgroundTracingTriggerOnUI, trigger_name, nullptr));
}

void ProcessNodeImpl::SetProcessExitStatus(int32_t exit_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This may occur as the first event seen in the case where the process
  // fails to start or suffers a startup crash.
  exit_status_ = exit_status;

  // Close the process handle to kill the zombie.
  process_.SetAndNotify(this, base::Process());

  // No more message should be received from this process.
  receiver_.reset();
}

void ProcessNodeImpl::SetProcess(base::Process process,
                                 base::Time launch_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(process.IsValid());
  // Either this is the initial process associated with this process node,
  // or it's a subsequent process. In the latter case, there must have been
  // an exit status associated with the previous process.
  DCHECK(!process_.value().IsValid() || exit_status_.has_value());

  base::ProcessId pid = process.Pid();
  SetProcessImpl(std::move(process), pid, launch_time);
}

const base::flat_set<FrameNodeImpl*>& ProcessNodeImpl::frame_nodes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return frame_nodes_;
}

PageNodeImpl* ProcessNodeImpl::GetPageNodeIfExclusive() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PageNodeImpl* page_node = nullptr;
  for (auto* frame_node : frame_nodes_) {
    if (!page_node)
      page_node = frame_node->page_node();
    if (page_node != frame_node->page_node())
      return nullptr;
  }
  return page_node;
}

RenderProcessHostId ProcessNodeImpl::GetRenderProcessId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return RenderProcessHostId(
      render_process_host_proxy_.render_process_host_id());
}

void ProcessNodeImpl::AddFrame(FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool inserted = frame_nodes_.insert(frame_node).second;
  DCHECK(inserted);
}

void ProcessNodeImpl::RemoveFrame(FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(frame_nodes_, frame_node));
  frame_nodes_.erase(frame_node);
}

void ProcessNodeImpl::AddWorker(WorkerNodeImpl* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool inserted = worker_nodes_.insert(worker_node).second;
  DCHECK(inserted);
}

void ProcessNodeImpl::RemoveWorker(WorkerNodeImpl* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(worker_nodes_, worker_node));
  worker_nodes_.erase(worker_node);
}

void ProcessNodeImpl::set_priority(base::TaskPriority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  priority_.SetAndMaybeNotify(this, priority);
}

void ProcessNodeImpl::add_hosted_content_type(ContentType content_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  hosted_content_types_.Put(content_type);
}

// static
void ProcessNodeImpl::FireBackgroundTracingTriggerOnUIForTesting(
    const std::string& trigger_name,
    content::BackgroundTracingManager* manager) {
  FireBackgroundTracingTriggerOnUI(trigger_name, manager);
}

base::WeakPtr<ProcessNodeImpl> ProcessNodeImpl::GetWeakPtrOnUIThread() {
  // TODO(siggi): Validate thread context.
  return weak_this_;
}

base::WeakPtr<ProcessNodeImpl> ProcessNodeImpl::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void ProcessNodeImpl::SetProcessImpl(base::Process process,
                                     base::ProcessId new_pid,
                                     base::Time launch_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  graph()->BeforeProcessPidChange(this, new_pid);

  // Clear the exit status for the previous process (if any).
  exit_status_.reset();

  // Also clear the measurement data (if any), as it references the previous
  // process.
  private_footprint_kb_ = 0;
  resident_set_kb_ = 0;

  process_id_ = new_pid;
  launch_time_ = launch_time;

  // Set the process variable last, as it will fire the notification.
  process_.SetAndNotify(this, std::move(process));
}

content::ProcessType ProcessNodeImpl::GetProcessType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return process_type();
}

base::ProcessId ProcessNodeImpl::GetProcessId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return process_id();
}

const base::Process& ProcessNodeImpl::GetProcess() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return process();
}

base::Time ProcessNodeImpl::GetLaunchTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return launch_time();
}

absl::optional<int32_t> ProcessNodeImpl::GetExitStatus() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return exit_status();
}

bool ProcessNodeImpl::VisitFrameNodes(const FrameNodeVisitor& visitor) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto* frame_impl : frame_nodes()) {
    const FrameNode* frame = frame_impl;
    if (!visitor.Run(frame))
      return false;
  }
  return true;
}

base::flat_set<const FrameNode*> ProcessNodeImpl::GetFrameNodes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return UpcastNodeSet<FrameNode>(frame_nodes());
}

base::flat_set<const WorkerNode*> ProcessNodeImpl::GetWorkerNodes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return UpcastNodeSet<WorkerNode>(worker_nodes_);
}

bool ProcessNodeImpl::GetMainThreadTaskLoadIsLow() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return main_thread_task_load_is_low();
}

uint64_t ProcessNodeImpl::GetPrivateFootprintKb() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return private_footprint_kb();
}

uint64_t ProcessNodeImpl::GetResidentSetKb() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return resident_set_kb();
}

RenderProcessHostId ProcessNodeImpl::GetRenderProcessHostId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetRenderProcessId();
}

const RenderProcessHostProxy& ProcessNodeImpl::GetRenderProcessHostProxy()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return render_process_host_proxy();
}

base::TaskPriority ProcessNodeImpl::GetPriority() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return priority();
}

ProcessNode::ContentTypes ProcessNodeImpl::GetHostedContentTypes() const {
  return hosted_content_types();
}

void ProcessNodeImpl::OnAllFramesInProcessFrozen() {
  for (auto* observer : GetObservers())
    observer->OnAllFramesInProcessFrozen(this);
}

void ProcessNodeImpl::OnBeforeLeavingGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Make as if we're transitioning to the null PID before we die to clear this
  // instance from the PID map.
  if (process_id_ != base::kNullProcessId)
    graph()->BeforeProcessPidChange(this, base::kNullProcessId);

  // All child frames should have been removed before the process is removed.
  DCHECK(frame_nodes_.empty());
}

void ProcessNodeImpl::RemoveNodeAttachedData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  frozen_frame_data_.Reset();
  process_priority_data_.reset();
}

}  // namespace performance_manager
