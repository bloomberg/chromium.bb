// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentModuleScriptFetcher_h
#define DocumentModuleScriptFetcher_h

#include "core/loader/modulescript/ModuleScriptCreationParams.h"
#include "core/loader/modulescript/ModuleScriptFetcher.h"
#include "core/loader/resource/ScriptResource.h"
#include "core/script/Modulator.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Optional.h"

namespace blink {

class ConsoleMessage;

// DocumentModuleScriptFetcher is used to fetch module scripts used in main
// documents (that means, not worker nor worklets).
//
// DocumentModuleScriptFetcher emits FetchParameters to ResourceFetcher
// (via ScriptResource::Fetch). Then, it keeps track of the fetch progress by
// being a ResourceClient. Finally, it returns its client a fetched resource as
// ModuleScriptCreationParams.
class CORE_EXPORT DocumentModuleScriptFetcher : public ModuleScriptFetcher,
                                                public ResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(DocumentModuleScriptFetcher);

 public:
  explicit DocumentModuleScriptFetcher(ResourceFetcher*);

  void Fetch(FetchParameters&, ModuleScriptFetcher::Client*) final;

  // Implements ResourceClient
  void NotifyFinished(Resource*) final;
  String DebugName() const final { return "DocumentModuleScriptFetcher"; }

  void Trace(blink::Visitor*) override;

 private:
  void Finalize(const WTF::Optional<ModuleScriptCreationParams>&,
                const HeapVector<Member<ConsoleMessage>>& error_messages);

  Member<ResourceFetcher> fetcher_;
};

}  // namespace blink

#endif  // DocumentModuleScriptFetcher_h
