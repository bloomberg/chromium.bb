// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NewScriptState_h
#define NewScriptState_h

#include "bindings/v8/ScopedPersistent.h"
#include "bindings/v8/ScriptState.h"
#include "bindings/v8/V8PerContextData.h"
#include "wtf/RefCounted.h"
#include <v8.h>

namespace WebCore {

class DOMWindow;
class DOMWrapperWorld;
class ExecutionContext;
class LocalFrame;

// NewScriptState is created when v8::Context is created.
// NewScriptState is destroyed when v8::Context is garbage-collected and
// all V8 proxy objects that have references to the NewScriptState are destructed.
class NewScriptState : public RefCounted<NewScriptState> {
    WTF_MAKE_NONCOPYABLE(NewScriptState);
public:
    class Scope {
    public:
        // You need to make sure that scriptState->context() is not empty before creating a Scope.
        explicit Scope(NewScriptState* scriptState)
            : m_handleScope(scriptState->isolate())
            , m_context(scriptState->context())
        {
            ASSERT(!m_context.IsEmpty());
            m_context->Enter();
        }

        ~Scope()
        {
            m_context->Exit();
        }

    private:
        v8::HandleScope m_handleScope;
        v8::Handle<v8::Context> m_context;
    };

    static PassRefPtr<NewScriptState> create(v8::Handle<v8::Context>, PassRefPtr<DOMWrapperWorld>);
    ~NewScriptState();

    static NewScriptState* current(v8::Isolate* isolate)
    {
        return from(isolate->GetCurrentContext());
    }

    static NewScriptState* from(v8::Handle<v8::Context> context)
    {
        ASSERT(!context.IsEmpty());
        NewScriptState* scriptState = static_cast<NewScriptState*>(context->GetAlignedPointerFromEmbedderData(v8ContextPerContextDataIndex));
        // NewScriptState::from() must not be called for a context that does not have
        // valid embedder data in the embedder field.
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(scriptState);
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(scriptState->context() == context);
        return scriptState;
    }

    static NewScriptState* forMainWorld(LocalFrame*);

    v8::Isolate* isolate() const { return m_isolate; }
    DOMWrapperWorld& world() const { return *m_world; }
    ExecutionContext* executionContext() const;
    DOMWindow* domWindow() const;

    // This can return an empty handle if the v8::Context is gone.
    v8::Handle<v8::Context> context() const { return m_context.newLocal(m_isolate); }
    bool contextIsEmpty() const { return m_context.isEmpty(); }
    void clearContext() { return m_context.clear(); }

    V8PerContextData* perContextData() const { return m_perContextData.get(); }
    void disposePerContextData() { m_perContextData = nullptr; }

    bool evalEnabled() const;
    void setEvalEnabled(bool);

    // FIXME: Once we replace all ScriptStates with NewScriptStates, remove this method.
    ScriptState* oldScriptState();

private:
    NewScriptState(v8::Handle<v8::Context>, PassRefPtr<DOMWrapperWorld>);

    v8::Isolate* m_isolate;
    // This persistent handle is weak.
    ScopedPersistent<v8::Context> m_context;

    // This RefPtr doesn't cause a cycle because all persistent handles that DOMWrapperWorld holds are weak.
    RefPtr<DOMWrapperWorld> m_world;

    // This OwnPtr causes a cycle:
    // V8PerContextData --(Persistent)--> v8::Context --(RefPtr)--> NewScriptState --(OwnPtr)--> V8PerContextData
    // So you must explicitly clear the OwnPtr by calling disposePerContextData()
    // once you no longer need V8PerContextData. Otherwise, the v8::Context will leak.
    OwnPtr<V8PerContextData> m_perContextData;
};

// NewScriptStateProtectingContext keeps the context associated with the NewScriptState alive.
// You need to call clear() once you no longer need the context. Otherwise, the context will leak.
class NewScriptStateProtectingContext {
    WTF_MAKE_NONCOPYABLE(NewScriptStateProtectingContext);
public:
    NewScriptStateProtectingContext(NewScriptState* scriptState)
        : m_scriptState(scriptState)
    {
        if (m_scriptState)
            m_context.set(m_scriptState->isolate(), m_scriptState->context());
    }

    NewScriptState* get() const { return m_scriptState.get(); }
    void clear()
    {
        m_scriptState = nullptr;
        m_context.clear();
    }

private:
    RefPtr<NewScriptState> m_scriptState;
    ScopedPersistent<v8::Context> m_context;
};

}

#endif // NewScriptState_h
