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

#ifndef DOMDataStore_h
#define DOMDataStore_h

#include "bindings/v8/DOMWrapperMap.h"
#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/ScriptWrappable.h"
#include "bindings/v8/WrapperTypeInfo.h"
#include "core/dom/Node.h"
#include <v8.h>
#include "wtf/HashMap.h"
#include "wtf/MainThread.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/StdLibExtras.h"
#include "wtf/ThreadSpecific.h"
#include "wtf/Threading.h"
#include "wtf/Vector.h"

namespace WebCore {

class DOMDataStore {
    WTF_MAKE_NONCOPYABLE(DOMDataStore);
public:
    explicit DOMDataStore(WrapperWorldType);
    ~DOMDataStore();

    static DOMDataStore* current(v8::Isolate*);

    template<typename T, typename HolderContainer, typename Wrappable>
    static v8::Handle<v8::Object> getWrapperFast(T* object, const HolderContainer& container, Wrappable* holder)
    {
        // What we'd really like to check here is whether we're in the
        // main world or in an isolated world. The fastest way to do that
        // is to check that there is no isolated world and the 'object'
        // is an object that can exist in the main world. The second fastest
        // way is to check whether the wrappable's wrapper is the same as
        // the holder.
        if ((!DOMWrapperWorld::isolatedWorldsExist() && !canExistInWorker(object)) || holderContainsWrapper(container, holder)) {
            if (ScriptWrappable::wrapperCanBeStoredInObject(object))
                return ScriptWrappable::getUnsafeWrapperFromObject(object).handle();
            return mainWorldStore()->m_wrapperMap.get(object);
        }
        return current(container.GetIsolate())->get(object);
    }

    template<typename T>
    static v8::Handle<v8::Object> getWrapper(T* object, v8::Isolate* isolate)
    {
        if (ScriptWrappable::wrapperCanBeStoredInObject(object) && !canExistInWorker(object)) {
            if (LIKELY(!DOMWrapperWorld::isolatedWorldsExist()))
                return ScriptWrappable::getUnsafeWrapperFromObject(object).handle();
        }
        return current(isolate)->get(object);
    }

    template<typename T>
    static v8::Handle<v8::Object> getWrapperForMainWorld(T* object)
    {
        if (ScriptWrappable::wrapperCanBeStoredInObject(object))
            return ScriptWrappable::getUnsafeWrapperFromObject(object).handle();
        return mainWorldStore()->get(object);
    }

    template<typename T>
    static void setWrapper(T* object, v8::Handle<v8::Object> wrapper, v8::Isolate* isolate, const WrapperConfiguration& configuration)
    {
        if (ScriptWrappable::wrapperCanBeStoredInObject(object) && !canExistInWorker(object)) {
            if (LIKELY(!DOMWrapperWorld::isolatedWorldsExist())) {
                ScriptWrappable::setWrapperInObject(object, wrapper, isolate, configuration);
                return;
            }
        }
        return current(isolate)->set(object, wrapper, isolate, configuration);
    }

    template<typename T>
    inline v8::Handle<v8::Object> get(T* object)
    {
        if (ScriptWrappable::wrapperCanBeStoredInObject(object) && m_type == MainWorld)
            return ScriptWrappable::getUnsafeWrapperFromObject(object).handle();
        return m_wrapperMap.get(object);
    }

    void reportMemoryUsage(MemoryObjectInfo*) const;

private:
    template<typename T>
    inline void set(T* object, v8::Handle<v8::Object> wrapper, v8::Isolate* isolate, const WrapperConfiguration& configuration)
    {
        ASSERT(!!object);
        ASSERT(!wrapper.IsEmpty());
        if (ScriptWrappable::wrapperCanBeStoredInObject(object) && m_type == MainWorld) {
            ScriptWrappable::setWrapperInObject(object, wrapper, isolate, configuration);
            return;
        }
        m_wrapperMap.set(object, wrapper, configuration);
    }

    static DOMDataStore* mainWorldStore();

    static bool canExistInWorker(void*) { return true; }
    static bool canExistInWorker(Node*) { return false; }

    template<typename HolderContainer>
    static bool holderContainsWrapper(const HolderContainer&, void*)
    {
        return false;
    }

    template<typename HolderContainer>
    static bool holderContainsWrapper(const HolderContainer& container, ScriptWrappable* wrappable)
    {
        // Verify our assumptions about the main world.
        ASSERT(wrappable->unsafePersistent().handle().IsEmpty() || container.Holder() != wrappable->unsafePersistent().handle() || current(v8::Isolate::GetCurrent())->m_type == MainWorld);
        return container.Holder() == wrappable->unsafePersistent().handle();
    }

    WrapperWorldType m_type;
    DOMWrapperMap<void> m_wrapperMap;
};

template<>
inline void WeakHandleListener<DOMWrapperMap<void> >::callback(v8::Isolate* isolate, v8::Persistent<v8::Value> value, DOMWrapperMap<void>* map)
{
    ASSERT(value->IsObject());
    v8::Persistent<v8::Object> wrapper = v8::Persistent<v8::Object>::Cast(value);
    WrapperTypeInfo* type = toWrapperTypeInfo(wrapper);
    ASSERT(type->derefObjectFunction);
    void* key = static_cast<void*>(toNative(wrapper));
    map->removeAndDispose(key, wrapper, isolate);
    type->derefObject(key);
}

} // namespace WebCore

#endif // DOMDataStore_h
