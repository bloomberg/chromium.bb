/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef ScriptWrappable_h
#define ScriptWrappable_h

#include "bindings/v8/UnsafePersistent.h"
#include "bindings/v8/WrapperTypeInfo.h"
#include "heap/Handle.h"
#include <v8.h>

// Helper to call webCoreInitializeScriptWrappableForInterface in the global namespace.
template <class C> inline void initializeScriptWrappableHelper(C* object)
{
    void webCoreInitializeScriptWrappableForInterface(C*);
    webCoreInitializeScriptWrappableForInterface(object);
}

namespace WebCore {

class ScriptWrappable {
public:
    ScriptWrappable() : m_wrapperOrTypeInfo(0) { }

    // Wrappables need to be initialized with their most derrived type for which
    // bindings exist, in much the same way that certain other types need to be
    // adopted and so forth. The overloaded initializeScriptWrappableForInterface()
    // functions are implemented by the generated V8 bindings code. Declaring the
    // extern function in the template avoids making a centralized header of all
    // the bindings in the universe. C++11's extern template feature may provide
    // a cleaner solution someday.
    template <class C> static void init(C* object)
    {
        initializeScriptWrappableHelper(object);
    }

    void setWrapper(v8::Handle<v8::Object> wrapper, v8::Isolate* isolate, const WrapperConfiguration& configuration)
    {
        ASSERT(!containsWrapper());
        if (!*wrapper) {
            m_wrapperOrTypeInfo = 0;
            return;
        }
        v8::Persistent<v8::Object> persistent(isolate, wrapper);
        configuration.configureWrapper(&persistent);
        persistent.SetWeak(this, &setWeakCallback);
        m_wrapperOrTypeInfo = reinterpret_cast<uintptr_t>(persistent.ClearAndLeak()) | 1;
        ASSERT(containsWrapper());
    }

    v8::Local<v8::Object> newLocalWrapper(v8::Isolate* isolate) const
    {
        return unsafePersistent().newLocal(isolate);
    }

    const WrapperTypeInfo* typeInfo()
    {
        if (containsTypeInfo())
            return reinterpret_cast<const WrapperTypeInfo*>(m_wrapperOrTypeInfo);

        if (containsWrapper())
            return toWrapperTypeInfo(*(unsafePersistent().persistent()));

        return 0;
    }

    void setTypeInfo(const WrapperTypeInfo* typeInfo)
    {
        m_wrapperOrTypeInfo = reinterpret_cast<uintptr_t>(typeInfo);
        ASSERT(containsTypeInfo());
    }

    static bool wrapperCanBeStoredInObject(const void*) { return false; }
    static bool wrapperCanBeStoredInObject(const ScriptWrappable*) { return true; }

    static void setWrapperInObject(void*, v8::Handle<v8::Object>, v8::Isolate*, const WrapperConfiguration&)
    {
        ASSERT_NOT_REACHED();
    }

    static void setWrapperInObject(ScriptWrappable* object, v8::Handle<v8::Object> wrapper, v8::Isolate* isolate, const WrapperConfiguration& configuration)
    {
        object->setWrapper(wrapper, isolate, configuration);
    }

    static const WrapperTypeInfo* getTypeInfoFromObject(void* object)
    {
        ASSERT_NOT_REACHED();
        return 0;
    }

    static const WrapperTypeInfo* getTypeInfoFromObject(ScriptWrappable* object)
    {
        return object->typeInfo();
    }

    static void setTypeInfoInObject(void* object, const WrapperTypeInfo*)
    {
        ASSERT_NOT_REACHED();
    }

    static void setTypeInfoInObject(ScriptWrappable* object, const WrapperTypeInfo* typeInfo)
    {
        object->setTypeInfo(typeInfo);
    }

    template<typename V8T, typename T>
    static bool setReturnValueWithSecurityCheck(v8::ReturnValue<v8::Value> returnValue, T* object)
    {
        return ScriptWrappable::getUnsafeWrapperFromObject(object).template setReturnValueWithSecurityCheck<V8T>(returnValue, object);
    }

    template<typename T>
    static bool setReturnValue(v8::ReturnValue<v8::Value> returnValue, T* object)
    {
        return ScriptWrappable::getUnsafeWrapperFromObject(object).setReturnValue(returnValue);
    }

protected:
    ~ScriptWrappable()
    {
        ASSERT(m_wrapperOrTypeInfo);  // Assert initialization via init() even if not subsequently wrapped.
        m_wrapperOrTypeInfo = 0;      // Break UAF attempts to wrap.
    }

private:
    // For calling unsafePersistent and getWrapperFromObject.
    friend class MinorGCWrapperVisitor;
    friend class DOMDataStore;

    UnsafePersistent<v8::Object> unsafePersistent() const
    {
        v8::Object* object = containsWrapper() ? reinterpret_cast<v8::Object*>(m_wrapperOrTypeInfo & ~1) : 0;
        return UnsafePersistent<v8::Object>(object);
    }

    static UnsafePersistent<v8::Object> getUnsafeWrapperFromObject(void*)
    {
        ASSERT_NOT_REACHED();
        return UnsafePersistent<v8::Object>();
    }

    static UnsafePersistent<v8::Object> getUnsafeWrapperFromObject(ScriptWrappable* object)
    {
        return object->unsafePersistent();
    }

    inline bool containsWrapper() const { return (m_wrapperOrTypeInfo & 1) == 1; }
    inline bool containsTypeInfo() const { return m_wrapperOrTypeInfo && (m_wrapperOrTypeInfo & 1) == 0; }

    inline void disposeWrapper(v8::Local<v8::Object> wrapper)
    {
        ASSERT(containsWrapper());
        ASSERT(wrapper == *unsafePersistent().persistent());
        unsafePersistent().dispose();
        setTypeInfo(toWrapperTypeInfo(wrapper));
    }

    // If zero, then this contains nothing, otherwise:
    //   If the bottom bit it set, then this contains a pointer to a wrapper object in the remainging bits.
    //   If the bottom bit is clear, then this contains a pointer to the wrapper type info in the remaining bits.
    uintptr_t m_wrapperOrTypeInfo;

    static void setWeakCallback(const v8::WeakCallbackData<v8::Object, ScriptWrappable>& data)
    {
        ASSERT(*data.GetParameter()->unsafePersistent().persistent() == data.GetValue());
        data.GetParameter()->disposeWrapper(data.GetValue());

        // FIXME: I noticed that 50%~ of minor GC cycle times can be consumed
        // inside data.GetParameter()->deref(), which causes Node destructions. We should
        // make Node destructions incremental.
        releaseObject(data.GetValue());
    }
};

} // namespace WebCore

#endif // ScriptWrappable_h
