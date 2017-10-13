// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ModuleMap.h"

#include "core/dom/Modulator.h"
#include "core/dom/ModuleScript.h"
#include "core/loader/modulescript/ModuleScriptFetchRequest.h"
#include "core/loader/modulescript/ModuleScriptLoaderClient.h"
#include "platform/WebTaskRunner.h"

namespace blink {

// Entry struct represents a value in "module map" spec object.
// https://html.spec.whatwg.org/multipage/webappapis.html#module-map
class ModuleMap::Entry final : public GarbageCollectedFinalized<Entry>,
                               public TraceWrapperBase,
                               public ModuleScriptLoaderClient {
  USING_GARBAGE_COLLECTED_MIXIN(ModuleMap::Entry);

 public:
  static Entry* Create(ModuleMap* map) { return new Entry(map); }
  ~Entry() override {}

  DECLARE_TRACE();
  DECLARE_TRACE_WRAPPERS();

  // Notify fetched |m_moduleScript| to the client asynchronously.
  void AddClient(SingleModuleClient*);

  // This is only to be used from ScriptModuleResolver implementations.
  ModuleScript* GetModuleScript() const;

 private:
  explicit Entry(ModuleMap*);

  void DispatchFinishedNotificationAsync(SingleModuleClient*);

  // Implements ModuleScriptLoaderClient
  void NotifyNewSingleModuleFinished(ModuleScript*) override;

  TraceWrapperMember<ModuleScript> module_script_;
  Member<ModuleMap> map_;

  // Correspond to the HTML spec: "fetching" state.
  bool is_fetching_ = true;

  HeapHashSet<Member<SingleModuleClient>> clients_;
};

ModuleMap::Entry::Entry(ModuleMap* map) : map_(map) {
  DCHECK(map_);
}

DEFINE_TRACE(ModuleMap::Entry) {
  visitor->Trace(module_script_);
  visitor->Trace(map_);
  visitor->Trace(clients_);
}

DEFINE_TRACE_WRAPPERS(ModuleMap::Entry) {
  visitor->TraceWrappers(module_script_);
}

void ModuleMap::Entry::DispatchFinishedNotificationAsync(
    SingleModuleClient* client) {
  map_->GetModulator()->TaskRunner()->PostTask(
      BLINK_FROM_HERE,
      WTF::Bind(&SingleModuleClient::NotifyModuleLoadFinished,
                WrapPersistent(client), WrapPersistent(module_script_.Get())));
}

void ModuleMap::Entry::AddClient(SingleModuleClient* new_client) {
  DCHECK(!clients_.Contains(new_client));
  if (!is_fetching_) {
    DCHECK(clients_.IsEmpty());
    DispatchFinishedNotificationAsync(new_client);
    return;
  }

  clients_.insert(new_client);
}

void ModuleMap::Entry::NotifyNewSingleModuleFinished(
    ModuleScript* module_script) {
  CHECK(is_fetching_);
  module_script_ = module_script;
  is_fetching_ = false;

  for (const auto& client : clients_) {
    DispatchFinishedNotificationAsync(client);
  }
  clients_.clear();
}

ModuleScript* ModuleMap::Entry::GetModuleScript() const {
  return module_script_.Get();
}

ModuleMap::ModuleMap(Modulator* modulator) : modulator_(modulator) {
  DCHECK(modulator);
}

DEFINE_TRACE(ModuleMap) {
  visitor->Trace(map_);
  visitor->Trace(modulator_);
}

DEFINE_TRACE_WRAPPERS(ModuleMap) {
  for (const auto& it : map_)
    visitor->TraceWrappers(it.value);
}

void ModuleMap::FetchSingleModuleScript(const ModuleScriptFetchRequest& request,
                                        ModuleGraphLevel level,
                                        SingleModuleClient* client) {
  // https://html.spec.whatwg.org/#fetch-a-single-module-script

  // Step 1. "Let moduleMap be module map settings object's module map."
  // [spec text]
  // Note: |this| is the ModuleMap.

  // Step 2. "If moduleMap[url] is "fetching", wait in parallel until that
  // entry's value changes, then queue a task on the networking task source to
  // proceed with running the following steps." [spec text]
  MapImpl::AddResult result = map_.insert(request.Url(), nullptr);
  TraceWrapperMember<Entry>& entry = result.stored_value->value;
  if (result.is_new_entry) {
    entry = Entry::Create(this);

    // Steps 4-9 loads a new single module script.
    // Delegates to ModuleScriptLoader via Modulator.
    modulator_->FetchNewSingleModule(request, level, entry);
  }
  DCHECK(entry);

  // Step 3. "If moduleMap[url] exists, asynchronously complete this algorithm
  // with moduleMap[url], and abort these steps." [spec text]
  // Step 11. "Set moduleMap[url] to module script, and asynchronously complete
  // this algorithm with module script." [spec text]
  entry->AddClient(client);
}

ModuleScript* ModuleMap::GetFetchedModuleScript(const KURL& url) const {
  MapImpl::const_iterator it = map_.find(url);
  if (it == map_.end())
    return nullptr;
  return it->value->GetModuleScript();
}

}  // namespace blink
