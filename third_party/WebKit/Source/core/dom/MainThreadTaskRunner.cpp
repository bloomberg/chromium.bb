/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/dom/MainThreadTaskRunner.h"

#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"

namespace blink {

class MainThreadTask : public WebTaskRunner::Task {
    WTF_MAKE_NONCOPYABLE(MainThreadTask); USING_FAST_MALLOC(MainThreadTask);
public:
    MainThreadTask(WeakPtrWillBeRawPtr<MainThreadTaskRunner> runner, PassOwnPtr<ExecutionContextTask> task, bool isInspectorTask)
        : m_runner(runner)
        , m_task(task)
        , m_isInspectorTask(isInspectorTask)
    {
    }

    void run() override;

private:
    WeakPtrWillBeCrossThreadWeakPersistent<MainThreadTaskRunner> m_runner;
    OwnPtr<ExecutionContextTask> m_task;
    bool m_isInspectorTask;
};

void MainThreadTask::run()
{
    ASSERT(isMainThread());

    if (!m_runner.get())
        return;
    m_runner->perform(m_task.release(), m_isInspectorTask);
}

MainThreadTaskRunner::MainThreadTaskRunner(ExecutionContext* context)
    : m_context(context)
#if !ENABLE(OILPAN)
    , m_weakFactory(this)
#endif
    , m_pendingTasksTimer(this, &MainThreadTaskRunner::pendingTasksTimerFired)
    , m_suspended(false)
{
}

MainThreadTaskRunner::~MainThreadTaskRunner()
{
}

DEFINE_TRACE(MainThreadTaskRunner)
{
    visitor->trace(m_context);
}

void MainThreadTaskRunner::postTask(const WebTraceLocation& location, PassOwnPtr<ExecutionContextTask> task)
{
    if (!task->taskNameForInstrumentation().isEmpty())
        InspectorInstrumentation::didPostExecutionContextTask(m_context, task.get());
    Platform::current()->mainThread()->taskRunner()->postTask(location, new MainThreadTask(createWeakPointerToSelf(), task, false));
}

void MainThreadTaskRunner::postInspectorTask(const WebTraceLocation& location, PassOwnPtr<ExecutionContextTask> task)
{
    Platform::current()->mainThread()->taskRunner()->postTask(location, new MainThreadTask(createWeakPointerToSelf(), task, true));
}

void MainThreadTaskRunner::perform(PassOwnPtr<ExecutionContextTask> task, bool isInspectorTask)
{
    if (!isInspectorTask && (m_context->tasksNeedSuspension() || !m_pendingTasks.isEmpty())) {
        m_pendingTasks.append(task);
        return;
    }

    const bool instrumenting = !isInspectorTask && !task->taskNameForInstrumentation().isEmpty();
    if (instrumenting)
        InspectorInstrumentation::willPerformExecutionContextTask(m_context, task.get());
    task->performTask(m_context);
    if (instrumenting)
        InspectorInstrumentation::didPerformExecutionContextTask(m_context);
}

void MainThreadTaskRunner::suspend()
{
    ASSERT(!m_suspended);
    m_pendingTasksTimer.stop();
    m_suspended = true;
}

void MainThreadTaskRunner::resume()
{
    ASSERT(m_suspended);
    if (!m_pendingTasks.isEmpty())
        m_pendingTasksTimer.startOneShot(0, BLINK_FROM_HERE);

    m_suspended = false;
}

void MainThreadTaskRunner::pendingTasksTimerFired(Timer<MainThreadTaskRunner>*)
{
    while (!m_pendingTasks.isEmpty()) {
        OwnPtr<ExecutionContextTask> task = m_pendingTasks[0].release();
        m_pendingTasks.remove(0);
        const bool instrumenting = !task->taskNameForInstrumentation().isEmpty();
        if (instrumenting)
            InspectorInstrumentation::willPerformExecutionContextTask(m_context, task.get());
        task->performTask(m_context);
        if (instrumenting)
            InspectorInstrumentation::didPerformExecutionContextTask(m_context);
    }
}

WeakPtrWillBeRawPtr<MainThreadTaskRunner> MainThreadTaskRunner::createWeakPointerToSelf()
{
#if ENABLE(OILPAN)
    return this;
#else
    return m_weakFactory.createWeakPtr();
#endif
}

} // namespace blink
