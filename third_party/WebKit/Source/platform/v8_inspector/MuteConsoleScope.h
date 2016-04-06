// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MuteConsoleScope_h
#define MuteConsoleScope_h

#include "platform/inspector_protocol/Allocator.h"
#include "platform/v8_inspector/V8DebuggerImpl.h"
#include "platform/v8_inspector/public/V8DebuggerClient.h"

namespace blink {

class MuteConsoleScope {
    PROTOCOL_DISALLOW_COPY(MuteConsoleScope);
public:
    explicit MuteConsoleScope(V8DebuggerImpl* debugger)
        : m_debugger(debugger)
    {
        if (m_debugger)
            m_debugger->client()->muteConsole();
    }

    ~MuteConsoleScope()
    {
        if (m_debugger)
            m_debugger->client()->unmuteConsole();
    }

private:
    V8DebuggerImpl* m_debugger;
};

} // namespace blink

#endif // MuteConsoleScope_h
