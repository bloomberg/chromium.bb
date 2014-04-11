// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/v8/ScriptPromiseResolverWithContext.h"

namespace WebCore {

ScriptPromiseResolverWithContext::ScriptPromiseResolverWithContext(NewScriptState* scriptState)
    : ActiveDOMObject(scriptState->executionContext())
    , m_state(Pending)
    , m_scriptState(scriptState)
    , m_timer(this, &ScriptPromiseResolverWithContext::resolveOrRejectImmediately)
    , m_resolver(ScriptPromiseResolver::create(m_scriptState->executionContext())) { }

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

void ScriptPromiseResolverWithContext::resolveOrRejectImmediately(Timer<ScriptPromiseResolverWithContext>*)
{
    ASSERT(!executionContext()->activeDOMObjectsAreStopped());
    ASSERT(!executionContext()->activeDOMObjectsAreSuspended());
    if (m_state == Resolving) {
        NewScriptState::Scope scope(m_scriptState.get());
        m_resolver->resolve(m_value.newLocal(m_scriptState->isolate()));
    } else {
        ASSERT(m_state == Rejecting);
        NewScriptState::Scope scope(m_scriptState.get());
        m_resolver->reject(m_value.newLocal(m_scriptState->isolate()));
    }
    m_state = ResolvedOrRejected;
    clear();
}

void ScriptPromiseResolverWithContext::clear()
{
    m_resolver.clear();
    m_value.clear();
    m_scriptState.clear();
}

} // namespace WebCore
