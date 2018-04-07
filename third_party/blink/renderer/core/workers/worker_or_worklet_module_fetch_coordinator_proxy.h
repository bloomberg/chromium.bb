// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_OR_WORKLET_MODULE_FETCH_COORDINATOR_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_OR_WORKLET_MODULE_FETCH_COORDINATOR_PROXY_H_

#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_module_fetch_coordinator.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

// WorkerOrWorkletModuleFetchCoordinatorProxy serves as a proxy to talk to
// an implementation of WorkerOrWorkletModuleFetchCoordinator on the main thread
// (outside_settings) from the worker or worklet context thread
// (inside_settings). The constructor and all public functions must be called on
// the context thread.
class CORE_EXPORT WorkerOrWorkletModuleFetchCoordinatorProxy
    : public GarbageCollectedFinalized<
          WorkerOrWorkletModuleFetchCoordinatorProxy> {
 public:
  using Client = WorkerOrWorkletModuleFetchCoordinator::Client;

  static WorkerOrWorkletModuleFetchCoordinatorProxy* Create(
      WorkerOrWorkletModuleFetchCoordinator*,
      scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> inside_settings_task_runner);

  void Fetch(const FetchParameters&, Client*);

  void Trace(blink::Visitor*);

 private:
  WorkerOrWorkletModuleFetchCoordinatorProxy(
      WorkerOrWorkletModuleFetchCoordinator*,
      scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> inside_settings_task_runner);

  void FetchOnMainThread(std::unique_ptr<CrossThreadFetchParametersData>,
                         Client*);

  CrossThreadPersistent<WorkerOrWorkletModuleFetchCoordinator> coordinator_;
  scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> inside_settings_task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_OR_WORKLET_MODULE_FETCH_COORDINATOR_PROXY_H_
