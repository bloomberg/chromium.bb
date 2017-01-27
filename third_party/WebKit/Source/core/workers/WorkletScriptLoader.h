// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletScriptLoader_h
#define WorkletScriptLoader_h

#include "core/CoreExport.h"
#include "core/loader/resource/ScriptResource.h"
#include "platform/loader/fetch/ResourceClient.h"
#include "platform/loader/fetch/ResourceOwner.h"

namespace blink {

class ResourceFetcher;
class ScriptSourceCode;

// This class is responsible for fetching and loading a worklet script as a
// classic script. You can access this class only on the main thread.
// A client of this class receives notifications via Client interface.
// TODO(nhiroki): Switch to module script loading (https://crbug.com/627945)
class WorkletScriptLoader final
    : public GarbageCollectedFinalized<WorkletScriptLoader>,
      public ResourceOwner<ScriptResource, ScriptResourceClient> {
  USING_GARBAGE_COLLECTED_MIXIN(WorkletScriptLoader);
  WTF_MAKE_NONCOPYABLE(WorkletScriptLoader);

 public:
  class CORE_EXPORT Client : public GarbageCollectedMixin {
   public:
    // Called when resource loading is completed. If loading is failed or
    // canceled, an empty ScriptSourceCode is passed. You can check if loading
    // is successfully completed by wasScriptLoadSuccessful().
    virtual void notifyWorkletScriptLoadingFinished(
        WorkletScriptLoader*,
        const ScriptSourceCode&) = 0;
  };

  static WorkletScriptLoader* create(ResourceFetcher* fetcher, Client* client) {
    return new WorkletScriptLoader(fetcher, client);
  }

  ~WorkletScriptLoader() override = default;

  // Fetches an URL and loads it as a classic script. Synchronously calls
  // Client::notifyWorkletScriptLoadingFinished() if there is an error.
  void fetchScript(const String& scriptURL);

  // Cancels resource loading and synchronously calls
  // Client::notifyWorkletScriptLoadingFinished().
  void cancel();

  // Returns true if a script was successfully loaded. This should be called
  // after Client::notifyWorkletScriptLoadingFinished() is called.
  bool wasScriptLoadSuccessful() const;

  DECLARE_TRACE();

 private:
  WorkletScriptLoader(ResourceFetcher*, Client*);

  // ResourceClient
  void notifyFinished(Resource*) final;
  String debugName() const final { return "WorkletLoader"; }

  Member<ResourceFetcher> m_fetcher;
  Member<Client> m_client;

  bool m_wasScriptLoadSuccessful = false;
  bool m_wasScriptLoadComplete = false;
};

}  // namespace blink

#endif  // WorkletScriptLoader_h
