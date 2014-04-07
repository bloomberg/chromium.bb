/*
 * Copyright (C) 2008, 2009, 2011 Google Inc. All rights reserved.
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

#ifndef ScriptState_h
#define ScriptState_h

#include "bindings/v8/ScopedPersistent.h"
#include <v8.h>
#include "wtf/Noncopyable.h"

namespace WebCore {

class DOMWindow;
class DOMWrapperWorld;
class LocalFrame;
class ExecutionContext;
class WorkerGlobalScope;

// FIXME: ScriptState is deprecated and going to be removed. Use NewScriptState instead.
class ScriptState {
    WTF_MAKE_NONCOPYABLE(ScriptState);
public:
    v8::Local<v8::Context> context() const
    {
        return m_context.newLocal(m_isolate);
    }

    v8::Isolate* isolate()
    {
        return m_isolate;
    }

    DOMWindow* domWindow() const;
    ExecutionContext* executionContext() const;
    bool evalEnabled() const;
    void setEvalEnabled(bool);

    static ScriptState* forContext(v8::Handle<v8::Context>);
    static ScriptState* current();

protected:
    ScriptState()
        : m_isolate(v8::Isolate::GetCurrent())
    {
    }

    ~ScriptState();

private:
    friend ScriptState* mainWorldScriptState(LocalFrame*);
    explicit ScriptState(v8::Handle<v8::Context>);

    static void setWeakCallback(const v8::WeakCallbackData<v8::Context, ScriptState>&);

    ScopedPersistent<v8::Value> m_exception;
    ScopedPersistent<v8::Context> m_context;
    v8::Isolate* m_isolate;
};

class ScriptStateProtectedPtr {
    WTF_MAKE_NONCOPYABLE(ScriptStateProtectedPtr);
public:
    ScriptStateProtectedPtr()
        : m_scriptState(0)
    {
    }

    ScriptStateProtectedPtr(ScriptState* scriptState)
        : m_scriptState(scriptState)
    {
        if (!scriptState)
            return;

        v8::HandleScope handleScope(scriptState->isolate());
        // Keep the context from being GC'ed. ScriptState is guaranteed to be live while the context is live.
        m_context.set(scriptState->isolate(), scriptState->context());
    }

    ScriptState* get() const { return m_scriptState; }

    void clear()
    {
        if (!m_scriptState)
            return;

        m_context.clear();
        m_scriptState = 0;
    }

private:
    ScriptState* m_scriptState;
    ScopedPersistent<v8::Context> m_context;
};

ScriptState* mainWorldScriptState(LocalFrame*);

ScriptState* scriptStateFromWorkerGlobalScope(WorkerGlobalScope*);

}

#endif // ScriptState_h
