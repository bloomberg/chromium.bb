// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/scheduler/Scheduler.h"

#include "platform/TraceLocation.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

class IdleTaskRunner : public WebScheduler::IdleTask {
public:
    explicit IdleTaskRunner(const Scheduler::IdleTask& task)
        : m_task(task)
    {
    }

    virtual ~IdleTaskRunner()
    {
    }

    // WebScheduler::IdleTask implementation.
    void run(double deadlineSeconds) override
    {
        m_task(deadlineSeconds);
    }
private:
    Scheduler::IdleTask m_task;
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

Scheduler::Scheduler(WebScheduler* webScheduler)
    : m_webScheduler(webScheduler)
{
}

Scheduler::~Scheduler()
{
    if (m_webScheduler)
        m_webScheduler->shutdown();
}

void Scheduler::postIdleTask(const TraceLocation& location, const IdleTask& idleTask)
{
    if (m_webScheduler)
        m_webScheduler->postIdleTask(WebTraceLocation(location), new IdleTaskRunner(idleTask));
}

bool Scheduler::shouldYieldForHighPriorityWork() const
{
    if (m_webScheduler)
        return m_webScheduler->shouldYieldForHighPriorityWork();
    return false;
}

} // namespace blink
