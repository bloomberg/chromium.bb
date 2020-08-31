// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/performance_manager_impl.h"

#include <atomic>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"

namespace performance_manager {

namespace {

// Singleton instance of PerformanceManagerImpl. Set from
// PerformanceManagerImpl::StartImpl() and reset from the destructor of
// PerformanceManagerImpl (PM sequence). Should only be accessed on the PM
// sequence.
PerformanceManagerImpl* g_performance_manager = nullptr;

// The performance manager TaskRunner. Thread-safe.
//
// NOTE: This task runner has to block shutdown as some of the tasks posted to
// it should be guaranteed to run before shutdown (e.g. removing some entries
// from the site data store).
base::LazyThreadPoolSequencedTaskRunner g_performance_manager_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::TaskPriority::USER_VISIBLE,
                         base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
                         base::MayBlock()));

// Indicates if a task posted to |g_performance_manager_task_runner| will have
// access to a valid PerformanceManagerImpl instance via
// |g_performance_manager|. Should only be accessed on the main thread.
bool g_pm_is_available = false;

}  // namespace

// static
bool PerformanceManager::IsAvailable() {
  return g_pm_is_available;
}

PerformanceManagerImpl::~PerformanceManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(g_performance_manager, this);
  // TODO(https://crbug.com/966840): Move this to a TearDown function.
  graph_.TearDown();
  g_performance_manager = nullptr;
  if (on_destroyed_callback_)
    std::move(on_destroyed_callback_).Run();
}

// static
void PerformanceManagerImpl::CallOnGraphImpl(const base::Location& from_here,
                                             base::OnceClosure callback) {
  DCHECK(callback);
  GetTaskRunner()->PostTask(from_here, std::move(callback));
}

// static
void PerformanceManagerImpl::CallOnGraphImpl(const base::Location& from_here,
                                             GraphImplCallback callback) {
  DCHECK(callback);
  GetTaskRunner()->PostTask(
      from_here,
      base::BindOnce(&PerformanceManagerImpl::RunCallbackWithGraphImpl,
                     std::move(callback)));
}

// static
std::unique_ptr<PerformanceManagerImpl> PerformanceManagerImpl::Create(
    GraphImplCallback on_start) {
  DCHECK(!g_pm_is_available);
  g_pm_is_available = true;

  std::unique_ptr<PerformanceManagerImpl> instance =
      base::WrapUnique(new PerformanceManagerImpl());

  GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&PerformanceManagerImpl::OnStartImpl,
                     base::Unretained(instance.get()), std::move(on_start)));

  return instance;
}

// static
void PerformanceManagerImpl::Destroy(
    std::unique_ptr<PerformanceManager> instance) {
  DCHECK(g_pm_is_available);
  g_pm_is_available = false;

  GetTaskRunner()->DeleteSoon(FROM_HERE, instance.release());
}

// static
std::unique_ptr<FrameNodeImpl> PerformanceManagerImpl::CreateFrameNode(
    ProcessNodeImpl* process_node,
    PageNodeImpl* page_node,
    FrameNodeImpl* parent_frame_node,
    int frame_tree_node_id,
    int render_frame_id,
    const base::UnguessableToken& dev_tools_token,
    int32_t browsing_instance_id,
    int32_t site_instance_id,
    FrameNodeCreationCallback creation_callback) {
  return CreateNodeImpl<FrameNodeImpl>(
      std::move(creation_callback), process_node, page_node, parent_frame_node,
      frame_tree_node_id, render_frame_id, dev_tools_token,
      browsing_instance_id, site_instance_id);
}

// static
std::unique_ptr<PageNodeImpl> PerformanceManagerImpl::CreatePageNode(
    const WebContentsProxy& contents_proxy,
    const std::string& browser_context_id,
    const GURL& visible_url,
    bool is_visible,
    bool is_audible,
    base::TimeTicks visibility_change_time) {
  return CreateNodeImpl<PageNodeImpl>(base::OnceCallback<void(PageNodeImpl*)>(),
                                      contents_proxy, browser_context_id,
                                      visible_url, is_visible, is_audible,
                                      visibility_change_time);
}

// static
std::unique_ptr<ProcessNodeImpl> PerformanceManagerImpl::CreateProcessNode(
    content::ProcessType process_type,
    RenderProcessHostProxy proxy) {
  return CreateNodeImpl<ProcessNodeImpl>(
      base::OnceCallback<void(ProcessNodeImpl*)>(), process_type, proxy);
}

// static
std::unique_ptr<WorkerNodeImpl> PerformanceManagerImpl::CreateWorkerNode(
    const std::string& browser_context_id,
    WorkerNode::WorkerType worker_type,
    ProcessNodeImpl* process_node,
    const base::UnguessableToken& dev_tools_token) {
  return CreateNodeImpl<WorkerNodeImpl>(
      base::OnceCallback<void(WorkerNodeImpl*)>(), browser_context_id,
      worker_type, process_node, dev_tools_token);
}

// static
void PerformanceManagerImpl::DeleteNode(std::unique_ptr<NodeBase> node) {
  CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&PerformanceManagerImpl::DeleteNodeImpl, node.release()));
}

// static
void PerformanceManagerImpl::BatchDeleteNodes(
    std::vector<std::unique_ptr<NodeBase>> nodes) {
  // Move the nodes vector to the heap.
  auto nodes_ptr = std::make_unique<std::vector<std::unique_ptr<NodeBase>>>(
      std::move(nodes));
  CallOnGraphImpl(FROM_HERE,
                  base::BindOnce(&PerformanceManagerImpl::BatchDeleteNodesImpl,
                                 nodes_ptr.release()));
}

