// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/scheduler/Scheduler.h"

#include "platform/TestingPlatformSupport.h"
#include "platform/TraceLocation.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using blink::Scheduler;
using namespace std;

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
        : m_reentrantCount(0)
        , m_maxRecursion(4)
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

    void appendToVector(string value)
    {
        m_order.push_back(value);
    }

    void appendToVectorReentrantTask()
    {
        m_reentrantOrder.push_back(m_reentrantCount++);

        if (m_reentrantCount > m_maxRecursion)
            return;
        Scheduler::shared()->postTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVectorReentrantTask, this));
    }

    void appendToVectorReentrantInputTask()
    {
        m_reentrantOrder.push_back(m_reentrantCount++);

        if (m_reentrantCount > m_maxRecursion)
            return;
        m_scheduler->postInputTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVectorReentrantInputTask, this));
    }

    void appendToVectorReentrantCompositorTask()
    {
        m_reentrantOrder.push_back(m_reentrantCount++);

        if (m_reentrantCount > m_maxRecursion)
            return;
        m_scheduler->postCompositorTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVectorReentrantCompositorTask, this));
    }

protected:
    SchedulerTestingPlatformSupport m_platformSupport;
    Scheduler* m_scheduler;
    std::vector<string> m_order;
    std::vector<int> m_reentrantOrder;
    int m_reentrantCount;
    int m_maxRecursion;
};

void orderedTestTask(int value, int* result)
{
    *result = (*result << 4) | value;
}

void unorderedTestTask(int value, int* result)
{
    *result += value;
}

void idleTestTask(int value, int* result, double allottedTime)
{
    *result += value;
}

TEST_F(SchedulerTest, TestPostTask)
{
    int result = 0;
    m_scheduler->postTask(FROM_HERE, WTF::bind(&orderedTestTask, 1, &result));
    m_scheduler->postTask(FROM_HERE, WTF::bind(&orderedTestTask, 2, &result));
    m_scheduler->postTask(FROM_HERE, WTF::bind(&orderedTestTask, 3, &result));
    m_scheduler->postTask(FROM_HERE, WTF::bind(&orderedTestTask, 4, &result));
    runPendingTasks();
    EXPECT_EQ(0x1234, result);
}

TEST_F(SchedulerTest, TestPostMixedTaskTypes)
{
    int result = 0;
    m_scheduler->postTask(FROM_HERE, WTF::bind(&unorderedTestTask, 1, &result));
    m_scheduler->postInputTask(FROM_HERE, WTF::bind(&unorderedTestTask, 2, &result));
    m_scheduler->postCompositorTask(FROM_HERE, WTF::bind(&unorderedTestTask, 4, &result));
    m_scheduler->postTask(FROM_HERE, WTF::bind(&unorderedTestTask, 8, &result));
    runPendingTasks();
    EXPECT_EQ(15, result);
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

TEST_F(SchedulerTest, TestIdleTask)
{
    // TODO: Check task allottedTime when implemented in the scheduler.
    int result = 0;
    m_scheduler->postIdleTask(FROM_HERE, WTF::bind<double>(&idleTestTask, 1, &result));
    m_scheduler->postIdleTask(FROM_HERE, WTF::bind<double>(&idleTestTask, 1, &result));
    m_scheduler->postIdleTask(FROM_HERE, WTF::bind<double>(&idleTestTask, 1, &result));
    m_scheduler->postIdleTask(FROM_HERE, WTF::bind<double>(&idleTestTask, 1, &result));
    runPendingTasks();
    EXPECT_EQ(4, result);
}

TEST_F(SchedulerTest, TestTaskPrioritization)
{
    m_scheduler->postTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVector, this, string("L1")));
    m_scheduler->postTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVector, this, string("L2")));
    m_scheduler->postInputTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVector, this, string("I1")));
    m_scheduler->postInputTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVector, this, string("I2")));
    m_scheduler->postCompositorTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVector, this, string("C1")));
    m_scheduler->postCompositorTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVector, this, string("C2")));

    runPendingTasks();
    EXPECT_THAT(m_order, testing::ElementsAre(
        string("I1"), string("I2"), string("C1"), string("C2"), string("L1"), string("L2")));
}

TEST_F(SchedulerTest, TestRentrantTask)
{
    m_scheduler->postTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVectorReentrantTask, this));
    runPendingTasks();

    EXPECT_THAT(m_reentrantOrder, testing::ElementsAre(0, 1, 2, 3, 4));
}


TEST_F(SchedulerTest, TestRentrantInputTaskDuringShutdown)
{
    m_scheduler->postInputTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVectorReentrantInputTask, this));
    Scheduler::shutdown();

    EXPECT_THAT(m_reentrantOrder, testing::ElementsAre(0, 1, 2, 3, 4));
}

TEST_F(SchedulerTest, TestRentrantCompositorTaskDuringShutdown)
{
    m_scheduler->postCompositorTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVectorReentrantCompositorTask, this));
    Scheduler::shutdown();

    EXPECT_THAT(m_reentrantOrder, testing::ElementsAre(0, 1, 2, 3, 4));
}

bool s_shouldContinue;
void reentrantInputTask(Scheduler* scheduler)
{
    if (s_shouldContinue)
        scheduler->postInputTask(FROM_HERE, WTF::bind(&reentrantInputTask, scheduler));
}

void reentrantCompositorTask(Scheduler* scheduler)
{
    if (s_shouldContinue)
        scheduler->postCompositorTask(FROM_HERE, WTF::bind(&reentrantCompositorTask, scheduler));
}

void stopReentrantTask()
{
    s_shouldContinue = false;
}

TEST_F(SchedulerTest, TestRentrantInputTaskDoesNotStarveOutLowPriorityTask)
{
    s_shouldContinue = true;
    m_scheduler->postInputTask(FROM_HERE, WTF::bind(&reentrantInputTask, m_scheduler));
    m_scheduler->postTask(FROM_HERE, WTF::bind(&stopReentrantTask));

    // If starvation occurs then this will never exit.
    runPendingTasks();
}

TEST_F(SchedulerTest, TestRentrantCompositorTaskDoesNotStarveOutLowPriorityTask)
{
    s_shouldContinue = true;
    m_scheduler->postInputTask(FROM_HERE, WTF::bind(&reentrantCompositorTask, m_scheduler));
    m_scheduler->postTask(FROM_HERE, WTF::bind(&stopReentrantTask));

    // If starvation occurs then this will never exit.
    runPendingTasks();
}

} // namespace
