// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_MODULE_FETCH_COORDINATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_MODULE_FETCH_COORDINATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_module_fetch_coordinator.h"

namespace blink {

class ResourceFetcher;

// An implementation of WorkerOrWorkletModuleFetchCoordinator for
// WorkerGlobalScopes. When the parent context gets destroyed, inflight requests
// are supposed to be canceled via Dispose(). This lives on the main thread.
class CORE_EXPORT WorkerModuleFetchCoordinator final
    : public GarbageCollectedFinalized<WorkerModuleFetchCoordinator>,
      public WorkerOrWorkletModuleFetchCoordinator {
  USING_GARBAGE_COLLECTED_MIXIN(WorkerModuleFetchCoordinator);

 public:
  static WorkerModuleFetchCoordinator* Create(ResourceFetcher*);

  void Fetch(FetchParameters&, Client*) override;

  // Called when the associated document is destroyed. Aborts all waiting
  // clients and clears the map.
  void Dispose();

  void Trace(blink::Visitor*) override;

 private:
  class Request;

  explicit WorkerModuleFetchCoordinator(ResourceFetcher*);

  void OnFetched(Request*);

  Member<ResourceFetcher> resource_fetcher_;

  HeapHashSet<Member<Request>> inflight_requests_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_MODULE_FETCH_COORDINATOR_H_
