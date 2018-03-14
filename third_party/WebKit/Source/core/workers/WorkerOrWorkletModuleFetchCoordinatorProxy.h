// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerOrWorkletModuleFetchCoordinatorProxy_h
#define WorkerOrWorkletModuleFetchCoordinatorProxy_h

#include "base/single_thread_task_runner.h"
#include "core/CoreExport.h"
#include "core/workers/WorkerOrWorkletModuleFetchCoordinator.h"
#include "platform/heap/Heap.h"

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

#endif  // WorkerOrWorkletModuleFetchCoordinatorProxy_h
