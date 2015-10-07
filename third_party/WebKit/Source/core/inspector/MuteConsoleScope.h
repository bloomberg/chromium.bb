// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MuteConsoleScope_h
#define MuteConsoleScope_h

namespace blink {

template <class T>
class MuteConsoleScope {
public:
    explicit MuteConsoleScope(T* agent) : m_agent(agent)
    {
        m_agent->muteConsole();
    }
    ~MuteConsoleScope()
    {
        m_agent->unmuteConsole();
    }

private:
    T* m_agent;
};

} // namespace blink

#endif // MuteConsoleScope_h
