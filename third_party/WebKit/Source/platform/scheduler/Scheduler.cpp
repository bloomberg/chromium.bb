// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/scheduler/Scheduler.h"

#include "platform/Task.h"
#include "platform/TraceEvent.h"
#include "platform/TraceLocation.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"

namespace blink {

namespace {

class MainThreadTaskAdapter : public blink::WebThread::Task {
public:
    explicit MainThreadTaskAdapter(const TraceLocation& location, const Scheduler::Task& task)
        : m_location(location)
        , m_task(task)
    {
    }

    // WebThread::Task implementation.
    virtual void run() OVERRIDE
    {
        TRACE_EVENT2("blink", "MainThreadTaskAdapter::run",
            "src_file", m_location.fileName(),
            "src_func", m_location.functionName());
        m_task();
    }

private:
    const TraceLocation m_location;
    Scheduler::Task m_task;
};

class MainThreadIdleTaskAdapter : public blink::WebThread::Task {
public:
    MainThreadIdleTaskAdapter(const Scheduler::IdleTask& idleTask, double allottedTimeMs)
        : m_idleTask(idleTask)
        , m_allottedTimeMs(allottedTimeMs)
    {
    }

    // WebThread::Task implementation.
    virtual void run() OVERRIDE
    {
        TRACE_EVENT1("blink", "MainThreadIdleTaskAdapter::run", "allottedTime", m_allottedTimeMs);
        m_idleTask(m_allottedTimeMs);
    }

private:
    Scheduler::IdleTask m_idleTask;
    double m_allottedTimeMs;
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

void Scheduler::scheduleTask(const TraceLocation& location, const Task& task)
{
    m_mainThread->postTask(new MainThreadTaskAdapter(location, task));
}

void Scheduler::scheduleIdleTask(const IdleTask& idleTask)
{
    // TODO: send a real allottedTime here.
    m_mainThread->postTask(new MainThreadIdleTaskAdapter(idleTask, 0));
}

void Scheduler::postTask(const TraceLocation& location, const Task& task)
{
    scheduleTask(location, task);
}

void Scheduler::postInputTask(const TraceLocation& location, const Task& task)
{
    scheduleTask(location, task);
}

void Scheduler::postCompositorTask(const TraceLocation& location, const Task& task)
{
    scheduleTask(location, task);
}

void Scheduler::postIdleTask(const IdleTask& idleTask)
{
    scheduleIdleTask(idleTask);
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
