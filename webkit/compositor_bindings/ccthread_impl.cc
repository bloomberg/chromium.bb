// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "ccthread_impl.h"

#include "cc/completion_event.h"
#include "third_party/WebKit/Source/Platform/chromium/public/Platform.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebThread.h"

using cc::Thread;
using cc::CompletionEvent;

namespace WebKit {

// Task that, when runs, places the current thread ID into the provided
// pointer and signals a completion event.
//
// Does not provide a PassOwnPtr<GetThreadIDTask>::create method because
// the resulting object is always handed into a WebThread, which does not understand
// PassOwnPtrs.
class GetThreadIDTask : public WebThread::Task {
public:
    GetThreadIDTask(base::PlatformThreadId* result, CompletionEvent* completion)
         : m_completion(completion)
         , m_result(result) { }

    virtual ~GetThreadIDTask() { }

    virtual void run()
    {
        *m_result = base::PlatformThread::CurrentId();
        m_completion->signal();
    }

private:
    CompletionEvent* m_completion;
    base::PlatformThreadId* m_result;
};

// General adapter from a Thread::Task to a WebThread::Task.
class ThreadTaskAdapter : public WebThread::Task {
public:
    explicit ThreadTaskAdapter(PassOwnPtr<Thread::Task> task) : m_task(task) { }

    virtual ~ThreadTaskAdapter() { }

    virtual void run()
    {
        m_task->performTask();
    }

private:
    OwnPtr<Thread::Task> m_task;
};

scoped_ptr<Thread> CCThreadImpl::createForCurrentThread()
{
    return scoped_ptr<Thread>(new CCThreadImpl(Platform::current()->currentThread(), true)).Pass();
}

scoped_ptr<Thread> CCThreadImpl::createForDifferentThread(WebThread* thread)
{
    return scoped_ptr<Thread>(new CCThreadImpl(thread, false)).Pass();
}

CCThreadImpl::~CCThreadImpl()
{
}

void CCThreadImpl::postTask(PassOwnPtr<Thread::Task> task)
{
    m_thread->postTask(new ThreadTaskAdapter(task));
}

void CCThreadImpl::postDelayedTask(PassOwnPtr<Thread::Task> task, long long delayMs)
{
    m_thread->postDelayedTask(new ThreadTaskAdapter(task), delayMs);
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
    CompletionEvent completion;
    m_thread->postTask(new GetThreadIDTask(&m_threadID, &completion));
    completion.wait();
}

} // namespace WebKit
