// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CCThread.h"
#include "base/threading/platform_thread.h"
#include <wtf/OwnPtr.h>
#include <wtf/Threading.h>

#ifndef CCThreadImpl_h
#define CCThreadImpl_h

namespace WebKit {

class WebThread;

// Implements CCThread in terms of WebThread.
class CCThreadImpl : public cc::CCThread {
public:
    // Creates a CCThreadImpl wrapping the current thread.
    static PassOwnPtr<cc::CCThread> createForCurrentThread();

    // Creates a CCThread wrapping a non-current WebThread.
    static PassOwnPtr<cc::CCThread> createForDifferentThread(WebThread*);

    virtual ~CCThreadImpl();
    virtual void postTask(PassOwnPtr<cc::CCThread::Task>);
    virtual void postDelayedTask(PassOwnPtr<cc::CCThread::Task>, long long delayMs);
    base::PlatformThreadId threadID() const;

private:
    CCThreadImpl(WebThread*, bool currentThread);

    WebThread* m_thread;
    base::PlatformThreadId m_threadID;
};

} // namespace WebKit

#endif
