// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletModuleScriptFetcher_h
#define WorkletModuleScriptFetcher_h

#include "core/loader/modulescript/ModuleScriptFetcher.h"

#include "core/workers/WorkletModuleResponsesMap.h"
#include "core/workers/WorkletModuleResponsesMapProxy.h"
#include "platform/wtf/Optional.h"

namespace blink {

// WorkletModuleScriptFetcher implements the custom fetch logic for worklet's
// module loading defined in:
// https://drafts.css-houdini.org/worklets/#fetch-a-worklet-script
//
// This class works as follows. First, this queries WorkletModuleResponsesMap
// about whether a module script in question has already been cached. If the
// module script is in the map, this retrieves it as ModuleScriptCreationParams
// and returns it to its client. If the module script isn't in the map, this
// fetches the module script via the default module fetch path and then caches
// it in the map.
class CORE_EXPORT WorkletModuleScriptFetcher final
    : public ModuleScriptFetcher,
      public WorkletModuleResponsesMap::Client {
  USING_GARBAGE_COLLECTED_MIXIN(WorkletModuleScriptFetcher);

 public:
  WorkletModuleScriptFetcher(const FetchParameters&,
                             ResourceFetcher*,
                             ModuleScriptFetcher::Client*,
                             WorkletModuleResponsesMapProxy*);

  // Implements ModuleScriptFetcher.
  void Fetch() override;

  // Implements WorkletModuleResponsesMap::Client.
  void OnRead(const ModuleScriptCreationParams&) override;
  void OnFailed() override;

  DECLARE_TRACE();

 private:
  Member<WorkletModuleResponsesMapProxy> module_responses_map_proxy_;
};

}  // namespace blink

#endif  // WorkletModuleScriptFetcher_h
