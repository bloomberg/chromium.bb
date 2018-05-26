// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_WORKLET_MODULE_SCRIPT_FETCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_WORKLET_MODULE_SCRIPT_FETCHER_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetcher.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"

namespace blink {

// WorkletModuleScriptFetcher either fetchs a cached result from
// WorkletModuleResponsesMap, or defers to ModuleScriptFetcher and
// stores the result in WorkletModuleResponsesMap on fetch completion.
class CORE_EXPORT WorkletModuleScriptFetcher final
    : public ModuleScriptFetcher,
      private ModuleScriptFetcher::Client {
  USING_GARBAGE_COLLECTED_MIXIN(WorkletModuleScriptFetcher);

 public:
  WorkletModuleScriptFetcher(ResourceFetcher*, WorkletModuleResponsesMap*);

  // Implements ModuleScriptFetcher.
  void Fetch(FetchParameters&, ModuleScriptFetcher::Client*) override;

  void Trace(blink::Visitor*) override;

 private:
  // Implements ModuleScriptFetcher::Client.
  void NotifyFetchFinished(
      const base::Optional<ModuleScriptCreationParams>&,
      const HeapVector<Member<ConsoleMessage>>& error_messages) override;

  CrossThreadPersistent<WorkletModuleResponsesMap> module_responses_map_;
  KURL url_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_WORKLET_MODULE_SCRIPT_FETCHER_H_
