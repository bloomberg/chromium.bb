// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ModuleMap.h"

#include "core/dom/Modulator.h"
#include "core/dom/ModuleScript.h"
#include "core/dom/ScriptModuleResolver.h"
#include "core/loader/modulescript/ModuleScriptFetchRequest.h"
#include "core/loader/modulescript/ModuleScriptLoaderClient.h"
#include "platform/WebTaskRunner.h"

namespace blink {

// Entry struct represents a value in "module map" spec object.
// https://html.spec.whatwg.org/multipage/webappapis.html#module-map
class ModuleMap::Entry final : public GarbageCollectedFinalized<Entry>,
                               public ModuleScriptLoaderClient {
  USING_GARBAGE_COLLECTED_MIXIN(ModuleMap::Entry);

 public:
  static Entry* create(ModuleMap* map) { return new Entry(map); }
  ~Entry() override {}

  DECLARE_TRACE();

  // Notify fetched |m_moduleScript| to the client asynchronously.
  void addClient(SingleModuleClient*);

  // This is only to be used from ScriptModuleResolver implementations.
  ModuleScript* getModuleScript() const;

 private:
  explicit Entry(ModuleMap*);

  void dispatchFinishedNotificationAsync(SingleModuleClient*);

  // Implements ModuleScriptLoaderClient
  void notifyNewSingleModuleFinished(ModuleScript*) override;

  Member<ModuleScript> m_moduleScript;
  Member<ModuleMap> m_map;

  // Correspond to the HTML spec: "fetching" state.
  bool m_isFetching = true;

  HeapHashSet<Member<SingleModuleClient>> m_clients;
};

ModuleMap::Entry::Entry(ModuleMap* map) : m_map(map) {
  DCHECK(m_map);
}

DEFINE_TRACE(ModuleMap::Entry) {
  visitor->trace(m_moduleScript);
  visitor->trace(m_map);
  visitor->trace(m_clients);
}

void ModuleMap::Entry::dispatchFinishedNotificationAsync(
    SingleModuleClient* client) {
  m_map->modulator()->taskRunner()->postTask(
      BLINK_FROM_HERE,
      WTF::bind(&SingleModuleClient::notifyModuleLoadFinished,
                wrapPersistent(client), wrapPersistent(m_moduleScript.get())));
}

void ModuleMap::Entry::addClient(SingleModuleClient* newClient) {
  DCHECK(!m_clients.contains(newClient));
  if (!m_isFetching) {
    DCHECK(m_clients.isEmpty());
    dispatchFinishedNotificationAsync(newClient);
    return;
  }

  m_clients.insert(newClient);
}

void ModuleMap::Entry::notifyNewSingleModuleFinished(
    ModuleScript* moduleScript) {
  CHECK(m_isFetching);
  m_moduleScript = moduleScript;
  m_isFetching = false;

  if (m_moduleScript) {
    m_map->modulator()->scriptModuleResolver()->registerModuleScript(
        m_moduleScript);
  }

  for (const auto& client : m_clients) {
    dispatchFinishedNotificationAsync(client);
  }
  m_clients.clear();
}

ModuleScript* ModuleMap::Entry::getModuleScript() const {
  DCHECK(!m_isFetching);
  return m_moduleScript.get();
}

ModuleMap::ModuleMap(Modulator* modulator) : m_modulator(modulator) {
  DCHECK(modulator);
}

DEFINE_TRACE(ModuleMap) {
  visitor->trace(m_map);
  visitor->trace(m_modulator);
}

void ModuleMap::fetchSingleModuleScript(const ModuleScriptFetchRequest& request,
                                        ModuleGraphLevel level,
                                        SingleModuleClient* client) {
  // https://html.spec.whatwg.org/#fetch-a-single-module-script

  // Step 1. Let moduleMap be module map settings object's module map.
  // Note: This is the ModuleMap.

  // Step 2. If moduleMap[url] is "fetching", wait in parallel until that
  // entry's value changes, then queue a task on the networking task source to
  // proceed with running the following steps.
  MapImpl::AddResult result = m_map.insert(request.url(), nullptr);
  Member<Entry>& entry = result.storedValue->value;
  if (result.isNewEntry) {
    entry = Entry::create(this);

    // Steps 4-9 loads a new single module script.
    // Delegates to ModuleScriptLoader via Modulator.
    m_modulator->fetchNewSingleModule(request, level, entry);
  }
  DCHECK(entry);

  // Step 3. If moduleMap[url] exists, asynchronously complete this algorithm
  // with moduleMap[url], and abort these steps.
  // Step 10. Set moduleMap[url] to module script, and asynchronously complete
  // this algorithm with module script.
  entry->addClient(client);
}

ModuleScript* ModuleMap::getFetchedModuleScript(const KURL& url) const {
  MapImpl::const_iterator it = m_map.find(url);
  CHECK_NE(it, m_map.end());
  return it->value->getModuleScript();
}

}  // namespace blink
