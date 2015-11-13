// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MuteConsoleScope_h
#define MuteConsoleScope_h

#include "platform/heap/Handle.h"

namespace blink {

template <class T>
class MuteConsoleScope {
    STACK_ALLOCATED();
public:
    MuteConsoleScope()
    {
    }
    explicit MuteConsoleScope(T* agent)
    {
        enter(agent);
    }
    ~MuteConsoleScope()
    {
        exit();
    }

    void enter(T* agent)
    {
        ASSERT(!m_agent);
        m_agent = agent;
        m_agent->muteConsole();
    }

    void exit()
    {
        if (!m_agent)
            return;

        m_agent->unmuteConsole();
        m_agent = nullptr;
    }

private:
    RawPtrWillBeMember<T> m_agent = nullptr;
};

} // namespace blink

#endif // MuteConsoleScope_h
