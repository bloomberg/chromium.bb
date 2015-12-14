// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebThreadSupportingGC_h
#define WebThreadSupportingGC_h

#include "platform/heap/GCTaskRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebThread.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

// WebThreadSupportingGC wraps a WebThread and adds support for attaching
// to and detaching from the Blink GC infrastructure. The initialize method
// must be called during initialization on the WebThread and before the
// thread allocates any objects managed by the Blink GC. The shutdown
// method must be called on the WebThread during shutdown when the thread
// no longer needs to access objects managed by the Blink GC.
//
// WebThreadSupportingGC usually internally creates and owns WebThread unless
// an existing WebThread is given via createForThread.
class PLATFORM_EXPORT WebThreadSupportingGC final {
    USING_FAST_MALLOC(WebThreadSupportingGC);
    WTF_MAKE_NONCOPYABLE(WebThreadSupportingGC);
public:
    static PassOwnPtr<WebThreadSupportingGC> create(const char* name);
    static PassOwnPtr<WebThreadSupportingGC> createForThread(WebThread*);
    ~WebThreadSupportingGC();

    void postTask(const WebTraceLocation& location, WebTaskRunner::Task* task)
    {
        m_thread->taskRunner()->postTask(location, task);
    }

    void postDelayedTask(const WebTraceLocation& location, WebTaskRunner::Task* task, long long delayMs)
    {
        m_thread->taskRunner()->postDelayedTask(location, task, delayMs);
    }

    bool isCurrentThread() const
    {
        return m_thread->isCurrentThread();
    }

    void addTaskObserver(WebThread::TaskObserver* observer)
    {
        m_thread->addTaskObserver(observer);
    }

    void removeTaskObserver(WebThread::TaskObserver* observer)
    {
        m_thread->removeTaskObserver(observer);
    }

    void initialize();
    void shutdown();

    WebThread& platformThread() const
    {
        ASSERT(m_thread);
        return *m_thread;
    }

private:
    WebThreadSupportingGC(const char* name, WebThread*);

    OwnPtr<GCTaskRunner> m_gcTaskRunner;

    // m_thread is guaranteed to be non-null after this instance is constructed.
    // m_owningThread is non-null unless this instance is constructed for an
    // existing thread via createForThread().
    WebThread* m_thread = nullptr;
    OwnPtr<WebThread> m_owningThread;
};

}

#endif
