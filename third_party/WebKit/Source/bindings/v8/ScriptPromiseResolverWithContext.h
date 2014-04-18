// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptPromiseResolverWithContext_h
#define ScriptPromiseResolverWithContext_h

#include "bindings/v8/NewScriptState.h"
#include "bindings/v8/ScopedPersistent.h"
#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/ExecutionContext.h"
#include "platform/Timer.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include <v8.h>

namespace WebCore {

// This class wraps ScriptPromiseResolver and provides the following
// functionalities in addition to ScriptPromiseResolver's.
//  - A ScriptPromiseResolverWithContext retains a ScriptState. A caller
//    can call resolve or reject from outside of a V8 context.
//  - This class is an ActiveDOMObject and keeps track of the associated
//    ExecutionContext state. When the ExecutionContext is suspended,
//    resolve or reject will be delayed. When it is stopped, resolve or reject
//    will be ignored.
class ScriptPromiseResolverWithContext FINAL : public ActiveDOMObject, public RefCounted<ScriptPromiseResolverWithContext> {
    WTF_MAKE_NONCOPYABLE(ScriptPromiseResolverWithContext);

public:
    static PassRefPtr<ScriptPromiseResolverWithContext> create(NewScriptState* scriptState)
    {
        RefPtr<ScriptPromiseResolverWithContext> resolver = adoptRef(new ScriptPromiseResolverWithContext(scriptState));
        resolver->suspendIfNeeded();
        return resolver.release();
    }

    // Anything that can be passed to toV8Value can be passed to this function.
    template <typename T>
    void resolve(T value)
    {
        if (m_state != Pending || !executionContext() || executionContext()->activeDOMObjectsAreStopped())
            return;
        m_state = Resolving;
        NewScriptState::Scope scope(m_scriptState.get());
        m_value.set(m_scriptState->isolate(), toV8Value(value));
        if (!executionContext()->activeDOMObjectsAreSuspended())
            resolveOrRejectImmediately(&m_timer);
    }

    // Anything that can be passed to toV8Value can be passed to this function.
    template <typename T>
    void reject(T value)
    {
        if (m_state != Pending || !executionContext() || executionContext()->activeDOMObjectsAreStopped())
            return;
        m_state = Rejecting;
        NewScriptState::Scope scope(m_scriptState.get());
        m_value.set(m_scriptState->isolate(), toV8Value(value));
        if (!executionContext()->activeDOMObjectsAreSuspended())
            resolveOrRejectImmediately(&m_timer);
    }

    // Note that an empty ScriptPromise will be returned after resolve or
    // reject is called.
    ScriptPromise promise()
    {
        return m_resolver ? m_resolver->promise() : ScriptPromise();
    }

    NewScriptState* scriptState() const { return m_scriptState.get(); }

    // ActiveDOMObject implementation.
    virtual void suspend() OVERRIDE;
    virtual void resume() OVERRIDE;
    virtual void stop() OVERRIDE;

private:
    enum ResolutionState {
        Pending,
        Resolving,
        Rejecting,
        ResolvedOrRejected,
    };

    explicit ScriptPromiseResolverWithContext(NewScriptState*);

    template<typename T>
    v8::Handle<v8::Value> toV8Value(const T& value)
    {
        // Default implementaion: for types that don't need the
        // creation context.
        return V8ValueTraits<T>::toV8Value(value, m_scriptState->isolate());
    }

    // Pointer specializations.
    template <typename T>
    v8::Handle<v8::Value> toV8Value(T* value)
    {
        ASSERT(!m_scriptState->contextIsEmpty());
        return toV8NoInline(value, m_scriptState->context()->Global(), m_scriptState->isolate());
    }
    template<typename T> v8::Handle<v8::Value> toV8Value(RefPtr<T> value) { return toV8Value(value.get()); }
    template<typename T> v8::Handle<v8::Value> toV8Value(PassRefPtr<T> value) { return toV8Value(value.get()); }
    template<typename T> v8::Handle<v8::Value> toV8Value(OwnPtr<T> value) { return toV8Value(value.get()); }
    template<typename T> v8::Handle<v8::Value> toV8Value(PassOwnPtr<T> value) { return toV8Value(value.get()); }
    template<typename T> v8::Handle<v8::Value> toV8Value(RawPtr<T> value) { return toV8Value(value.get()); }

    // const char* should use V8ValueTraits.
    v8::Handle<v8::Value> toV8Value(const char* value) { return V8ValueTraits<const char*>::toV8Value(value, m_scriptState->isolate()); }

    template<typename T, size_t inlineCapacity>
    v8::Handle<v8::Value> toV8Value(const Vector<T, inlineCapacity>& value)
    {
        return v8ArrayNoInline(value, m_scriptState->isolate());
    }

    void resolveOrRejectImmediately(Timer<ScriptPromiseResolverWithContext>*);
    void clear();

    ResolutionState m_state;
    RefPtr<NewScriptState> m_scriptState;
    Timer<ScriptPromiseResolverWithContext> m_timer;
    RefPtr<ScriptPromiseResolver> m_resolver;
    ScopedPersistent<v8::Value> m_value;
};

} // namespace WebCore

#endif // #ifndef ScriptPromiseResolverWithContext_h
