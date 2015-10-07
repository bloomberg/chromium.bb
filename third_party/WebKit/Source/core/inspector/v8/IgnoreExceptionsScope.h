// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IgnoreExceptionsScope_h
#define IgnoreExceptionsScope_h

#include "core/inspector/v8/V8DebuggerImpl.h"

namespace blink {

class IgnoreExceptionsScope {
public:
    explicit IgnoreExceptionsScope(V8DebuggerImpl* debugger)
        : m_debugger(debugger)
        , m_previousPauseOnExceptionsState(V8Debugger::DontPauseOnExceptions)
    {
        m_previousPauseOnExceptionsState = setPauseOnExceptionsState(V8Debugger::DontPauseOnExceptions);
    }
    ~IgnoreExceptionsScope()
    {
        setPauseOnExceptionsState(m_previousPauseOnExceptionsState);
    }

private:
    V8Debugger::PauseOnExceptionsState setPauseOnExceptionsState(V8Debugger::PauseOnExceptionsState newState)
    {
        ASSERT(m_debugger);
        if (!m_debugger->enabled())
            return newState;
        V8Debugger::PauseOnExceptionsState presentState = m_debugger->pauseOnExceptionsState();
        if (presentState != newState)
            m_debugger->setPauseOnExceptionsState(newState);
        return presentState;
    }

    V8DebuggerImpl* m_debugger;
    V8Debugger::PauseOnExceptionsState m_previousPauseOnExceptionsState;
};

} // namespace blink

#endif // IgnoreExceptionsScope_h
