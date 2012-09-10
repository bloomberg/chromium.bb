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
class CCThreadImpl : public WebCore::CCThread {
public:
    // Creates a CCThreadImpl wrapping the current thread.
    static PassOwnPtr<WebCore::CCThread> createForCurrentThread();

    // Creates a CCThread wrapping a non-current WebThread.
    static PassOwnPtr<WebCore::CCThread> createForDifferentThread(WebThread*);

    virtual ~CCThreadImpl();
    virtual void postTask(PassOwnPtr<WebCore::CCThread::Task>);
    virtual void postDelayedTask(PassOwnPtr<WebCore::CCThread::Task>, long long delayMs);
    base::PlatformThreadId threadID() const;

private:
    CCThreadImpl(WebThread*, bool currentThread);

    WebThread* m_thread;
    base::PlatformThreadId m_threadID;
};

} // namespace WebKit

#endif
