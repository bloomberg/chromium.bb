// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleScriptLoader_h
#define ModuleScriptLoader_h

#include "core/CoreExport.h"
#include "core/loader/modulescript/ModuleScriptFetchRequest.h"
#include "core/loader/resource/ScriptResource.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
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
// A ModuleScriptLoader constructs and emits FetchRequest to ResourceFetcher
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
    Initial,
    // FetchRequest is being processed, and ModuleScriptLoader hasn't
    // notifyFinished().
    Fetching,
    // Finished successfully or w/ error.
    Finished,
  };

 public:
  static ModuleScriptLoader* create(Modulator* modulator,
                                    ModuleScriptLoaderRegistry* registry,
                                    ModuleScriptLoaderClient* client) {
    return new ModuleScriptLoader(modulator, registry, client);
  }

  ~ModuleScriptLoader() override;

  // Note: fetch may notify |m_client| synchronously or asynchronously.
  void fetch(const ModuleScriptFetchRequest&,
             ResourceFetcher*,
             ModuleGraphLevel);

  bool isInitialState() const { return m_state == State::Initial; }
  bool hasFinished() const { return m_state == State::Finished; }

  DECLARE_TRACE();

 private:
  ModuleScriptLoader(Modulator*,
                     ModuleScriptLoaderRegistry*,
                     ModuleScriptLoaderClient*);

  static ModuleScript* createModuleScript(const String& sourceText,
                                          const KURL&,
                                          Modulator*,
                                          const String& nonce,
                                          ParserDisposition,
                                          WebURLRequest::FetchCredentialsMode);

  void advanceState(State newState);
#if DCHECK_IS_ON()
  static const char* stateToString(State);
#endif

  // Implements ScriptResourceClient
  void notifyFinished(Resource*) override;
  String debugName() const override { return "ModuleScriptLoader"; }

  static bool wasModuleLoadSuccessful(Resource*);

  Member<Modulator> m_modulator;
  State m_state = State::Initial;
  String m_nonce;
  ParserDisposition m_parserState;
  Member<ModuleScript> m_moduleScript;
  Member<ModuleScriptLoaderRegistry> m_registry;
  Member<ModuleScriptLoaderClient> m_client;
#if DCHECK_IS_ON()
  KURL m_url;
#endif
};

}  // namespace blink

#endif
