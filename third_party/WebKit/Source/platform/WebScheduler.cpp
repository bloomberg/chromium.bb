// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/platform/WebScheduler.h"

#include "public/platform/WebTraceLocation.h"
#include "wtf/Assertions.h"
#include "wtf/OwnPtr.h"

namespace blink {

namespace {
class TaskRunner : public WebThread::Task {
    WTF_MAKE_NONCOPYABLE(TaskRunner);
public:
    explicit TaskRunner(PassOwnPtr<WebScheduler::Task> task)
        : m_task(task)
    {
    }

    ~TaskRunner() override
    {
    }

    // WebThread::Task implementation.
    void run() override
    {
        (*m_task)();
    }

private:
    OwnPtr<WebScheduler::Task> m_task;
};

class IdleTaskRunner : public WebThread::IdleTask {
    WTF_MAKE_NONCOPYABLE(IdleTaskRunner);

public:
    explicit IdleTaskRunner(PassOwnPtr<WebScheduler::IdleTask> task)
        : m_task(task)
    {
    }

    ~IdleTaskRunner() override
    {
    }

    // WebThread::IdleTask implementation.
    void run(double deadlineSeconds) override
    {
        (*m_task)(deadlineSeconds);
    }

private:
    OwnPtr<WebScheduler::IdleTask> m_task;
};

} // namespace

void WebScheduler::postIdleTask(const WebTraceLocation& location, PassOwnPtr<IdleTask> idleTask)
{
    postIdleTask(location, new IdleTaskRunner(idleTask));
}

void WebScheduler::postNonNestableIdleTask(const WebTraceLocation& location, PassOwnPtr<IdleTask> idleTask)
{
    postNonNestableIdleTask(location, new IdleTaskRunner(idleTask));
}

void WebScheduler::postIdleTaskAfterWakeup(const WebTraceLocation& location, PassOwnPtr<IdleTask> idleTask)
{
    postIdleTaskAfterWakeup(location, new IdleTaskRunner(idleTask));
}

void WebScheduler::postLoadingTask(const WebTraceLocation& location, PassOwnPtr<Task> task)
{
    postLoadingTask(location, new TaskRunner(task));
}

} // namespace blink
