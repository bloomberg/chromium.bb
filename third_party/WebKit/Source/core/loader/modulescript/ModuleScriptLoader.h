// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleScriptLoader_h
#define ModuleScriptLoader_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/loader/modulescript/ModuleScriptCreationParams.h"
#include "core/loader/modulescript/ModuleScriptFetchRequest.h"
#include "core/loader/modulescript/ModuleScriptFetcher.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

class Modulator;
class ModuleScript;
class ModuleScriptLoaderClient;
class ModuleScriptLoaderRegistry;
enum class ModuleGraphLevel;

// ModuleScriptLoader is responsible for loading a new single ModuleScript.
//
// ModuleScriptLoader constructs FetchParameters and asks ModuleScriptFetcher
// to fetch a script with the parameters. Then, it returns its client a compiled
// ModuleScript.
//
// ModuleScriptLoader(s) should only be used via Modulator and its ModuleMap.
class CORE_EXPORT ModuleScriptLoader final
    : public GarbageCollectedFinalized<ModuleScriptLoader>,
      public ModuleScriptFetcher::Client {
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
                                    const ScriptFetchOptions& options,
                                    ModuleScriptLoaderRegistry* registry,
                                    ModuleScriptLoaderClient* client) {
    return new ModuleScriptLoader(modulator, options, registry, client);
  }

  ~ModuleScriptLoader();

  void Fetch(const ModuleScriptFetchRequest&,
             ModuleGraphLevel);

  // Implements ModuleScriptFetcher::Client.
  void NotifyFetchFinished(
      const WTF::Optional<ModuleScriptCreationParams>&,
      const HeapVector<Member<ConsoleMessage>>& error_messages) override;

  bool IsInitialState() const { return state_ == State::kInitial; }
  bool HasFinished() const { return state_ == State::kFinished; }

  void Trace(blink::Visitor*) override;

 private:
  ModuleScriptLoader(Modulator*,
                     const ScriptFetchOptions&,
                     ModuleScriptLoaderRegistry*,
                     ModuleScriptLoaderClient*);

  void AdvanceState(State new_state);
#if DCHECK_IS_ON()
  static const char* StateToString(State);
#endif

  Member<Modulator> modulator_;
  State state_ = State::kInitial;
  const ScriptFetchOptions options_;
  Member<ModuleScript> module_script_;
  Member<ModuleScriptLoaderRegistry> registry_;
  Member<ModuleScriptLoaderClient> client_;
  Member<ModuleScriptFetcher> module_fetcher_;
#if DCHECK_IS_ON()
  KURL url_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ModuleScriptLoader);
};

}  // namespace blink

#endif
