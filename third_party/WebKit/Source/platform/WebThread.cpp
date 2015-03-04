// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/platform/WebThread.h"

#include "public/platform/WebTraceLocation.h"
#include "wtf/Assertions.h"
#include "wtf/OwnPtr.h"

#if OS(WIN)
#include <windows.h>
#elif OS(POSIX)
#include <unistd.h>
#endif

namespace blink {

namespace {
#if OS(WIN)
static_assert(sizeof(blink::PlatformThreadId) >= sizeof(DWORD), "size of platform thread id is too small");
#elif OS(POSIX)
static_assert(sizeof(blink::PlatformThreadId) >= sizeof(pid_t), "size of platform thread id is too small");
#else
#error Unexpected platform
#endif

class FunctionTask: public WebThread::Task {
    WTF_MAKE_NONCOPYABLE(FunctionTask);
public:
    FunctionTask(PassOwnPtr<Function<void()>> function)
        : m_function(function)
    {
    }

    void run() override
    {
        (*m_function)();
    }
private:
    OwnPtr<Function<void()>> m_function;
};

} // namespace

void WebThread::postTask(const WebTraceLocation& location, PassOwnPtr<Function<void()>> function)
{
    postTask(location, new FunctionTask(function));
}

void WebThread::postDelayedTask(const WebTraceLocation& location, PassOwnPtr<Function<void()>> function, long long delayMs)
{
    postDelayedTask(location, new FunctionTask(function), delayMs);
}

} // namespace blink
