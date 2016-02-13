// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/IgnoreExceptionsScope.h"

#include "platform/v8_inspector/V8DebuggerImpl.h"

namespace blink {

class IgnoreExceptionsScopeImpl {
public:
    explicit IgnoreExceptionsScopeImpl(V8DebuggerImpl* debugger)
        : m_debugger(debugger)
        , m_previousPauseOnExceptionsState(V8DebuggerImpl::DontPauseOnExceptions)
    {
        m_previousPauseOnExceptionsState = setPauseOnExceptionsState(V8DebuggerImpl::DontPauseOnExceptions);
    }
    ~IgnoreExceptionsScopeImpl()
    {
        setPauseOnExceptionsState(m_previousPauseOnExceptionsState);
    }

private:
    V8DebuggerImpl::PauseOnExceptionsState setPauseOnExceptionsState(V8DebuggerImpl::PauseOnExceptionsState newState)
    {
        ASSERT(m_debugger);
        if (!m_debugger->enabled())
            return newState;
        V8DebuggerImpl::PauseOnExceptionsState presentState = m_debugger->pauseOnExceptionsState();
        if (presentState != newState)
            m_debugger->setPauseOnExceptionsState(newState);
        return presentState;
    }

    V8DebuggerImpl* m_debugger;
    V8DebuggerImpl::PauseOnExceptionsState m_previousPauseOnExceptionsState;
};

IgnoreExceptionsScope::IgnoreExceptionsScope(V8Debugger* debugger)
    : m_impl(adoptPtr(new IgnoreExceptionsScopeImpl(static_cast<V8DebuggerImpl*>(debugger))))
{
}

IgnoreExceptionsScope::~IgnoreExceptionsScope()
{
}

} // namespace blink
