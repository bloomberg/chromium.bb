// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/threading/platform_thread.h"
#include "cc/thread.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

#ifndef CCThreadImpl_h
#define CCThreadImpl_h

namespace WebKit {

class WebThread;

// Implements Thread in terms of WebThread.
class CCThreadImpl : public cc::Thread {
public:
    // Creates a CCThreadImpl wrapping the current thread.
    static scoped_ptr<cc::Thread> createForCurrentThread();

    // Creates a Thread wrapping a non-current WebThread.
    static scoped_ptr<cc::Thread> createForDifferentThread(WebThread*);

    virtual ~CCThreadImpl();
    virtual void postTask(PassOwnPtr<cc::Thread::Task>);
    virtual void postDelayedTask(PassOwnPtr<cc::Thread::Task>, long long delayMs);
    base::PlatformThreadId threadID() const;

private:
    CCThreadImpl(WebThread*, bool currentThread);

    WebThread* m_thread;
    base::PlatformThreadId m_threadID;
};

} // namespace WebKit

#endif
