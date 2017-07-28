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

class CORE_EXPORT WorkletModuleScriptFetcher final
    : public ModuleScriptFetcher,
      public WorkletModuleResponsesMap::Client {
  USING_GARBAGE_COLLECTED_MIXIN(WorkletModuleScriptFetcher);

 public:
  WorkletModuleScriptFetcher(const FetchParameters&,
                             ResourceFetcher*,
                             Modulator*,
                             ModuleScriptFetcher::Client*,
                             WorkletModuleResponsesMapProxy*);

  // Implements ModuleScriptFetcher.
  void Fetch() override;

  // Implements WorkletModuleResponsesMap::Client.
  void OnRead(const ModuleScriptCreationParams&) override;
  void OnFetchNeeded() override;
  void OnFailed() override;

  DECLARE_TRACE();

 private:
  void Finalize(const WTF::Optional<ModuleScriptCreationParams>&) override;

  Member<WorkletModuleResponsesMapProxy> module_responses_map_proxy_;
  bool was_fetched_via_network_ = false;
};

}  // namespace blink

#endif  // WorkletModuleScriptFetcher_h
