/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef DOMWrapperMap_h
#define DOMWrapperMap_h

#include "bindings/v8/UnsafePersistent.h"
#include "bindings/v8/V8Utilities.h"
#include "bindings/v8/WrapperTypeInfo.h"
#include <v8.h>
#include "wtf/HashMap.h"

namespace WebCore {

template<class KeyType>
class DOMWrapperMap {
public:
    typedef HashMap<KeyType*, UnsafePersistent<v8::Object> > MapType;

    explicit DOMWrapperMap(v8::Isolate* isolate)
        : m_isolate(isolate)
    {
    }

    v8::Handle<v8::Object> get(KeyType* key)
    {
        return m_map.get(key).deprecatedHandle();
    }

    v8::Handle<v8::Object> getNewLocal(v8::Isolate* isolate, KeyType* key)
    {
        return m_map.get(key).newLocal(isolate);
    }

    void set(KeyType* key, v8::Handle<v8::Object> wrapper, const WrapperConfiguration& configuration)
    {
        ASSERT(static_cast<KeyType*>(toNative(wrapper)) == key);
        v8::Persistent<v8::Object> persistent(m_isolate, wrapper);
        configuration.configureWrapper(&persistent, m_isolate);
        persistent.MakeWeak(this, &makeWeakCallback);
        typename MapType::AddResult result = m_map.add(key, UnsafePersistent<v8::Object>());
        ASSERT(result.isNewEntry);
        // FIXME: Stop handling this case once duplicate wrappers are guaranteed not to be created.
        if (!result.isNewEntry)
            result.iterator->value.dispose();
        result.iterator->value = UnsafePersistent<v8::Object>(persistent);
    }

    void clear()
    {
        while (!m_map.isEmpty()) {
            // Swap out m_map on each iteration to ensure any wrappers added due to side effects of the loop are cleared.
            MapType map;
            map.swap(m_map);
            for (typename MapType::iterator it = map.begin(); it != map.end(); ++it) {
                toWrapperTypeInfo(it->value.deprecatedHandle())->derefObject(it->key);
                it->value.dispose();
            }
        }
    }

    void removeAndDispose(KeyType* key)
    {
        typename MapType::iterator it = m_map.find(key);
        ASSERT(it != m_map.end());
        it->value.dispose();
        m_map.remove(it);
    }

private:
    static void makeWeakCallback(v8::Isolate*, v8::Persistent<v8::Object>* wrapper, DOMWrapperMap<KeyType>*);

    v8::Isolate* m_isolate;
    MapType m_map;
};

template<>
inline void DOMWrapperMap<void>::makeWeakCallback(v8::Isolate* isolate, v8::Persistent<v8::Object>* wrapper, DOMWrapperMap<void>* map)
{
    WrapperTypeInfo* type = toWrapperTypeInfo(*wrapper);
    ASSERT(type->derefObjectFunction);
    void* key = static_cast<void*>(toNative(*wrapper));
    ASSERT(map->get(key) == *wrapper);
    map->removeAndDispose(key);
    type->derefObject(key);
}

} // namespace WebCore

#endif // DOMWrapperMap_h