// static
bool PerformanceManagerImpl::OnPMTaskRunnerForTesting() {
  return GetTaskRunner()->RunsTasksInCurrentSequence();
}

// static
void PerformanceManagerImpl::SetOnDestroyedCallbackForTesting(
    base::OnceClosure callback) {
  // Bind the callback in one that can be called on the PM sequence (it also
  // binds the main thread, and bounces a task back to that thread).
  scoped_refptr<base::SequencedTaskRunner> main_thread =
      base::SequencedTaskRunnerHandle::Get();
  base::OnceClosure pm_callback = base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> main_thread,
         base::OnceClosure callback) {
        main_thread->PostTask(FROM_HERE, std::move(callback));
      },
      main_thread, std::move(callback));

  // Pass the callback to the PM.
  GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&PerformanceManagerImpl::SetOnDestroyedCallbackImpl,
                     std::move(pm_callback)));
}

PerformanceManagerImpl::PerformanceManagerImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

// static
scoped_refptr<base::SequencedTaskRunner>
PerformanceManagerImpl::GetTaskRunner() {
  return g_performance_manager_task_runner.Get();
}

PerformanceManagerImpl* PerformanceManagerImpl::GetInstance() {
  return g_performance_manager;
}

namespace {

// Helper function for adding a node to a graph, and invoking a post-creation
// callback immediately afterwards.
template <typename NodeType>
void AddNodeAndInvokeCreationCallback(
    base::OnceCallback<void(NodeType*)> callback,
    NodeType* node,
    GraphImpl* graph) {
  graph->AddNewNode(node);
  if (callback)
    std::move(callback).Run(node);
}

}  // namespace

// static
template <typename NodeType, typename... Args>
std::unique_ptr<NodeType> PerformanceManagerImpl::CreateNodeImpl(
    base::OnceCallback<void(NodeType*)> creation_callback,
    Args&&... constructor_args) {
  std::unique_ptr<NodeType> new_node =
      std::make_unique<NodeType>(std::forward<Args>(constructor_args)...);
  CallOnGraphImpl(FROM_HERE,
                  base::BindOnce(&AddNodeAndInvokeCreationCallback<NodeType>,
                                 std::move(creation_callback),
                                 base::Unretained(new_node.get())));
  return new_node;
}

// static
void PerformanceManagerImpl::DeleteNodeImpl(NodeBase* node_ptr,
                                            GraphImpl* graph) {
  // Must be done first to avoid leaking |node_ptr|.
  std::unique_ptr<NodeBase> node(node_ptr);

  graph->RemoveNode(node.get());
}

namespace {

void RemoveFrameAndChildrenFromGraph(FrameNodeImpl* frame_node,
                                     GraphImpl* graph) {
  // Recurse on the first child while there is one.
  while (!frame_node->child_frame_nodes().empty()) {
    RemoveFrameAndChildrenFromGraph(*(frame_node->child_frame_nodes().begin()),
                                    graph);
  }

  // Now that all children are deleted, delete this frame.
  graph->RemoveNode(frame_node);
}

}  // namespace

// static
void PerformanceManagerImpl::BatchDeleteNodesImpl(
    std::vector<std::unique_ptr<NodeBase>>* nodes_ptr,
    GraphImpl* graph) {
  // Must be done first to avoid leaking |nodes_ptr|.
  std::unique_ptr<std::vector<std::unique_ptr<NodeBase>>> nodes(nodes_ptr);

  base::flat_set<ProcessNodeImpl*> process_nodes;

  for (const auto& node : *nodes) {
    switch (node->type()) {
      case PageNodeImpl::Type(): {
        auto* page_node = PageNodeImpl::FromNodeBase(node.get());

        // Delete the main frame nodes until no more exist.
        while (!page_node->main_frame_nodes().empty()) {
          RemoveFrameAndChildrenFromGraph(
              *(page_node->main_frame_nodes().begin()), graph);
        }

        graph->RemoveNode(page_node);
        break;
      }
      case ProcessNodeImpl::Type(): {
        // Keep track of the process nodes for removing once all frames nodes
        // are removed.
        auto* process_node = ProcessNodeImpl::FromNodeBase(node.get());
        process_nodes.insert(process_node);
        break;
      }
      case FrameNodeImpl::Type():
        break;
      case WorkerNodeImpl::Type(): {
        auto* worker_node = WorkerNodeImpl::FromNodeBase(node.get());
        graph->RemoveNode(worker_node);
        break;
      }
      case SystemNodeImpl::Type():
      case NodeTypeEnum::kInvalidType:
      default: {
        NOTREACHED();
        break;
      }
    }
  }

  // Remove the process nodes from the graph.
  for (auto* process_node : process_nodes)
    graph->RemoveNode(process_node);

  // When |nodes| goes out of scope, all nodes are deleted.
}

void PerformanceManagerImpl::OnStartImpl(GraphImplCallback on_start) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!g_performance_manager);

  g_performance_manager = this;
  graph_.set_ukm_recorder(ukm::UkmRecorder::Get());
  std::move(on_start).Run(&graph_);
}

// static
void PerformanceManagerImpl::RunCallbackWithGraphImpl(
    GraphImplCallback graph_callback) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());

  if (g_performance_manager)
    std::move(graph_callback).Run(&g_performance_manager->graph_);
}

// static
void PerformanceManagerImpl::RunCallbackWithGraph(
    GraphCallback graph_callback) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());

  if (g_performance_manager)
    std::move(graph_callback).Run(&g_performance_manager->graph_);
}

// static
void PerformanceManagerImpl::SetOnDestroyedCallbackImpl(
    base::OnceClosure callback) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());

  if (g_performance_manager)
    g_performance_manager->on_destroyed_callback_ = std::move(callback);
}

}  // namespace performance_manager
