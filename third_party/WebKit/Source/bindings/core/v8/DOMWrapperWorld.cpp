/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/core/v8/DOMWrapperWorld.h"

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMActivityLogger.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8Window.h"
#include "bindings/core/v8/WindowProxy.h"
#include "bindings/core/v8/WrapperTypeInfo.h"
#include "core/dom/ExecutionContext.h"
#include "wtf/HashTraits.h"
#include "wtf/PtrUtil.h"
#include "wtf/StdLibExtras.h"
#include <memory>

namespace blink {

class DOMObjectHolderBase {
  USING_FAST_MALLOC(DOMObjectHolderBase);

 public:
  DOMObjectHolderBase(v8::Isolate* isolate, v8::Local<v8::Value> wrapper)
      : m_wrapper(isolate, wrapper), m_world(0) {}
  virtual ~DOMObjectHolderBase() {}

  DOMWrapperWorld* world() const { return m_world; }
  void setWorld(DOMWrapperWorld* world) { m_world = world; }
  void setWeak(
      void (*callback)(const v8::WeakCallbackInfo<DOMObjectHolderBase>&)) {
    m_wrapper.setWeak(this, callback);
  }

 private:
  ScopedPersistent<v8::Value> m_wrapper;
  DOMWrapperWorld* m_world;
};

template <typename T>
class DOMObjectHolder : public DOMObjectHolderBase {
 public:
  static std::unique_ptr<DOMObjectHolder<T>>
  create(v8::Isolate* isolate, T* object, v8::Local<v8::Value> wrapper) {
    return WTF::wrapUnique(new DOMObjectHolder(isolate, object, wrapper));
  }

 private:
  DOMObjectHolder(v8::Isolate* isolate, T* object, v8::Local<v8::Value> wrapper)
      : DOMObjectHolderBase(isolate, wrapper), m_object(object) {}

  Persistent<T> m_object;
};

unsigned DOMWrapperWorld::isolatedWorldCount = 0;

PassRefPtr<DOMWrapperWorld> DOMWrapperWorld::create(v8::Isolate* isolate,
                                                    int worldId) {
  return adoptRef(new DOMWrapperWorld(isolate, worldId));
}

DOMWrapperWorld::DOMWrapperWorld(v8::Isolate* isolate, int worldId)
    : m_worldId(worldId),
      m_domDataStore(
          WTF::wrapUnique(new DOMDataStore(isolate, isMainWorld()))) {
  if (worldId == WorkerWorldId) {
    workerWorld() = this;
  }
}

DOMWrapperWorld& DOMWrapperWorld::mainWorld() {
  ASSERT(isMainThread());
  DEFINE_STATIC_REF(
      DOMWrapperWorld, cachedMainWorld,
      (DOMWrapperWorld::create(v8::Isolate::GetCurrent(), MainWorldId)));
  return *cachedMainWorld;
}

DOMWrapperWorld*& DOMWrapperWorld::workerWorld() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<DOMWrapperWorld*>, workerWorld,
                                  new ThreadSpecific<DOMWrapperWorld*>);
  return *workerWorld;
}

PassRefPtr<DOMWrapperWorld> DOMWrapperWorld::fromWorldId(v8::Isolate* isolate,
                                                         int worldId) {
  if (worldId == MainWorldId)
    return &mainWorld();
  return ensureIsolatedWorld(isolate, worldId);
}

typedef HashMap<int, DOMWrapperWorld*> WorldMap;
static WorldMap& isolatedWorldMap() {
  ASSERT(isMainThread());
  DEFINE_STATIC_LOCAL(WorldMap, map, ());
  return map;
}

void DOMWrapperWorld::allWorldsInMainThread(
    Vector<RefPtr<DOMWrapperWorld>>& worlds) {
  ASSERT(isMainThread());
  worlds.push_back(&mainWorld());
  WorldMap& isolatedWorlds = isolatedWorldMap();
  for (WorldMap::iterator it = isolatedWorlds.begin();
       it != isolatedWorlds.end(); ++it)
    worlds.push_back(it->value);
}

void DOMWrapperWorld::markWrappersInAllWorlds(
    ScriptWrappable* scriptWrappable,
    const ScriptWrappableVisitor* visitor) {
  // Handle marking in per-worker wrapper worlds.
  if (!isMainThread()) {
    DCHECK(ThreadState::current()->isolate());
    DOMWrapperWorld* worker = workerWorld();
    if (worker) {
      DOMDataStore& dataStore = worker->domDataStore();
      if (dataStore.containsWrapper(scriptWrappable)) {
        dataStore.markWrapper(scriptWrappable);
      }
    }
    return;
  }

  scriptWrappable->markWrapper(visitor);
  WorldMap& isolatedWorlds = isolatedWorldMap();
  for (auto& world : isolatedWorlds.values()) {
    DOMDataStore& dataStore = world->domDataStore();
    if (dataStore.containsWrapper(scriptWrappable)) {
      // Marking for the isolated worlds
      dataStore.markWrapper(scriptWrappable);
    }
  }
}

DOMWrapperWorld::~DOMWrapperWorld() {
  ASSERT(!isMainWorld());

  dispose();

  if (!isIsolatedWorld())
    return;

  WorldMap& map = isolatedWorldMap();
  WorldMap::iterator it = map.find(m_worldId);
  if (it == map.end()) {
    ASSERT_NOT_REACHED();
    return;
  }
  ASSERT(it->value == this);

  map.remove(it);
  isolatedWorldCount--;
}

