// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleScriptFetcher_h
#define ModuleScriptFetcher_h

#include "core/dom/Modulator.h"
#include "core/loader/modulescript/ModuleScriptCreationParams.h"
#include "core/loader/resource/ScriptResource.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceOwner.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Optional.h"

namespace blink {

// ModuleScriptFetcher emits FetchParameters to ResourceFetcher
// (via ScriptResource::Fetch). Then, it keeps track of the fetch progress by
// being a ResourceOwner. Finally, it returns its client a fetched resource as
// ModuleScriptCreationParams.
class CORE_EXPORT ModuleScriptFetcher
    : public GarbageCollectedFinalized<ModuleScriptFetcher>,
      public ResourceOwner<ScriptResource> {
  USING_GARBAGE_COLLECTED_MIXIN(ModuleScriptFetcher);

 public:
  class CORE_EXPORT Client : public GarbageCollectedMixin {
   public:
    virtual void NotifyFetchFinished(
        const WTF::Optional<ModuleScriptCreationParams>&) = 0;
  };

  ModuleScriptFetcher(const FetchParameters&,
                      ResourceFetcher*,
                      Modulator*,
                      Client*);

  // 'virtual' for custom fetch.
  virtual void Fetch();

  // Implements ScriptResourceClient
  void NotifyFinished(Resource*) final;
  String DebugName() const final { return "ModuleScriptFetcher"; }

  DECLARE_TRACE();

 protected:
  virtual void Finalize(const WTF::Optional<ModuleScriptCreationParams>&);

  KURL GetRequestUrl() const { return fetch_params_.Url(); }

 private:
  FetchParameters fetch_params_;
  Member<ResourceFetcher> fetcher_;
  Member<Modulator> modulator_;
  Member<Client> client_;
  bool was_fetched_ = false;
};

}  // namespace blink

#endif  // ModuleScriptFetcher_h
