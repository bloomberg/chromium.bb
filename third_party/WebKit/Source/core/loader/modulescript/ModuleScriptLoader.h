// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleScriptLoader_h
#define ModuleScriptLoader_h

#include "core/CoreExport.h"
#include "core/loader/modulescript/ModuleScriptFetchRequest.h"
#include "core/loader/resource/ScriptResource.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/ResourceOwner.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

class Modulator;
class ModuleScript;
class ModuleScriptLoaderClient;
class ModuleScriptLoaderRegistry;
enum class ModuleGraphLevel;

// A ModuleScriptLoader is responsible for loading a new single ModuleScript.
//
// A ModuleScriptLoader constructs and emits FetchParameters to ResourceFetcher
// (via ScriptResource::fetch). Then, it keeps track of the fetch progress by
// being a ScriptResourceClient. Finally, it returns its client a compiled
// ModuleScript.
//
// ModuleScriptLoader(s) should only be used via Modulator and its ModuleMap.
class CORE_EXPORT ModuleScriptLoader final
    : public GarbageCollectedFinalized<ModuleScriptLoader>,
      public ResourceOwner<ScriptResource> {
  WTF_MAKE_NONCOPYABLE(ModuleScriptLoader);
  USING_GARBAGE_COLLECTED_MIXIN(ModuleScriptLoader);

  enum class State {
    kInitial,
    // FetchParameters is being processed, and ModuleScriptLoader hasn't
    // notifyFinished().
    kFetching,
    // Finished successfully or w/ error.
    kFinished,
  };

 public:
  static ModuleScriptLoader* Create(Modulator* modulator,
                                    ModuleScriptLoaderRegistry* registry,
                                    ModuleScriptLoaderClient* client) {
    return new ModuleScriptLoader(modulator, registry, client);
  }

  ~ModuleScriptLoader() override;

  // Note: fetch may notify |m_client| synchronously or asynchronously.
  void Fetch(const ModuleScriptFetchRequest&,
             ResourceFetcher*,
             ModuleGraphLevel);

  bool IsInitialState() const { return state_ == State::kInitial; }
  bool HasFinished() const { return state_ == State::kFinished; }

  DECLARE_TRACE();

 private:
  ModuleScriptLoader(Modulator*,
                     ModuleScriptLoaderRegistry*,
                     ModuleScriptLoaderClient*);

  void AdvanceState(State new_state);
#if DCHECK_IS_ON()
  static const char* StateToString(State);
#endif

  // Implements ScriptResourceClient
  void NotifyFinished(Resource*) override;
  String DebugName() const override { return "ModuleScriptLoader"; }

  Member<Modulator> modulator_;
  State state_ = State::kInitial;
  String nonce_;
  ParserDisposition parser_state_;
  Member<ModuleScript> module_script_;
  Member<ModuleScriptLoaderRegistry> registry_;
  Member<ModuleScriptLoaderClient> client_;
#if DCHECK_IS_ON()
  KURL url_;
#endif
};

}  // namespace blink

#endif