void DOMWrapperWorld::dispose() {
  m_domObjectHolders.clear();
  m_domDataStore.reset();
  if (isWorkerWorld())
    workerWorld() = nullptr;
}

#if DCHECK_IS_ON()
static bool isIsolatedWorldId(int worldId) {
  return MainWorldId < worldId && worldId < IsolatedWorldIdLimit;
}
#endif

PassRefPtr<DOMWrapperWorld> DOMWrapperWorld::ensureIsolatedWorld(
    v8::Isolate* isolate,
    int worldId) {
  ASSERT(isIsolatedWorldId(worldId));

  WorldMap& map = isolatedWorldMap();
  WorldMap::AddResult result = map.insert(worldId, nullptr);
  RefPtr<DOMWrapperWorld> world = result.storedValue->value;
  if (world) {
    ASSERT(world->worldId() == worldId);
    return world.release();
  }

  world = DOMWrapperWorld::create(isolate, worldId);
  result.storedValue->value = world.get();
  isolatedWorldCount++;
  return world.release();
}

typedef HashMap<int, RefPtr<SecurityOrigin>> IsolatedWorldSecurityOriginMap;
static IsolatedWorldSecurityOriginMap& isolatedWorldSecurityOrigins() {
  ASSERT(isMainThread());
  DEFINE_STATIC_LOCAL(IsolatedWorldSecurityOriginMap, map, ());
  return map;
}

SecurityOrigin* DOMWrapperWorld::isolatedWorldSecurityOrigin() {
  ASSERT(this->isIsolatedWorld());
  IsolatedWorldSecurityOriginMap& origins = isolatedWorldSecurityOrigins();
  IsolatedWorldSecurityOriginMap::iterator it = origins.find(worldId());
  return it == origins.end() ? 0 : it->value.get();
}

void DOMWrapperWorld::setIsolatedWorldSecurityOrigin(
    int worldId,
    PassRefPtr<SecurityOrigin> securityOrigin) {
  ASSERT(isIsolatedWorldId(worldId));
  if (securityOrigin)
    isolatedWorldSecurityOrigins().set(worldId, std::move(securityOrigin));
  else
    isolatedWorldSecurityOrigins().remove(worldId);
}

typedef HashMap<int, String> IsolatedWorldHumanReadableNameMap;
static IsolatedWorldHumanReadableNameMap& isolatedWorldHumanReadableNames() {
  ASSERT(isMainThread());
  DEFINE_STATIC_LOCAL(IsolatedWorldHumanReadableNameMap, map, ());
  return map;
}

String DOMWrapperWorld::isolatedWorldHumanReadableName() {
  ASSERT(this->isIsolatedWorld());
  return isolatedWorldHumanReadableNames().at(worldId());
}

void DOMWrapperWorld::setIsolatedWorldHumanReadableName(
    int worldId,
    const String& humanReadableName) {
  ASSERT(isIsolatedWorldId(worldId));
  isolatedWorldHumanReadableNames().set(worldId, humanReadableName);
}

typedef HashMap<int, bool> IsolatedWorldContentSecurityPolicyMap;
static IsolatedWorldContentSecurityPolicyMap&
isolatedWorldContentSecurityPolicies() {
  ASSERT(isMainThread());
  DEFINE_STATIC_LOCAL(IsolatedWorldContentSecurityPolicyMap, map, ());
  return map;
}

bool DOMWrapperWorld::isolatedWorldHasContentSecurityPolicy() {
  ASSERT(this->isIsolatedWorld());
  IsolatedWorldContentSecurityPolicyMap& policies =
      isolatedWorldContentSecurityPolicies();
  IsolatedWorldContentSecurityPolicyMap::iterator it = policies.find(worldId());
  return it == policies.end() ? false : it->value;
}

void DOMWrapperWorld::setIsolatedWorldContentSecurityPolicy(
    int worldId,
    const String& policy) {
  ASSERT(isIsolatedWorldId(worldId));
  if (!policy.isEmpty())
    isolatedWorldContentSecurityPolicies().set(worldId, true);
  else
    isolatedWorldContentSecurityPolicies().remove(worldId);
}

template <typename T>
void DOMWrapperWorld::registerDOMObjectHolder(v8::Isolate* isolate,
                                              T* object,
                                              v8::Local<v8::Value> wrapper) {
  registerDOMObjectHolderInternal(
      DOMObjectHolder<T>::create(isolate, object, wrapper));
}

template void DOMWrapperWorld::registerDOMObjectHolder(v8::Isolate*,
                                                       ScriptFunction*,
                                                       v8::Local<v8::Value>);

void DOMWrapperWorld::registerDOMObjectHolderInternal(
    std::unique_ptr<DOMObjectHolderBase> holderBase) {
  ASSERT(!m_domObjectHolders.contains(holderBase.get()));
  holderBase->setWorld(this);
  holderBase->setWeak(&DOMWrapperWorld::weakCallbackForDOMObjectHolder);
  m_domObjectHolders.insert(std::move(holderBase));
}

void DOMWrapperWorld::unregisterDOMObjectHolder(
    DOMObjectHolderBase* holderBase) {
  ASSERT(m_domObjectHolders.contains(holderBase));
  m_domObjectHolders.erase(holderBase);
}

void DOMWrapperWorld::weakCallbackForDOMObjectHolder(
    const v8::WeakCallbackInfo<DOMObjectHolderBase>& data) {
  DOMObjectHolderBase* holderBase = data.GetParameter();
  holderBase->world()->unregisterDOMObjectHolder(holderBase);
}

}  // namespace blink
