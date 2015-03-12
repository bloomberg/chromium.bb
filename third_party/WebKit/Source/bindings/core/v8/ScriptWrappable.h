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

#include "bindings/core/v8/WrapperTypeInfo.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"
#include <v8.h>

namespace blink {

/**
 * ScriptWrappable wraps a V8 object and its WrapperTypeInfo.
 *
 * ScriptWrappable acts much like a v8::Persistent<> in that it keeps a
 * V8 object alive.
 *
 *  The state transitions are:
 *  - new: an empty ScriptWrappable.
 *  - setWrapper: install a v8::Persistent (or empty)
 *  - disposeWrapper (via setWeakCallback, triggered by V8 garbage collecter):
 *        remove v8::Persistent and become empty.
 */
class CORE_EXPORT ScriptWrappable {
    WTF_MAKE_NONCOPYABLE(ScriptWrappable);
public:
    ScriptWrappable() { }

    template<typename T>
    T* toImpl()
    {
        // Check if T* is castable to ScriptWrappable*, which means T doesn't
        // have two or more ScriptWrappable as superclasses. If T has two
        // ScriptWrappable as superclasses, conversions from T* to
        // ScriptWrappable* are ambiguous.
        ASSERT(static_cast<ScriptWrappable*>(static_cast<T*>(this)));
        return static_cast<T*>(this);
    }

    // Returns the WrapperTypeInfo of the instance.
    //
    // This method must be overridden by DEFINE_WRAPPERTYPEINFO macro.
    virtual const WrapperTypeInfo* wrapperTypeInfo() const = 0;

    // Creates and returns a new wrapper object.
    virtual v8::Handle<v8::Object> wrap(v8::Handle<v8::Object> creationContext, v8::Isolate*);

    // Associates the instance with the existing wrapper. Returns |wrapper|.
    virtual v8::Handle<v8::Object> associateWithWrapper(v8::Isolate*, const WrapperTypeInfo*, v8::Handle<v8::Object> wrapper);

    void setWrapper(v8::Handle<v8::Object> wrapper, v8::Isolate* isolate, const WrapperTypeInfo* wrapperTypeInfo)
    {
        RELEASE_ASSERT(!containsWrapper());
        if (!*wrapper)
            return;
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(toScriptWrappable(wrapper) == this);
        m_wrapper.Reset(isolate, wrapper);
        wrapperTypeInfo->configureWrapper(&m_wrapper);
        m_wrapper.SetWeak(this, &setWeakCallback);
        ASSERT(containsWrapper());
    }

    v8::Local<v8::Object> newLocalWrapper(v8::Isolate* isolate) const
    {
        return v8::Local<v8::Object>::New(isolate, m_wrapper);
    }

    bool isEqualTo(const v8::Local<v8::Object>& other) const
    {
        return m_wrapper == other;
    }

    // Provides a way to convert Node* to ScriptWrappable* without including
    // "core/dom/Node.h".
    //
    // Example:
    //   void foo(const void*) { ... }       // [1]
    //   void foo(ScriptWrappable*) { ... }  // [2]
    //   class Node;
    //   Node* node;
    //   foo(node);  // This calls [1] because there is no definition of Node
    //               // and compilers do not know that Node is a subclass of
    //               // ScriptWrappable.
    //   foo(ScriptWrappable::fromNode(node));  // This calls [2] as expected.
    //
    // The definition of fromNode is placed in Node.h because we'd like to
    // inline calls to fromNode as much as possible.
    static ScriptWrappable* fromNode(Node*);

    bool setReturnValue(v8::ReturnValue<v8::Value> returnValue)
    {
        returnValue.Set(m_wrapper);
        return containsWrapper();
    }

    void markAsDependentGroup(ScriptWrappable* groupRoot, v8::Isolate* isolate)
    {
        ASSERT(containsWrapper());
        ASSERT(groupRoot && groupRoot->containsWrapper());

        // FIXME: There has to be a better way.
        v8::UniqueId groupId(*reinterpret_cast<intptr_t*>(&groupRoot->m_wrapper));
        m_wrapper.MarkPartiallyDependent();
        isolate->SetObjectGroupId(v8::Persistent<v8::Value>::Cast(m_wrapper), groupId);
    }

    void setReference(const v8::Persistent<v8::Object>& parent, v8::Isolate* isolate)
    {
        isolate->SetReference(parent, m_wrapper);
    }

    bool containsWrapper() const { return !m_wrapper.IsEmpty(); }

#if !ENABLE(OILPAN)
protected:
    virtual ~ScriptWrappable()
    {
        // We must not get deleted as long as we contain a wrapper. If this happens, we screwed up ref
        // counting somewhere. Crash here instead of crashing during a later gc cycle.
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!containsWrapper());
    }
#endif
    // With Oilpan we don't need a ScriptWrappable destructor.
    //
    // - 'RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!containsWrapper())' is not needed
    // because Oilpan is not using reference counting at all. If containsWrapper() is true,
    // it means that ScriptWrappable still has a wrapper. In this case, the destructor
    // must not be called since the wrapper has a persistent handle back to this ScriptWrappable object.
    // Assuming that Oilpan's GC is correct (If we cannot assume this, a lot of more things are
    // already broken), we must not hit the RELEASE_ASSERT.

private:
    void disposeWrapper(v8::Local<v8::Object> wrapper)
    {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(toScriptWrappable(wrapper) == this);
        RELEASE_ASSERT(containsWrapper());
        RELEASE_ASSERT(wrapper == m_wrapper);
        m_wrapper.Reset();
    }

    static void setWeakCallback(const v8::WeakCallbackData<v8::Object, ScriptWrappable>& data)
    {
        data.GetParameter()->disposeWrapper(data.GetValue());

        // FIXME: I noticed that 50%~ of minor GC cycle times can be consumed
        // inside data.GetParameter()->deref(), which causes Node destructions. We should
        // make Node destructions incremental.
        releaseObject(data.GetValue());
    }

    v8::Persistent<v8::Object> m_wrapper;
};

// Defines 'wrapperTypeInfo' virtual method which returns the WrapperTypeInfo of
// the instance. Also declares a static member of type WrapperTypeInfo, of which
// the definition is given by the IDL code generator.
//
// All the derived classes of ScriptWrappable, regardless of directly or
// indirectly, must write this macro in the class definition as long as the
// class has a corresponding .idl file.
#define DEFINE_WRAPPERTYPEINFO() \
public: \
    virtual const WrapperTypeInfo* wrapperTypeInfo() const override \
    { \
        return &s_wrapperTypeInfo; \
    } \
private: \
    static const WrapperTypeInfo& s_wrapperTypeInfo

// Defines 'wrapperTypeInfo' virtual method, which should never be called.
//
// This macro is used when there exists a class hierarchy with a root class
// and most of the subclasses are script-wrappable but not all of them.
// In that case, the root class can inherit from ScriptWrappable and use
// this macro, and let subclasses have a choice whether or not use
// DEFINE_WRAPPERTYPEINFO macro. The script-wrappable subclasses which have
// corresponding IDL file must call DEFINE_WRAPPERTYPEINFO, and the others
// must not.
#define DEFINE_WRAPPERTYPEINFO_NOT_REACHED() \
public: \
    virtual const WrapperTypeInfo* wrapperTypeInfo() const override \
    { \
        ASSERT_NOT_REACHED(); \
        return 0; \
    } \
private: \
    typedef void end_of_define_wrappertypeinfo_not_reached_t

} // namespace blink

#endif // ScriptWrappable_h
