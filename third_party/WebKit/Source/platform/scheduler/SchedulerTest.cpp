// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/scheduler/Scheduler.h"

#include "platform/TestingPlatformSupport.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"

#include <gtest/gtest.h>

using blink::Scheduler;

namespace {

class TestMainThread : public blink::WebThread {
public:
    // blink::WebThread implementation.
    virtual void postTask(Task* task) OVERRIDE
    {
        m_pendingTasks.append(adoptPtr(task));
    }

    virtual void postDelayedTask(Task* task, long long delayMs) OVERRIDE
    {
        ASSERT_NOT_REACHED();
    }

    virtual bool isCurrentThread() const OVERRIDE
    {
        return true;
    }

    virtual void enterRunLoop() OVERRIDE
    {
        ASSERT_NOT_REACHED();
    }

    virtual void exitRunLoop() OVERRIDE
    {
        ASSERT_NOT_REACHED();
    }

    void runPendingTasks()
    {
        while (!m_pendingTasks.isEmpty())
            m_pendingTasks.takeFirst()->run();
    }

private:
    WTF::Deque<OwnPtr<Task> > m_pendingTasks;
};

class SchedulerTestingPlatformSupport : blink::TestingPlatformSupport {
public:
    SchedulerTestingPlatformSupport()
        : TestingPlatformSupport(TestingPlatformSupport::Config())
        , m_sharedTimerFunction(nullptr)
        , m_sharedTimerRunning(false)
        , m_sharedTimerFireInterval(0)
    {
    }

    // blink::Platform implementation.
    virtual blink::WebThread* currentThread() OVERRIDE
    {
        return &m_mainThread;
    }

    virtual void setSharedTimerFiredFunction(SharedTimerFunction timerFunction) OVERRIDE
    {
        m_sharedTimerFunction = timerFunction;
    }

    virtual void setSharedTimerFireInterval(double)
    {
        m_sharedTimerFireInterval = 0;
        m_sharedTimerRunning = true;
    }

    virtual void stopSharedTimer()
    {
        m_sharedTimerRunning = false;
    }

    void runPendingTasks()
    {
        m_mainThread.runPendingTasks();
    }

    bool sharedTimerRunning() const
    {
        return m_sharedTimerRunning;
    }

    double sharedTimerFireInterval() const
    {
        return m_sharedTimerFireInterval;
    }

    void triggerSharedTimer()
    {
        m_sharedTimerFunction();
    }

private:
    TestMainThread m_mainThread;
    SharedTimerFunction m_sharedTimerFunction;
    bool m_sharedTimerRunning;
    double m_sharedTimerFireInterval;
};

class SchedulerTest : public testing::Test {
public:
    SchedulerTest()
    {
        Scheduler::initializeOnMainThread();
        m_scheduler = Scheduler::shared();
    }

    ~SchedulerTest()
    {
        Scheduler::shutdown();
    }

    void runPendingTasks()
    {
        m_platformSupport.runPendingTasks();
    }

protected:
    SchedulerTestingPlatformSupport m_platformSupport;
    Scheduler* m_scheduler;
};

void orderedTestTask(int value, int* result)
{
    *result = (*result << 4) | value;
}

void unorderedTestTask(int value, int* result)
{
    *result += value;
}

TEST_F(SchedulerTest, TestPostTask)
{
    int result = 0;
    m_scheduler->postTask(bind(&orderedTestTask, 1, &result));
    m_scheduler->postTask(bind(&orderedTestTask, 2, &result));
    m_scheduler->postTask(bind(&orderedTestTask, 3, &result));
    m_scheduler->postTask(bind(&orderedTestTask, 4, &result));
    runPendingTasks();
    EXPECT_EQ(0x1234, result);
}

TEST_F(SchedulerTest, TestPostMixedTaskTypes)
{
    int result = 0;
    m_scheduler->postTask(bind(&unorderedTestTask, 1, &result));
    m_scheduler->postInputTask(bind(&unorderedTestTask, 1, &result));
    m_scheduler->postCompositorTask(bind(&unorderedTestTask, 1, &result));
    m_scheduler->postTask(bind(&unorderedTestTask, 1, &result));
    runPendingTasks();
    EXPECT_EQ(4, result);
}

int s_sharedTimerTickCount;
void sharedTimerFunction()
{
    s_sharedTimerTickCount++;
}

TEST_F(SchedulerTest, TestSharedTimer)
{
    s_sharedTimerTickCount = 0;
    m_scheduler->setSharedTimerFiredFunction(&sharedTimerFunction);
    EXPECT_FALSE(m_platformSupport.sharedTimerRunning());
    m_scheduler->setSharedTimerFireInterval(0);
    EXPECT_TRUE(m_platformSupport.sharedTimerRunning());

    m_platformSupport.triggerSharedTimer();
    EXPECT_EQ(1, s_sharedTimerTickCount);

    m_scheduler->stopSharedTimer();
    EXPECT_FALSE(m_platformSupport.sharedTimerRunning());

    m_scheduler->setSharedTimerFiredFunction(nullptr);
    EXPECT_FALSE(m_platformSupport.sharedTimerRunning());
}

} // namespace
