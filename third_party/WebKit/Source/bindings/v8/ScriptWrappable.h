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
#include "bindings/v8/V8Utilities.h"
#include "bindings/v8/WrapperTypeInfo.h"
#include "core/dom/WebCoreMemoryInstrumentation.h"
#include <v8.h>

namespace WebCore {

class ScriptWrappable : public MemoryReporterTag {
    friend class WeakHandleListener<ScriptWrappable>;
public:
    ScriptWrappable() : m_maskedStorage(0) { }

    // Wrappables need to be initialized with their most derrived type for which
    // bindings exist, in much the same way that certain other types need to be
    // adopted and so forth. The overloaded initializeScriptWrappableForInterface()
    // functions are implemented by the generated V8 bindings code. Declaring the
    // extern function in the template avoids making a centralized header of all
    // the bindings in the universe. C++11's extern template feature may provide
    // a cleaner solution someday.
    template <class C> static void init(C* object)
    {
        void initializeScriptWrappableForInterface(C*);
        initializeScriptWrappableForInterface(object);
    }

    void setWrapper(v8::Handle<v8::Object> wrapper, v8::Isolate* isolate, const WrapperConfiguration& configuration)
    {
        ASSERT(!containsWrapper());
        if (!*wrapper) {
            m_maskedStorage = 0;
            return;
        }
        v8::Persistent<v8::Object> persistent(isolate, wrapper);
        configuration.configureWrapper(persistent, isolate);
        WeakHandleListener<ScriptWrappable>::makeWeak(isolate, persistent, this);
        m_maskedStorage = maskOrUnmaskValue(reinterpret_cast<uintptr_t>(*persistent));
        ASSERT(containsWrapper());
    }

    const WrapperTypeInfo* typeInfo()
    {
        if (containsTypeInfo())
            return reinterpret_cast<const WrapperTypeInfo*>(m_maskedStorage);

        if (containsWrapper()) {
            v8::Persistent<v8::Object> unsafeWrapper;
            unsafePersistent().copyTo(&unsafeWrapper);
            return toWrapperTypeInfo(unsafeWrapper);
        }

        return 0;
    }

    void setTypeInfo(const WrapperTypeInfo* info)
    {
        m_maskedStorage = reinterpret_cast<uintptr_t>(info);
        ASSERT(containsTypeInfo());
    }

    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::DOM);
        info.ignoreMember(m_maskedStorage);
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

    static void setTypeInfoInObject(void* object, const WrapperTypeInfo* info)
    {
        ASSERT_NOT_REACHED();
    }

    static void setTypeInfoInObject(ScriptWrappable* object, const WrapperTypeInfo* info)
    {
        object->setTypeInfo(info);
    }

protected:
    ~ScriptWrappable()
    {
        ASSERT(m_maskedStorage);  // Assert initialization via init() even if not subsequently wrapped.
        m_maskedStorage = 0;      // Break UAF attempts to wrap.
    }

private:
    // For calling unsafePersistent and getWrapperFromObject.
    friend class MinorGCWrapperVisitor;
    friend class DOMDataStore;

    UnsafePersistent<v8::Object> unsafePersistent() const
    {
        v8::Object* object = containsWrapper() ? reinterpret_cast<v8::Object*>(maskOrUnmaskValue(m_maskedStorage)) : 0;
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

    inline bool containsWrapper() const { return (m_maskedStorage & 1) == 1; }
    inline bool containsTypeInfo() const { return m_maskedStorage && ((m_maskedStorage & 1) == 0); }

    static inline uintptr_t maskOrUnmaskValue(uintptr_t value)
    {
        // Entropy via ASLR, bottom bit set to always toggle the bottom bit in the result. Since masking is only
        // applied to wrappers, not wrapper type infos, and these are aligned poitners with zeros in the bottom
        // bit(s), this automatically set the wrapper flag in the bottom bit upon encoding. Simiarlry,this
        // automatically zeros out the bit upon decoding. Additionally, since setWrapper() now performs an explicit
        // null test, and wrapper() requires the bottom bit to be set, there is no need to preserve null here.
        const uintptr_t randomMask = ~((reinterpret_cast<uintptr_t>(&WebCoreMemoryTypes::DOM) >> 13)) | 1;
        return value ^ randomMask;
    }

    inline void disposeWrapper(v8::Persistent<v8::Value> value, v8::Isolate* isolate, const WrapperTypeInfo* info)
    {
        ASSERT(containsWrapper());
        ASSERT(reinterpret_cast<uintptr_t>(*value) == maskOrUnmaskValue(m_maskedStorage));
        value.Dispose(isolate);
        setTypeInfo(info);
    }

    // If zero, then this contains nothing, otherwise:
    //   If the bottom bit it set, then this contains a masked pointer to a wrapper object in the remainging bits.
    //   If the bottom bit is clear, then this contains a pointer to the wrapper type info in the remaining bits.
    // Masking wrappers prevents attackers from overwriting this field with pointers to sprayed data.
    // Pointers to (and inside) WrapperTypeInfo are already protected by ASLR.
    uintptr_t m_maskedStorage;
};

template<>
inline void WeakHandleListener<ScriptWrappable>::callback(v8::Isolate* isolate, v8::Persistent<v8::Value> value, ScriptWrappable* key)
{
    ASSERT(value->IsObject());
    v8::Persistent<v8::Object> wrapper = v8::Persistent<v8::Object>::Cast(value);
    ASSERT(key->unsafePersistent().handle() == wrapper);

    // Note: |object| might not be equal to |key|, e.g., if ScriptWrappable isn't a left-most base class.
    void* object = toNative(wrapper);
    WrapperTypeInfo* info = toWrapperTypeInfo(wrapper);
    ASSERT(info->derefObjectFunction);

    key->disposeWrapper(value, isolate, info);
    // FIXME: I noticed that 50%~ of minor GC cycle times can be consumed
    // inside key->deref(), which causes Node destructions. We should
    // make Node destructions incremental.
    info->derefObject(object);
}

} // namespace WebCore

#endif // ScriptWrappable_h
