// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IgnoreExceptionsScope_h
#define IgnoreExceptionsScope_h

#include "platform/inspector_protocol/Allocator.h"
#include "platform/v8_inspector/V8DebuggerImpl.h"
#include "wtf/OwnPtr.h"

namespace blink {

class IgnoreExceptionsScope {
    PROTOCOL_DISALLOW_COPY(IgnoreExceptionsScope);
public:
    explicit IgnoreExceptionsScope(V8DebuggerImpl* debugger)
        : m_debugger(debugger)
        , m_previousPauseOnExceptionsState(V8DebuggerImpl::DontPauseOnExceptions)
    {
        m_previousPauseOnExceptionsState = setPauseOnExceptionsState(V8DebuggerImpl::DontPauseOnExceptions);
    }

    ~IgnoreExceptionsScope()
    {
        setPauseOnExceptionsState(m_previousPauseOnExceptionsState);
    }

private:
    V8DebuggerImpl::PauseOnExceptionsState setPauseOnExceptionsState(V8DebuggerImpl::PauseOnExceptionsState newState)
    {
        if (!m_debugger || !m_debugger->enabled())
            return newState;
        V8DebuggerImpl::PauseOnExceptionsState presentState = m_debugger->getPauseOnExceptionsState();
        if (presentState != newState)
            m_debugger->setPauseOnExceptionsState(newState);
        return presentState;
    }

    V8DebuggerImpl* m_debugger;
    V8DebuggerImpl::PauseOnExceptionsState m_previousPauseOnExceptionsState;
};

} // namespace blink

#endif // IgnoreExceptionsScope_h
