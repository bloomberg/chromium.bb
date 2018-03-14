// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerOrWorkletModuleScriptFetcher_h
#define WorkerOrWorkletModuleScriptFetcher_h

#include "core/loader/modulescript/ModuleScriptFetcher.h"
#include "core/workers/WorkerOrWorkletModuleFetchCoordinator.h"
#include "core/workers/WorkerOrWorkletModuleFetchCoordinatorProxy.h"
#include "platform/wtf/Optional.h"

namespace blink {

// WorkerOrWorkletModuleScriptFetcher does not initiate module fetch by itself.
// Instead, this delegates it to WorkerOrWorkletModuleFetchCoordinator on the
// main thread via WorkerOrWorkletModuleFetchCoordinatorProxy.
class CORE_EXPORT WorkerOrWorkletModuleScriptFetcher final
    : public ModuleScriptFetcher,
      public WorkerOrWorkletModuleFetchCoordinator::Client {
  USING_GARBAGE_COLLECTED_MIXIN(WorkerOrWorkletModuleScriptFetcher);

 public:
  explicit WorkerOrWorkletModuleScriptFetcher(
      WorkerOrWorkletModuleFetchCoordinatorProxy*);

  // Implements ModuleScriptFetcher.
  void Fetch(FetchParameters&, ModuleScriptFetcher::Client*) override;

  // Implements WorkerOrWorkletModuleFetchCoordinator::Client.
  void OnFetched(const ModuleScriptCreationParams&) override;
  void OnFailed() override;

  void Trace(blink::Visitor*) override;

 private:
  void Finalize(const WTF::Optional<ModuleScriptCreationParams>&,
                const HeapVector<Member<ConsoleMessage>>& error_messages);

  Member<WorkerOrWorkletModuleFetchCoordinatorProxy> coordinator_proxy_;
};

}  // namespace blink

#endif  // WorkerOrWorkletModuleScriptFetcher_h
