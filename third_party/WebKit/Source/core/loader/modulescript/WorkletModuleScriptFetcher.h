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
// WorkletModuleScriptFetcher does not initiate module fetch by itself. This
// asks WorkletModuleResponsesMap on the main thread via
// WorkletModuleResponsesMapProxy to read a cached module script or to fetch it
// via network. See comments on WorkletModuleResponsesMap for details.
class CORE_EXPORT WorkletModuleScriptFetcher final
    : public ModuleScriptFetcher,
      public WorkletModuleResponsesMap::Client {
  USING_GARBAGE_COLLECTED_MIXIN(WorkletModuleScriptFetcher);

 public:
  WorkletModuleScriptFetcher(ModuleScriptFetcher::Client*,
                             WorkletModuleResponsesMapProxy*);

  // Implements ModuleScriptFetcher.
  void Fetch(FetchParameters&) override;

  // Implements WorkletModuleResponsesMap::Client.
  void OnRead(const ModuleScriptCreationParams&) override;
  void OnFailed() override;

  DECLARE_TRACE();

 private:
  void Finalize(const WTF::Optional<ModuleScriptCreationParams>&,
                ConsoleMessage* error_message);

  Member<WorkletModuleResponsesMapProxy> module_responses_map_proxy_;
};

}  // namespace blink

#endif  // WorkletModuleScriptFetcher_h
