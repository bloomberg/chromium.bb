/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/web/WebGeolocationPermissionRequestManager.h"

#include "modules/geolocation/Geolocation.h"
#include "platform/heap/Handle.h"
#include "public/web/WebGeolocationPermissionRequest.h"
#include "wtf/HashMap.h"

namespace blink {

typedef HeapHashMap<Member<Geolocation>, int> GeolocationIdMap;
typedef HeapHashMap<int, Member<Geolocation>> IdGeolocationMap;

class WebGeolocationPermissionRequestManagerPrivate final : public GarbageCollected<WebGeolocationPermissionRequestManagerPrivate> {
public:
    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_geolocationIdMap);
        visitor->trace(m_idGeolocationMap);
    }

    GeolocationIdMap m_geolocationIdMap;
    IdGeolocationMap m_idGeolocationMap;
};

int WebGeolocationPermissionRequestManager::add(const WebGeolocationPermissionRequest& permissionRequest)
{
    Geolocation* geolocation = permissionRequest.geolocation();
    WebGeolocationPermissionRequestManagerPrivate* manager = ensureManager();
    ASSERT(!manager->m_geolocationIdMap.contains(geolocation));
    static int lastId;
    int id = ++lastId;
    manager->m_geolocationIdMap.add(geolocation, id);
    manager->m_idGeolocationMap.add(id, geolocation);
    return id;
}

bool WebGeolocationPermissionRequestManager::remove(const WebGeolocationPermissionRequest& permissionRequest, int& id)
{
    Geolocation* geolocation = permissionRequest.geolocation();
    WebGeolocationPermissionRequestManagerPrivate* manager = ensureManager();
    GeolocationIdMap::iterator it = manager->m_geolocationIdMap.find(geolocation);
    if (it == manager->m_geolocationIdMap.end())
        return false;
    id = it->value;
    manager->m_geolocationIdMap.remove(it);
    manager->m_idGeolocationMap.remove(id);
    return true;
}

bool WebGeolocationPermissionRequestManager::remove(int id, WebGeolocationPermissionRequest& permissionRequest)
{
    WebGeolocationPermissionRequestManagerPrivate* manager = ensureManager();
    IdGeolocationMap::iterator it = manager->m_idGeolocationMap.find(id);
    if (it == manager->m_idGeolocationMap.end())
        return false;
    Geolocation* geolocation = it->value;
    permissionRequest = WebGeolocationPermissionRequest(geolocation);
    manager->m_idGeolocationMap.remove(it);
    manager->m_geolocationIdMap.remove(geolocation);
    return true;
}

WebGeolocationPermissionRequestManagerPrivate* WebGeolocationPermissionRequestManager::ensureManager()
{
    if (m_private.isNull())
        m_private = new WebGeolocationPermissionRequestManagerPrivate;

    return m_private.get();
}

void WebGeolocationPermissionRequestManager::reset()
{
    m_private.reset();
}

} // namespace blink
