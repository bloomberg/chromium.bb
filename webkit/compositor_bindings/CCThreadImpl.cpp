// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "CCThreadImpl.h"

#include "CCCompletionEvent.h"
#include <public/Platform.h>
#include <public/WebThread.h>

using WebCore::CCThread;
using WebCore::CCCompletionEvent;

namespace WebKit {

// Task that, when runs, places the current thread ID into the provided
// pointer and signals a completion event.
//
// Does not provide a PassOwnPtr<GetThreadIDTask>::create method because
// the resulting object is always handed into a WebThread, which does not understand
// PassOwnPtrs.
class GetThreadIDTask : public WebThread::Task {
public:
    GetThreadIDTask(base::PlatformThreadId* result, CCCompletionEvent* completion)
         : m_completion(completion)
         , m_result(result) { }

    virtual ~GetThreadIDTask() { }

    virtual void run()
    {
        *m_result = base::PlatformThread::CurrentId();
        m_completion->signal();
    }

private:
    CCCompletionEvent* m_completion;
    base::PlatformThreadId* m_result;
};

// General adapter from a CCThread::Task to a WebThread::Task.
class CCThreadTaskAdapter : public WebThread::Task {
public:
    CCThreadTaskAdapter(PassOwnPtr<CCThread::Task> task) : m_task(task) { }

    virtual ~CCThreadTaskAdapter() { }

    virtual void run()
    {
        m_task->performTask();
    }

private:
    OwnPtr<CCThread::Task> m_task;
};

PassOwnPtr<CCThread> CCThreadImpl::createForCurrentThread()
{
    return adoptPtr(new CCThreadImpl(Platform::current()->currentThread(), true));
}

PassOwnPtr<CCThread> CCThreadImpl::createForDifferentThread(WebThread* thread)
{
    return adoptPtr(new CCThreadImpl(thread, false));
}

CCThreadImpl::~CCThreadImpl()
{
}

void CCThreadImpl::postTask(PassOwnPtr<CCThread::Task> task)
{
    m_thread->postTask(new CCThreadTaskAdapter(task));
}

void CCThreadImpl::postDelayedTask(PassOwnPtr<CCThread::Task> task, long long delayMs)
{
    m_thread->postDelayedTask(new CCThreadTaskAdapter(task), delayMs);
}

base::PlatformThreadId CCThreadImpl::threadID() const
{
    return m_threadID;
}

CCThreadImpl::CCThreadImpl(WebThread* thread, bool currentThread)
    : m_thread(thread)
{
    if (currentThread) {
        m_threadID = base::PlatformThread::CurrentId();
        return;
    }

    // Get the threadId for the newly-created thread by running a task
    // on that thread, blocking on the result.
    CCCompletionEvent completion;
    m_thread->postTask(new GetThreadIDTask(&m_threadID, &completion));
    completion.wait();
}

} // namespace WebKit
