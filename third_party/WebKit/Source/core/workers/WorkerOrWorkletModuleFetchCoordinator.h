// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_OR_WORKLET_MODULE_FETCH_COORDINATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_OR_WORKLET_MODULE_FETCH_COORDINATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"

namespace blink {

// Subclasses are expected to fetch module scripts via
// DocumentModuleScriptFetcher on the main thread, to manage inflight requests,
// and to notify completions of the requests to
// WorkerOrWorkletModuleScriptFetcher on the worker or worklet context thread.
class WorkerOrWorkletModuleFetchCoordinator : public GarbageCollectedMixin {
 public:
  // Used for notifying results of Fetch().
  class CORE_EXPORT Client : public GarbageCollectedMixin {
   public:
    virtual ~Client() = default;
    virtual void OnFetched(const ModuleScriptCreationParams&) = 0;
    virtual void OnFailed() = 0;
  };

  virtual ~WorkerOrWorkletModuleFetchCoordinator() = default;

  // Fetches a module script for the given parameters, and notifies the client
  // upon completion.
  virtual void Fetch(FetchParameters&, Client*) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_OR_WORKLET_MODULE_FETCH_COORDINATOR_H_
