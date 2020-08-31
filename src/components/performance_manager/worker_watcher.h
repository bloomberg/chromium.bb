// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_WORKER_WATCHER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_WORKER_WATCHER_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "content/public/browser/dedicated_worker_id.h"
#include "content/public/browser/dedicated_worker_service.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/shared_worker_id.h"
#include "content/public/browser/shared_worker_service.h"

namespace performance_manager {

class FrameNodeImpl;
class FrameNodeSource;
class ProcessNodeSource;
class WorkerNodeImpl;

// This class keeps track of running workers of all types for a single browser
// context and handles the ownership of the worker nodes.
//
// TODO(https://crbug.com/993029): Add support for service workers.
class WorkerWatcher : public content::DedicatedWorkerService::Observer,
                      public content::SharedWorkerService::Observer,
                      public content::ServiceWorkerContextObserver {
 public:
  WorkerWatcher(const std::string& browser_context_id,
                content::DedicatedWorkerService* dedicated_worker_service,
                content::SharedWorkerService* shared_worker_service,
                content::ServiceWorkerContext* service_worker_context,
                ProcessNodeSource* process_node_source,
                FrameNodeSource* frame_node_source);
  ~WorkerWatcher() override;

  // Cleans up this instance and ensures shared worker nodes are correctly
  // destroyed on the PM graph.
  void TearDown();

  // content::DedicatedWorkerService::Observer:
  void OnWorkerStarted(
      content::DedicatedWorkerId dedicated_worker_id,
      int worker_process_id,
      content::GlobalFrameRoutingId ancestor_render_frame_host_id) override;
  void OnBeforeWorkerTerminated(
      content::DedicatedWorkerId dedicated_worker_id,
      content::GlobalFrameRoutingId ancestor_render_frame_host_id) override;
  void OnFinalResponseURLDetermined(
      content::DedicatedWorkerId dedicated_worker_id,
      const GURL& url) override;

  // content::SharedWorkerService::Observer:
  void OnWorkerStarted(content::SharedWorkerId shared_worker_id,
                       int worker_process_id,
                       const base::UnguessableToken& dev_tools_token) override;
  void OnBeforeWorkerTerminated(
      content::SharedWorkerId shared_worker_id) override;
  void OnFinalResponseURLDetermined(content::SharedWorkerId shared_worker_id,
                                    const GURL& url) override;
  void OnClientAdded(
      content::SharedWorkerId shared_worker_id,
      content::GlobalFrameRoutingId render_frame_host_id) override;
  void OnClientRemoved(
      content::SharedWorkerId shared_worker_id,
      content::GlobalFrameRoutingId render_frame_host_id) override;

  // content::ServiceWorkerContextObserver:
  void OnVersionStartedRunning(
      int64_t version_id,
      const content::ServiceWorkerRunningInfo& running_info) override;
  void OnVersionStoppedRunning(int64_t version_id) override;

 private:
  friend class WorkerWatcherTest;

  void AddClientFrame(
      WorkerNodeImpl* worker_node,
      content::GlobalFrameRoutingId client_render_frame_host_id);
  void RemoveClientFrame(
      WorkerNodeImpl* worker_node,
      content::GlobalFrameRoutingId client_render_frame_host_id);

  void OnBeforeFrameNodeRemoved(
      content::GlobalFrameRoutingId render_frame_host_id,
      FrameNodeImpl* frame_node);

  bool AddChildWorker(content::GlobalFrameRoutingId render_frame_host_id,
                      WorkerNodeImpl* child_worker_node);
  bool RemoveChildWorker(content::GlobalFrameRoutingId render_frame_host_id,
                         WorkerNodeImpl* child_worker_node);

  // Helper functions to retrieve an existing worker node.
  WorkerNodeImpl* GetDedicatedWorkerNode(
      content::DedicatedWorkerId dedicated_worker_id);
  WorkerNodeImpl* GetSharedWorkerNode(content::SharedWorkerId shared_worker_id);
  WorkerNodeImpl* GetServiceWorkerNode(int64_t version_id);

  // The ID of the BrowserContext who owns the shared worker service.
  const std::string browser_context_id_;

  // Observes the DedicatedWorkerService for this browser context.
  ScopedObserver<content::DedicatedWorkerService,
                 content::DedicatedWorkerService::Observer>
      dedicated_worker_service_observer_{this};

  // Observes the SharedWorkerService for this browser context.
  ScopedObserver<content::SharedWorkerService,
                 content::SharedWorkerService::Observer>
      shared_worker_service_observer_{this};

  ScopedObserver<content::ServiceWorkerContext,
                 content::ServiceWorkerContextObserver>
      service_worker_context_observer_{this};

  // Used to retrieve an existing process node from its render process ID.
  ProcessNodeSource* const process_node_source_;

  // Used to retrieve an existing frame node from its render process ID and
  // frame ID. Also allows to subscribe to a frame's deletion notification.
  FrameNodeSource* const frame_node_source_;

  // Maps each dedicated worker ID to its worker node.
  base::flat_map<content::DedicatedWorkerId, std::unique_ptr<WorkerNodeImpl>>
      dedicated_worker_nodes_;

  // Maps each shared worker ID to its worker node.
  base::flat_map<content::SharedWorkerId, std::unique_ptr<WorkerNodeImpl>>
      shared_worker_nodes_;

  // Maps each service worker version ID to its worker node.
  base::flat_map<int64_t /*version_id*/, std::unique_ptr<WorkerNodeImpl>>
      service_worker_nodes_;

  // Maps each frame to the shared workers that this frame is a client of. This
  // is used when a frame is torn down before the OnBeforeWorkerTerminated() is
  // received, to ensure the deletion of the worker nodes in the right order
  // (workers before frames).
  base::flat_map<content::GlobalFrameRoutingId, base::flat_set<WorkerNodeImpl*>>
      frame_node_child_workers_;

#if DCHECK_IS_ON()
  // Keeps track of how many OnClientRemoved() calls are expected for an
  // existing worker. This happens when OnBeforeFrameNodeRemoved() is invoked
  // before OnClientRemoved(), or when it wasn't possible to initially attach
  // a client frame node to a worker.
  base::flat_map<WorkerNodeImpl*, int> detached_frame_count_per_worker_;
#endif  // DCHECK_IS_ON()

  DISALLOW_COPY_AND_ASSIGN(WorkerWatcher);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_WORKER_WATCHER_H_
