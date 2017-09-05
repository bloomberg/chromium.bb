// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentModuleScriptFetcher_h
#define DocumentModuleScriptFetcher_h

#include "core/dom/Modulator.h"
#include "core/loader/modulescript/ModuleScriptCreationParams.h"
#include "core/loader/modulescript/ModuleScriptFetcher.h"
#include "core/loader/resource/ScriptResource.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceOwner.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Optional.h"

namespace blink {

class ConsoleMessage;

// DocumentModuleScriptFetcher is used to fetch module scripts used in main
// documents (that means, not worker nor worklets).
//
// DocumentModuleScriptFetcher emits FetchParameters to ResourceFetcher
// (via ScriptResource::Fetch). Then, it keeps track of the fetch progress by
// being a ResourceOwner. Finally, it returns its client a fetched resource as
// ModuleScriptCreationParams.
class CORE_EXPORT DocumentModuleScriptFetcher
    : public ModuleScriptFetcher,
      public ResourceOwner<ScriptResource> {
  USING_GARBAGE_COLLECTED_MIXIN(DocumentModuleScriptFetcher);

 public:
  DocumentModuleScriptFetcher(ResourceFetcher*, ModuleScriptFetcher::Client*);

  void Fetch(FetchParameters&) final;

  // Implements ScriptResourceClient
  void NotifyFinished(Resource*) final;
  String DebugName() const final { return "DocumentModuleScriptFetcher"; }

  DECLARE_TRACE();

 private:
  void Finalize(const WTF::Optional<ModuleScriptCreationParams>&,
                ConsoleMessage* error_message);

  Member<ResourceFetcher> fetcher_;
  bool was_fetched_ = false;
};

}  // namespace blink

#endif  // DocumentModuleScriptFetcher_h
