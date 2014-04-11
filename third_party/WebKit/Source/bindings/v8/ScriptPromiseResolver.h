/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef ScriptPromiseResolver_h
#define ScriptPromiseResolver_h

#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/ScopedPersistent.h"
#include "bindings/v8/ScriptObject.h"
#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptState.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "wtf/RefPtr.h"

#include <v8.h>

namespace WebCore {

class ExecutionContext;

// ScriptPromiseResolver is a class for performing operations on Promise
// (resolve / reject) from C++ world.
// ScriptPromiseResolver holds a PromiseResolver.
// Here is a typical usage:
//  1. Create a ScriptPromiseResolver.
//  2. Pass the associated promise object to a JavaScript program
//     (such as XMLHttpRequest return value).
//  3. Call resolve or reject when the operation completes or
//     the operation fails respectively.
//
// Most methods including constructors must be called within a v8 context.
// To use ScriptPromiseResolver out of a v8 context the caller must
// enter a v8 context. ScriptPromiseResolverWithContext provides such
// functionality.
//
// To prevent memory leaks, you should release the reference manually
// by calling resolve or reject.
// Destroying the object will also release the reference.
// Note that ScriptPromiseResolver::promise returns an empty value when the
// resolver is already resolved or rejected. If you want to resolve a resolver
// immediately and return the associated promise, you should get the promise
// before resolving.
class ScriptPromiseResolver : public RefCounted<ScriptPromiseResolver> {
    WTF_MAKE_NONCOPYABLE(ScriptPromiseResolver);
public:
    static PassRefPtr<ScriptPromiseResolver> create(ExecutionContext*);
    static PassRefPtr<ScriptPromiseResolver> create(v8::Isolate*);

    // A ScriptPromiseResolver should be resolved / rejected before
    // its destruction.
    // A ScriptPromiseResolver can be destructed safely without
    // entering a v8 context.
    ~ScriptPromiseResolver();

    // Returns the underlying Promise.
    // Note that the underlying Promise is cleared when |resolve| or |reject|
    // is called.
    ScriptPromise promise();

    // To use following template methods, T must be a DOM class.
    template<typename T>
    void resolve(T* value, v8::Handle<v8::Object> creationContext) { resolve(toV8NoInline(value, creationContext, m_isolate)); }
    template<typename T>
    void reject(T* value, v8::Handle<v8::Object> creationContext) { reject(toV8NoInline(value, creationContext, m_isolate)); }

    template<typename T>
    void resolve(PassRefPtr<T> value, v8::Handle<v8::Object> creationContext) { resolve(value.get(), creationContext); }
    template<typename T>
    void resolve(RawPtr<T> value, v8::Handle<v8::Object> creationContext) { resolve(value.get(), creationContext); }
    template<typename T>
    void reject(PassRefPtr<T> value, v8::Handle<v8::Object> creationContext) { reject(value.get(), creationContext); }
    template<typename T>
    void reject(RawPtr<T> value, v8::Handle<v8::Object> creationContext) { reject(value.get(), creationContext); }

    template<typename T>
    inline void resolve(T* value, ExecutionContext*);
    template<typename T>
    inline void reject(T* value, ExecutionContext*);

    template<typename T>
    void resolve(PassRefPtr<T> value, ExecutionContext* context) { resolve(value.get(), context); }
    template<typename T>
    void resolve(RawPtr<T> value, ExecutionContext* context) { resolve(value.get(), context); }
    template<typename T>
    void reject(PassRefPtr<T> value, ExecutionContext* context) { reject(value.get(), context); }
    template<typename T>
    void reject(RawPtr<T> value, ExecutionContext* context) { reject(value.get(), context); }

    template<typename T>
    inline void resolve(T* value);
    template<typename T>
    inline void reject(T* value);

    template<typename T, size_t inlineCapacity>
    void resolve(const Vector<T, inlineCapacity>& iterator) { resolve(v8ArrayNoInline(iterator, m_isolate)); }
    template<typename T, size_t inlineCapacity>
    void reject(const Vector<T, inlineCapacity>& iterator) { reject(v8ArrayNoInline(iterator, m_isolate)); }

    template<typename T>
    void resolve(PassRefPtr<T> value) { resolve(value.get()); }
    template<typename T>
    void resolve(RawPtr<T> value) { resolve(value.get()); }
    template<typename T>
    void reject(PassRefPtr<T> value) { reject(value.get()); }
    template<typename T>
    void reject(RawPtr<T> value) { reject(value.get()); }

    void resolve(ScriptValue);
    void reject(ScriptValue);

    v8::Isolate* isolate() const { return m_isolate; }

    void resolve(v8::Handle<v8::Value>);
    void reject(v8::Handle<v8::Value>);

private:
    ScriptPromiseResolver(ExecutionContext*);
    ScriptPromiseResolver(v8::Isolate*);

    v8::Isolate* m_isolate;
    // Used when scriptPromiseOnV8Promise is disabled.
    ScriptPromise m_promise;
    // Used when scriptPromiseOnV8Promise is enabled.
    ScriptValue m_resolver;
};

template<typename T>
void ScriptPromiseResolver::resolve(T* value, ExecutionContext* context)
{
    ASSERT(m_isolate->InContext());
    v8::Handle<v8::Context> v8Context = toV8Context(context, DOMWrapperWorld::current(m_isolate));
    resolve(value, v8Context->Global());
}

template<typename T>
void ScriptPromiseResolver::reject(T* value, ExecutionContext* context)
{
    ASSERT(m_isolate->InContext());
    v8::Handle<v8::Context> v8Context = toV8Context(context, DOMWrapperWorld::current(m_isolate));
    reject(value, v8Context->Global());
}

template<typename T>
void ScriptPromiseResolver::resolve(T* value)
{
    ASSERT(m_isolate->InContext());
    resolve(value, v8::Object::New(m_isolate));
}

template<typename T>
void ScriptPromiseResolver::reject(T* value)
{
    ASSERT(m_isolate->InContext());
    reject(value, v8::Object::New(m_isolate));
}

} // namespace WebCore


#endif // ScriptPromiseResolver_h
