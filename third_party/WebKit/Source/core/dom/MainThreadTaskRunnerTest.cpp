/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "core/dom/MainThreadTaskRunner.h"

#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/events/EventQueue.h"
#include "core/testing/UnitTestHelpers.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class NullEventQueue : public EventQueue {
public:
    NullEventQueue() { }
    virtual ~NullEventQueue() { }
    virtual bool enqueueEvent(PassRefPtr<Event>) OVERRIDE { return true; }
    virtual bool cancelEvent(Event*) OVERRIDE { return true; }
    virtual void close() OVERRIDE { }
};

class NullExecutionContext : public ExecutionContext, public RefCounted<NullExecutionContext> {
public:
    using RefCounted<NullExecutionContext>::ref;
    using RefCounted<NullExecutionContext>::deref;

    virtual void refExecutionContext() OVERRIDE { ref(); }
    virtual void derefExecutionContext() OVERRIDE { deref(); }
    virtual EventQueue* eventQueue() const OVERRIDE { return m_queue.get(); }
    virtual bool tasksNeedSuspension() { return m_tasksNeedSuspension; }

    void setTasksNeedSuspention(bool flag) { m_tasksNeedSuspension = flag; }

    NullExecutionContext();

private:
    bool m_tasksNeedSuspension;
    OwnPtr<EventQueue> m_queue;
};

NullExecutionContext::NullExecutionContext()
    : m_tasksNeedSuspension(false)
    , m_queue(adoptPtr(new NullEventQueue()))
{
}

class MarkingBooleanTask FINAL : public ExecutionContextTask {
public:
    static PassOwnPtr<MarkingBooleanTask> create(bool* toBeMarked)
    {
        return adoptPtr(new MarkingBooleanTask(toBeMarked));
    }


    virtual ~MarkingBooleanTask() { }

private:
    MarkingBooleanTask(bool* toBeMarked) : m_toBeMarked(toBeMarked) { }

    virtual void performTask(ExecutionContext* context) OVERRIDE
    {
        *m_toBeMarked = true;
    }

    bool* m_toBeMarked;
};

TEST(MainThreadTaskRunnerTest, PostTask)
{
    RefPtr<NullExecutionContext> context = adoptRef(new NullExecutionContext());
    OwnPtr<MainThreadTaskRunner> runner = MainThreadTaskRunner::create(context.get());
    bool isMarked = false;

    runner->postTask(MarkingBooleanTask::create(&isMarked));
    EXPECT_FALSE(isMarked);
    WebCore::testing::runPendingTasks();
    EXPECT_TRUE(isMarked);
}

TEST(MainThreadTaskRunnerTest, SuspendTask)
{
    RefPtr<NullExecutionContext> context = adoptRef(new NullExecutionContext());
    OwnPtr<MainThreadTaskRunner> runner = MainThreadTaskRunner::create(context.get());
    bool isMarked = false;

    context->setTasksNeedSuspention(true);
    runner->postTask(MarkingBooleanTask::create(&isMarked));
    runner->suspend();
    WebCore::testing::runPendingTasks();
    EXPECT_FALSE(isMarked);

    context->setTasksNeedSuspention(false);
    runner->resume();
    WebCore::testing::runPendingTasks();
    EXPECT_TRUE(isMarked);
}

TEST(MainThreadTaskRunnerTest, RemoveRunner)
{
    RefPtr<NullExecutionContext> context = adoptRef(new NullExecutionContext());
    OwnPtr<MainThreadTaskRunner> runner = MainThreadTaskRunner::create(context.get());
    bool isMarked = false;

    context->setTasksNeedSuspention(true);
    runner->postTask(MarkingBooleanTask::create(&isMarked));
    runner.clear();
    WebCore::testing::runPendingTasks();
    EXPECT_FALSE(isMarked);
}

}
