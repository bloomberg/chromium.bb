// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerOrWorkletModuleFetchCoordinator_h
#define WorkerOrWorkletModuleFetchCoordinator_h

#include "core/CoreExport.h"
#include "core/loader/modulescript/ModuleScriptCreationParams.h"
#include "platform/loader/fetch/FetchParameters.h"

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

#endif  // WorkerOrWorkletModuleFetchCoordinator_h
