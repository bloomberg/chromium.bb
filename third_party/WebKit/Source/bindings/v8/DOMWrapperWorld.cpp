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

#include "config.h"
#include "bindings/v8/DOMWrapperWorld.h"

#include "V8Window.h"
#include "bindings/v8/DOMDataStore.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8DOMActivityLogger.h"
#include "bindings/v8/V8DOMWrapper.h"
#include "bindings/v8/V8WindowShell.h"
#include "bindings/v8/WrapperTypeInfo.h"
#include "core/dom/ExecutionContext.h"
#include "wtf/HashTraits.h"
#include "wtf/MainThread.h"
#include "wtf/StdLibExtras.h"

namespace WebCore {

unsigned DOMWrapperWorld::isolatedWorldCount = 0;

PassRefPtr<DOMWrapperWorld> DOMWrapperWorld::create(int worldId, int extensionGroup)
{
    return adoptRef(new DOMWrapperWorld(worldId, extensionGroup));
}

DOMWrapperWorld::DOMWrapperWorld(int worldId, int extensionGroup)
    : m_worldId(worldId)
    , m_extensionGroup(extensionGroup)
{
    if (isMainWorld())
        m_domDataStore = adoptPtr(new DOMDataStore(MainWorld));
    else if (isIsolatedWorld())
        m_domDataStore = adoptPtr(new DOMDataStore(IsolatedWorld));
    else
        m_domDataStore = adoptPtr(new DOMDataStore(WorkerWorld));
}

DOMWrapperWorld* DOMWrapperWorld::current(v8::Isolate* isolate)
{
    ASSERT(isolate->InContext());
    v8::Handle<v8::Context> context = isolate->GetCurrentContext();
    if (!toDOMWindow(context))
        return 0;
    ASSERT(isMainThread());
    if (DOMWrapperWorld* world = isolatedWorld(context))
        return world;
    return mainWorld();
}

DOMWrapperWorld* DOMWrapperWorld::mainWorld()
{
    DEFINE_STATIC_REF(DOMWrapperWorld, mainWorld, (DOMWrapperWorld::create(MainWorldId, mainWorldExtensionGroup)));
    return mainWorld;
}

typedef HashMap<int, DOMWrapperWorld*> WorldMap;
static WorldMap& isolatedWorldMap()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(WorldMap, map, ());
    return map;
}

void DOMWrapperWorld::getAllWorldsInMainThread(Vector<RefPtr<DOMWrapperWorld> >& worlds)
{
    ASSERT(isMainThread());
    worlds.append(mainWorld());
    WorldMap& isolatedWorlds = isolatedWorldMap();
    for (WorldMap::iterator it = isolatedWorlds.begin(); it != isolatedWorlds.end(); ++it)
        worlds.append(it->value);
}

DOMWrapperWorld::~DOMWrapperWorld()
{
    ASSERT(!isMainWorld());

    if (!isIsolatedWorld())
        return;

    WorldMap& map = isolatedWorldMap();
    WorldMap::iterator i = map.find(m_worldId);
    if (i == map.end()) {
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT(i->value == this);

    map.remove(i);
    isolatedWorldCount--;
    ASSERT(map.size() == isolatedWorldCount);
}

PassRefPtr<DOMWrapperWorld> DOMWrapperWorld::ensureIsolatedWorld(int worldId, int extensionGroup)
{
    ASSERT(isIsolatedWorldId(worldId));

    WorldMap& map = isolatedWorldMap();
    WorldMap::AddResult result = map.add(worldId, 0);
    RefPtr<DOMWrapperWorld> world = result.iterator->value;
    if (world) {
        ASSERT(world->worldId() == worldId);
        ASSERT(world->extensionGroup() == extensionGroup);
        return world.release();
    }

    world = DOMWrapperWorld::create(worldId, extensionGroup);
    result.iterator->value = world.get();
    isolatedWorldCount++;
    ASSERT(map.size() == isolatedWorldCount);

    return world.release();
}

v8::Handle<v8::Context> DOMWrapperWorld::context(ScriptController& controller)
{
    return controller.windowShell(this)->context();
}

typedef HashMap<int, RefPtr<SecurityOrigin> > IsolatedWorldSecurityOriginMap;
static IsolatedWorldSecurityOriginMap& isolatedWorldSecurityOrigins()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(IsolatedWorldSecurityOriginMap, map, ());
    return map;
}

