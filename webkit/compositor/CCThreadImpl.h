// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CCThread.h"
#include <wtf/OwnPtr.h>
#include <wtf/Threading.h>

#ifndef CCThreadImpl_h
#define CCThreadImpl_h

namespace WebKit {

class WebThread;

// Implements CCThread in terms of WebThread.
class CCThreadImpl : public WebCore::CCThread {
public:
    static PassOwnPtr<WebCore::CCThread> create(WebThread*);
    virtual ~CCThreadImpl();
    virtual void postTask(PassOwnPtr<WebCore::CCThread::Task>);
    virtual void postDelayedTask(PassOwnPtr<WebCore::CCThread::Task>, long long delayMs);
    WTF::ThreadIdentifier threadID() const;

private:
    explicit CCThreadImpl(WebThread*);

    WebThread* m_thread;
    WTF::ThreadIdentifier m_threadID;
};

} // namespace WebKit

#endif
