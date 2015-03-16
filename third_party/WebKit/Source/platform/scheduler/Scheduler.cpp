// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/scheduler/Scheduler.h"

#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

class IdleTaskRunner : public WebScheduler::IdleTask {
    WTF_MAKE_NONCOPYABLE(IdleTaskRunner);

public:
    explicit IdleTaskRunner(PassOwnPtr<Scheduler::IdleTask> task)
        : m_task(task)
    {
    }

    ~IdleTaskRunner() override
    {
    }

    // WebScheduler::IdleTask implementation.
    void run(double deadlineSeconds) override
    {
        (*m_task)(deadlineSeconds);
    }
private:
    OwnPtr<Scheduler::IdleTask> m_task;
};

Scheduler* Scheduler::s_sharedScheduler = nullptr;

void Scheduler::shutdown()
{
    delete s_sharedScheduler;
    s_sharedScheduler = nullptr;
}

Scheduler* Scheduler::shared()
{
    if (!s_sharedScheduler)
        s_sharedScheduler = new Scheduler(Platform::current()->scheduler());
    return s_sharedScheduler;
}

void Scheduler::setForTesting(Scheduler* scheduler)
{
    s_sharedScheduler = scheduler;
}

Scheduler::Scheduler(WebScheduler* webScheduler)
    : m_webScheduler(webScheduler)
{
}

Scheduler::~Scheduler()
{
    if (m_webScheduler)
        m_webScheduler->shutdown();
}

void Scheduler::postIdleTask(const WebTraceLocation& location, PassOwnPtr<IdleTask> idleTask)
{
    if (m_webScheduler)
        m_webScheduler->postIdleTask(location, new IdleTaskRunner(idleTask));
}

void Scheduler::postNonNestableIdleTask(const WebTraceLocation& location, PassOwnPtr<IdleTask> idleTask)
{
    if (m_webScheduler)
        m_webScheduler->postNonNestableIdleTask(location, new IdleTaskRunner(idleTask));
}

void Scheduler::postIdleTaskAfterWakeup(const WebTraceLocation& location, PassOwnPtr<IdleTask> idleTask)
{
    if (m_webScheduler)
        m_webScheduler->postIdleTaskAfterWakeup(location, new IdleTaskRunner(idleTask));
}

void Scheduler::postLoadingTask(const WebTraceLocation& location, WebThread::Task* task)
{
    if (m_webScheduler)
        m_webScheduler->postLoadingTask(location, task);
}

bool Scheduler::shouldYieldForHighPriorityWork() const
{
    if (m_webScheduler)
        return m_webScheduler->shouldYieldForHighPriorityWork();
    return false;
}

} // namespace blink