SecurityOrigin* DOMWrapperWorld::isolatedWorldSecurityOrigin()
{
    ASSERT(this->isIsolatedWorld());
    IsolatedWorldSecurityOriginMap& origins = isolatedWorldSecurityOrigins();
    IsolatedWorldSecurityOriginMap::iterator it = origins.find(worldId());
    return it == origins.end() ? 0 : it->value.get();
}

void DOMWrapperWorld::setIsolatedWorldSecurityOrigin(int worldId, PassRefPtr<SecurityOrigin> securityOrigin)
{
    ASSERT(DOMWrapperWorld::isIsolatedWorldId(worldId));
    if (securityOrigin)
        isolatedWorldSecurityOrigins().set(worldId, securityOrigin);
    else
        isolatedWorldSecurityOrigins().remove(worldId);
}

void DOMWrapperWorld::clearIsolatedWorldSecurityOrigin(int worldId)
{
    ASSERT(DOMWrapperWorld::isIsolatedWorldId(worldId));
    isolatedWorldSecurityOrigins().remove(worldId);
}

typedef HashMap<int, bool> IsolatedWorldContentSecurityPolicyMap;
static IsolatedWorldContentSecurityPolicyMap& isolatedWorldContentSecurityPolicies()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(IsolatedWorldContentSecurityPolicyMap, map, ());
    return map;
}

bool DOMWrapperWorld::isolatedWorldHasContentSecurityPolicy()
{
    ASSERT(this->isIsolatedWorld());
    IsolatedWorldContentSecurityPolicyMap& policies = isolatedWorldContentSecurityPolicies();
    IsolatedWorldContentSecurityPolicyMap::iterator it = policies.find(worldId());
    return it == policies.end() ? false : it->value;
}

void DOMWrapperWorld::setIsolatedWorldContentSecurityPolicy(int worldId, const String& policy)
{
    ASSERT(DOMWrapperWorld::isIsolatedWorldId(worldId));
    if (!policy.isEmpty())
        isolatedWorldContentSecurityPolicies().set(worldId, true);
    else
        isolatedWorldContentSecurityPolicies().remove(worldId);
}

void DOMWrapperWorld::clearIsolatedWorldContentSecurityPolicy(int worldId)
{
    ASSERT(DOMWrapperWorld::isIsolatedWorldId(worldId));
    isolatedWorldContentSecurityPolicies().remove(worldId);
}

typedef HashMap<int, OwnPtr<V8DOMActivityLogger>, WTF::IntHash<int>, WTF::UnsignedWithZeroKeyHashTraits<int> > DOMActivityLoggerMap;
static DOMActivityLoggerMap& domActivityLoggers()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(DOMActivityLoggerMap, map, ());
    return map;
}

void DOMWrapperWorld::setActivityLogger(int worldId, PassOwnPtr<V8DOMActivityLogger> logger)
{
    domActivityLoggers().set(worldId, logger);
}

V8DOMActivityLogger* DOMWrapperWorld::activityLogger(int worldId)
{
    DOMActivityLoggerMap& loggers = domActivityLoggers();
    DOMActivityLoggerMap::iterator it = loggers.find(worldId);
    return it == loggers.end() ? 0 : it->value.get();
}

bool DOMWrapperWorld::contextHasCorrectPrototype(v8::Handle<v8::Context> context)
{
    return V8WindowShell::contextHasCorrectPrototype(context);
}

} // namespace WebCore
