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

#include "bindings/v8/ScopedPersistent.h"
#include "bindings/v8/ScriptObject.h"
#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptState.h"
#include "bindings/v8/ScriptValue.h"
#include "wtf/RefPtr.h"

#include <v8.h>

namespace WebCore {

class ScriptExecutionContext;

// ScriptPromiseResolver is a class for accessing PromiseResolver methods
// (fulfill / resolve / reject) from C++ world.
// ScriptPromiseResolver holds a Promise and a PromiseResolver.
// All methods of this class must be called from the main thread.
// Here is a typical usage:
//  1. Create a ScriptPromiseResolver.
//  2. Pass the promise object of the holder to a JavaScript program
//     (such as XMLHttpRequest return value).
//  3. Detach the promise object if you no longer need it.
//  4. Call fulfill or reject when the operation completes or
//     the operation fails respectively.
//
// Most methods including constructors must be called within a v8 context.
// To use ScriptPromiseResolver out of a v8 context the caller must
// enter a v8 context, for example by using ScriptScope and ScriptState.
//
// If you hold ScriptPromiseResolver as a member variable in a DOM object,
// it causes memory leaks unless you detach the promise and resolver object
// manually.
// So if you no longer need the promise object, you should call detachPromise.
// And if the operation completes or fails, you should call fulfill / resolve /
// reject. Destroying ScriptPromiseResolver will also detach the promise and
// resolver object.
//
class ScriptPromiseResolver : public RefCounted<ScriptPromiseResolver> {
    WTF_MAKE_NONCOPYABLE(ScriptPromiseResolver);
public:
    static PassRefPtr<ScriptPromiseResolver> create(ScriptExecutionContext*);
    static PassRefPtr<ScriptPromiseResolver> create();

    // A ScriptPromiseResolver should be fulfilled / resolved / rejected before
    // its destruction.
    // A ScriptPromiseResolver can be destructed safely without
    // entering a v8 context.
    ~ScriptPromiseResolver();

    // Detach the promise object and reject the resolver object with undefined.
    void detach();

    // Detach the promise object.
    void detachPromise() { m_promise.clear(); }

    // Return true if the following conditions are met:
    //  - The resolver object is not detached.
    //  - The resolver's promise object is in pending state.
    //  - The resolver's resolved flag is not set.
    bool isPending() const;

    ScriptPromise promise()
    {
        ASSERT(v8::Context::InContext());
        return m_promise;
    }

    // Fulfill with a C++ object which can be converted to a v8 object by toV8.
    template<typename T>
    inline void fulfill(PassRefPtr<T>);
    // Resolve with a C++ object which can be converted to a v8 object by toV8.
    template<typename T>
    inline void resolve(PassRefPtr<T>);
    // Reject with a C++ object which can be converted to a v8 object by toV8.
    template<typename T>
    inline void reject(PassRefPtr<T>);

    void fulfill(ScriptValue);
    void resolve(ScriptValue);
    void reject(ScriptValue);

private:
    ScriptPromiseResolver(v8::Handle<v8::Object> creationContext, v8::Isolate*);
    void fulfill(v8::Handle<v8::Value>);
    void resolve(v8::Handle<v8::Value>);
    void reject(v8::Handle<v8::Value>);

    v8::Isolate* m_isolate;
    ScriptPromise m_promise;
    ScopedPersistent<v8::Object> m_resolver;
    bool isPendingInternal() const;
};

template<typename T>
void ScriptPromiseResolver::fulfill(PassRefPtr<T> value)
{
    ASSERT(v8::Context::InContext());
    fulfill(toV8(value.get(), v8::Object::New(), m_isolate));
}

template<typename T>
void ScriptPromiseResolver::resolve(PassRefPtr<T> value)
{
    ASSERT(v8::Context::InContext());
    resolve(toV8(value.get(), v8::Object::New(), m_isolate));
}

template<typename T>
void ScriptPromiseResolver::reject(PassRefPtr<T> value)
{
    ASSERT(v8::Context::InContext());
    reject(toV8(value.get(), v8::Object::New(), m_isolate));
}

} // namespace WebCore


#endif // ScriptPromiseResolver_h
