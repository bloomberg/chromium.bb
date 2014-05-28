// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/v8/ScriptPromiseResolverWithContext.h"

#include "bindings/v8/V8RecursionScope.h"

namespace WebCore {

ScriptPromiseResolverWithContext::ScriptPromiseResolverWithContext(ScriptState* scriptState)
    : ActiveDOMObject(scriptState->executionContext())
    , m_state(Pending)
    , m_scriptState(scriptState)
    , m_timer(this, &ScriptPromiseResolverWithContext::onTimerFired)
    , m_resolver(ScriptPromiseResolver::create(m_scriptState.get()))
{
}

void ScriptPromiseResolverWithContext::suspend()
{
    m_timer.stop();
}

void ScriptPromiseResolverWithContext::resume()
{
    if (m_state == Resolving || m_state == Rejecting)
        m_timer.startOneShot(0, FROM_HERE);
}

void ScriptPromiseResolverWithContext::stop()
{
    m_timer.stop();
    clear();
}

void ScriptPromiseResolverWithContext::onTimerFired(Timer<ScriptPromiseResolverWithContext>*)
{
    RefPtr<ScriptPromiseResolverWithContext> protect(this);
    ScriptState::Scope scope(m_scriptState.get());
    resolveOrRejectImmediately();
}

void ScriptPromiseResolverWithContext::resolveOrRejectImmediately()
{
    ASSERT(!executionContext()->activeDOMObjectsAreStopped());
    ASSERT(!executionContext()->activeDOMObjectsAreSuspended());
    {
        // FIXME: The V8RecursionScope is only necessary to force microtask delivery for promises
        // resolved or rejected in workers. It can be removed once worker threads run microtasks
        // at the end of every task (rather than just the main thread).
        V8RecursionScope scope(m_scriptState->isolate(), m_scriptState->executionContext());
        if (m_state == Resolving) {
            m_resolver->resolve(m_value.newLocal(m_scriptState->isolate()));
        } else {
            ASSERT(m_state == Rejecting);
            m_resolver->reject(m_value.newLocal(m_scriptState->isolate()));
        }
    }
    clear();
}

void ScriptPromiseResolverWithContext::clear()
{
    ResolutionState state = m_state;
    m_state = ResolvedOrRejected;
    m_resolver.clear();
    m_value.clear();
    if (state == Resolving || state == Rejecting) {
        // |ref| was called in |resolveOrReject|.
        deref();
    }
    // |this| may be deleted here.
}

} // namespace WebCore
