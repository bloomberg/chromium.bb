// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerModuleFetchCoordinator_h
#define WorkerModuleFetchCoordinator_h

#include "core/CoreExport.h"
#include "core/workers/WorkerOrWorkletModuleFetchCoordinator.h"

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

#endif  // WorkerModuleFetchCoordinator_h
