// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/scheduler/Scheduler.h"

#include "platform/Task.h"
#include "platform/TraceEvent.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"

namespace blink {

namespace {

class MainThreadTaskAdapter : public blink::WebThread::Task {
public:
    explicit MainThreadTaskAdapter(const Scheduler::Task& task)
        : m_task(task)
    {
    }

    // WebThread::Task implementation.
    virtual void run() OVERRIDE
    {
        TRACE_EVENT0("blink", "MainThreadTaskAdapter::run");
        m_task();
    }

private:
    Scheduler::Task m_task;
};

}

Scheduler* Scheduler::s_sharedScheduler = nullptr;

void Scheduler::initializeOnMainThread()
{
    s_sharedScheduler = new Scheduler();
}

void Scheduler::shutdown()
{
    delete s_sharedScheduler;
    s_sharedScheduler = nullptr;
}

Scheduler* Scheduler::shared()
{
    return s_sharedScheduler;
}

Scheduler::Scheduler()
    : m_mainThread(blink::Platform::current()->currentThread())
    , m_sharedTimerFunction(nullptr)
{
}

Scheduler::~Scheduler()
{
}

void Scheduler::scheduleTask(const Task& task)
{
    m_mainThread->postTask(new MainThreadTaskAdapter(task));
}

void Scheduler::postTask(const Task& task)
{
    scheduleTask(task);
}

void Scheduler::postInputTask(const Task& task)
{
    scheduleTask(task);
}

void Scheduler::postCompositorTask(const Task& task)
{
    scheduleTask(task);
}

void Scheduler::tickSharedTimer()
{
    TRACE_EVENT0("blink", "Scheduler::tickSharedTimer");
    m_sharedTimerFunction();
}

void Scheduler::sharedTimerAdapter()
{
    shared()->tickSharedTimer();
}

void Scheduler::setSharedTimerFiredFunction(void (*function)())
{
    m_sharedTimerFunction = function;
    blink::Platform::current()->setSharedTimerFiredFunction(function ? &Scheduler::sharedTimerAdapter : nullptr);
}

void Scheduler::setSharedTimerFireInterval(double interval)
{
    blink::Platform::current()->setSharedTimerFireInterval(interval);
}

void Scheduler::stopSharedTimer()
{
    blink::Platform::current()->stopSharedTimer();
}

bool Scheduler::shouldYieldForHighPriorityWork()
{
    return false;
}

} // namespace blink
