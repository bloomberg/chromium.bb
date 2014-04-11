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

    template <typename T>
    void resolve(T value)
    {
        if (m_state != Pending || !executionContext() || executionContext()->activeDOMObjectsAreStopped())
            return;
        m_state = Resolving;
        NewScriptState::Scope scope(m_scriptState.get());
        m_value.set(m_scriptState->isolate(), toV8(value));
        if (!executionContext()->activeDOMObjectsAreSuspended())
            resolveOrRejectImmediately(&m_timer);
    }

    template <typename T>
    void reject(T value)
    {
        if (m_state != Pending || !executionContext() || executionContext()->activeDOMObjectsAreStopped())
            return;
        m_state = Rejecting;
        NewScriptState::Scope scope(m_scriptState.get());
        m_value.set(m_scriptState->isolate(), toV8(value));
        if (!executionContext()->activeDOMObjectsAreSuspended())
            resolveOrRejectImmediately(&m_timer);
    }

    // Note that an empty ScriptPromise will be returned after resolve or
    // reject is called.
    ScriptPromise promise()
    {
        return m_resolver ? m_resolver->promise() : ScriptPromise();
    }

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

    template <typename T>
    v8::Handle<v8::Value> toV8(T* value)
    {
        ASSERT(m_scriptState);
        ASSERT(!m_scriptState->contextIsEmpty());
        return toV8NoInline(value, m_scriptState->context()->Global(), m_scriptState->isolate());
    }
    template <typename T> v8::Handle<v8::Value> toV8(PassRefPtr<T> value) { return toV8(value.get()); }
    template <typename T> v8::Handle<v8::Value> toV8(RawPtr<T> value) { return toV8(value.get()); }
    template <typename T, size_t inlineCapacity>
    v8::Handle<v8::Value> toV8(const Vector<T, inlineCapacity>& value)
    {
        ASSERT(m_scriptState);
        return v8ArrayNoInline(value, m_scriptState->isolate());
    }
    v8::Handle<v8::Value> toV8(ScriptValue value)
    {
        return value.v8Value();
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
