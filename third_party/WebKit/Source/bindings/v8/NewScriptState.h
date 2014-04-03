// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NewScriptState_h
#define NewScriptState_h

#include "bindings/v8/ScopedPersistent.h"
#include "bindings/v8/V8PerContextData.h"
#include "wtf/RefCounted.h"
#include <v8.h>

namespace WebCore {

class DOMWrapperWorld;
class ExecutionContext;

// NewScriptState is created when v8::Context is created.
// NewScriptState is destroyed when v8::Context is garbage-collected and
// all V8 proxy objects that have references to the NewScriptState are destructed.
class NewScriptState : public RefCounted<NewScriptState> {
    WTF_MAKE_NONCOPYABLE(NewScriptState);
public:
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

    v8::Isolate* isolate() const { return m_isolate; }
    DOMWrapperWorld& world() const { return *m_world; }
    // This can return an empty handle if the v8::Context is gone.
    v8::Handle<v8::Context> context() const { return m_context.newLocal(m_isolate); }
    void clearContext() { return m_context.clear(); }
    ExecutionContext* executionContext() const;
    V8PerContextData* perContextData() const { return m_perContextData.get(); }
    void disposePerContextData() { m_perContextData = nullptr; }

private:
    NewScriptState(v8::Handle<v8::Context>, PassRefPtr<DOMWrapperWorld>);

    v8::Isolate* m_isolate;
    ScopedPersistent<v8::Context> m_context;
    // This RefPtr doesn't cause a cycle because all persistent handles that DOMWrapperWorld holds are weak.
    RefPtr<DOMWrapperWorld> m_world;
    OwnPtr<V8PerContextData> m_perContextData;
};

}

#endif // NewScriptState_h
