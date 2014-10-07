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

namespace {

class SchedulerForTest : public blink::Scheduler {
public:
    static void initializeOnMainThread()
    {
        s_sharedScheduler = new SchedulerForTest();
    }

    using Scheduler::Normal;
    using Scheduler::CompositorPriority;
    using Scheduler::enterSchedulerPolicy;
};

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

    virtual blink::PlatformThreadId threadId() const OVERRIDE
    {
        ASSERT_NOT_REACHED();
        return 0;
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

    size_t numPendingMainThreadTasks() const
    {
        return m_pendingTasks.size();
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
        , m_monotonicallyIncreasingTime(0)
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

    virtual double monotonicallyIncreasingTime() OVERRIDE
    {
        return m_monotonicallyIncreasingTime;
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

    size_t numPendingMainThreadTasks() const
    {
        return m_mainThread.numPendingMainThreadTasks();
    }

    void setMonotonicTimeForTest(double time)
    {
        m_monotonicallyIncreasingTime = time;
    }

private:
    TestMainThread m_mainThread;
    SharedTimerFunction m_sharedTimerFunction;
    bool m_sharedTimerRunning;
    double m_sharedTimerFireInterval;
    double m_monotonicallyIncreasingTime;
};

class SchedulerTest : public testing::Test {
public:
    SchedulerTest()
        : m_reentrantCount(0)
        , m_maxRecursion(4)
    {
        SchedulerForTest::initializeOnMainThread();
        m_scheduler = static_cast<SchedulerForTest*>(Scheduler::shared());
    }

    ~SchedulerTest()
    {
        Scheduler::shutdown();
    }

    virtual void SetUp() OVERRIDE
    {
        m_scheduler->enterSchedulerPolicy(SchedulerForTest::Normal);
    }

    virtual void TearDown() OVERRIDE
    {
        // If the Scheduler hasn't been shut down then explicitly flush the tasks.
        if (Scheduler::shared())
            runPendingTasks();
    }

    void runPendingTasks()
    {
        m_platformSupport.runPendingTasks();
    }

    void enableIdleTasks()
    {
        m_platformSupport.setMonotonicTimeForTest(0);
        m_scheduler->willBeginFrame(1);
        m_scheduler->didCommitFrameToCompositor();
    }

    void appendToVector(std::string value)
    {
        m_order.push_back(value);
    }

    void appendToVectorIdleTask(std::string value, double deadline)
    {
        appendToVector(value);
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
    SchedulerForTest* m_scheduler;
    std::vector<std::string> m_order;
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

void idleTestTask(bool* taskRun, double expectedDeadline, double deadlineSeconds)
{
    EXPECT_FALSE(*taskRun);
    EXPECT_EQ(expectedDeadline, deadlineSeconds);
    *taskRun = true;
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
    m_scheduler->postIpcTask(FROM_HERE, WTF::bind(&unorderedTestTask, 16, &result));
    runPendingTasks();
    EXPECT_EQ(31, result);
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
    bool taskRun = false;
    double firstDeadline = 1.1;
    double secondDeadline = 2.3;
    m_platformSupport.setMonotonicTimeForTest(0.1);

    m_scheduler->postIdleTask(FROM_HERE, WTF::bind<double>(&idleTestTask, &taskRun, secondDeadline));

    runPendingTasks();
    EXPECT_FALSE(taskRun); // Shouldn't run yet as no willBeginFrame.

    m_scheduler->willBeginFrame(firstDeadline);
    runPendingTasks();
    EXPECT_FALSE(taskRun); // Shouldn't run yet as no didCommitFrameToCompositor.

    m_platformSupport.setMonotonicTimeForTest(firstDeadline + 0.1);
    m_scheduler->didCommitFrameToCompositor();
    runPendingTasks();
    EXPECT_FALSE(taskRun); // We missed the deadline.

    m_scheduler->willBeginFrame(secondDeadline);
    m_platformSupport.setMonotonicTimeForTest(secondDeadline - 0.1);
    m_scheduler->didCommitFrameToCompositor();
    runPendingTasks();
    EXPECT_TRUE(taskRun);
}

TEST_F(SchedulerTest, TestTaskPrioritization_normalPolicy)
{
    m_scheduler->postTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVector, this, std::string("L1")));
    m_scheduler->postTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVector, this, std::string("L2")));
    m_scheduler->postInputTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVector, this, std::string("I1")));
    m_scheduler->postCompositorTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVector, this, std::string("C1")));
    m_scheduler->postInputTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVector, this, std::string("I2")));
    m_scheduler->postCompositorTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVector, this, std::string("C2")));
    m_scheduler->postIpcTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVector, this, std::string("IPC")));

    runPendingTasks();
    EXPECT_THAT(m_order, testing::ElementsAre(
        std::string("L1"), std::string("L2"), std::string("I1"), std::string("C1"), std::string("I2"), std::string("C2"),
        std::string("IPC")));
}

TEST_F(SchedulerTest, TestRentrantTask)
{
    m_scheduler->postTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVectorReentrantTask, this));
    runPendingTasks();

    EXPECT_THAT(m_reentrantOrder, testing::ElementsAre(0, 1, 2, 3, 4));
}

TEST_F(SchedulerTest, TestTasksRunAfterShutdown)
{
    m_scheduler->postTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVector, this, std::string("1")));
    m_scheduler->postInputTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVector, this, std::string("2")));
    m_scheduler->postCompositorTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVector, this, std::string("3")));
    m_scheduler->postIpcTask(FROM_HERE, WTF::bind(&SchedulerTest::appendToVector, this, std::string("4")));
    // Idle task should not be run if scheduler is shutdown, but should not crash when flushed.
    m_scheduler->postIdleTask(FROM_HERE, WTF::bind<double>(&SchedulerTest::appendToVectorIdleTask, this, std::string("Not Run")));
    enableIdleTasks();

    Scheduler::shutdown();
    EXPECT_TRUE(m_order.empty());

    runPendingTasks();
    EXPECT_THAT(m_order, testing::ElementsAre(
        std::string("1"), std::string("2"), std::string("3"), std::string("4")));
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

int s_dummyTaskCount;
void dummyTask()
{
    s_dummyTaskCount++;
}

TEST_F(SchedulerTest, TestMainThreadTaskLifeCycle)
{
    EXPECT_EQ(0U, m_platformSupport.numPendingMainThreadTasks());

    m_scheduler->postInputTask(FROM_HERE, WTF::bind(&dummyTask));
    EXPECT_EQ(1U, m_platformSupport.numPendingMainThreadTasks());

    runPendingTasks();
    EXPECT_EQ(0U, m_platformSupport.numPendingMainThreadTasks());

    m_scheduler->postCompositorTask(FROM_HERE, WTF::bind(&dummyTask));
    EXPECT_EQ(1U, m_platformSupport.numPendingMainThreadTasks());

    runPendingTasks();
    EXPECT_EQ(0U, m_platformSupport.numPendingMainThreadTasks());
}

void postDummyInputTask()
{
    Scheduler::shared()->postInputTask(FROM_HERE, WTF::bind(&dummyTask));
}

TEST_F(SchedulerTest, HighPriorityTasksOnlyDontRunBecauseOfSharedTimerFiring_InNormalMode)
{
    s_dummyTaskCount = 0;
    m_scheduler->postInputTask(FROM_HERE, WTF::bind(&dummyTask));
    // Trigger the posting of an input task during execution of the shared timer function.
    m_scheduler->setSharedTimerFiredFunction(&postDummyInputTask);
    m_scheduler->setSharedTimerFireInterval(0);
    m_platformSupport.triggerSharedTimer();

    EXPECT_EQ(0, s_dummyTaskCount);

    // Clean up.
    m_scheduler->stopSharedTimer();
    m_scheduler->setSharedTimerFiredFunction(nullptr);
}

TEST_F(SchedulerTest, TestInputEventDoesNotTriggerShouldYield_InNormalMode)
{
    m_scheduler->postInputTask(FROM_HERE, WTF::bind(&dummyTask));

    EXPECT_FALSE(m_scheduler->shouldYieldForHighPriorityWork());
}

TEST_F(SchedulerTest, TestDidReceiveInputEventDoesNotTriggerShouldYield)
{
    m_scheduler->didReceiveInputEvent();

    EXPECT_FALSE(m_scheduler->shouldYieldForHighPriorityWork());
}

TEST_F(SchedulerTest, TestCompositorTaskDoesNotTriggerShouldYield_InNormalMode)
{
    m_scheduler->postCompositorTask(FROM_HERE, WTF::bind(&dummyTask));

    EXPECT_FALSE(m_scheduler->shouldYieldForHighPriorityWork());
}

TEST_F(SchedulerTest, TestIpcTaskDoesNotTriggerShouldYield_InNormalMode)
{
    m_scheduler->postIpcTask(FROM_HERE, WTF::bind(&dummyTask));

    EXPECT_FALSE(m_scheduler->shouldYieldForHighPriorityWork());
}

TEST_F(SchedulerTest, TestCompositorTaskDoesTriggerShouldYieldAfterDidReceiveInputEvent)
{
    m_scheduler->didReceiveInputEvent();

    ASSERT_FALSE(m_scheduler->shouldYieldForHighPriorityWork());
    m_scheduler->postCompositorTask(FROM_HERE, WTF::bind(&dummyTask));

    EXPECT_TRUE(m_scheduler->shouldYieldForHighPriorityWork());
}

TEST_F(SchedulerTest, TestInputTaskDoesTriggerShouldYield_InCompositorPriorityMode)
{
    m_scheduler->enterSchedulerPolicy(SchedulerForTest::CompositorPriority);
    m_scheduler->postInputTask(FROM_HERE, WTF::bind(&dummyTask));

    EXPECT_TRUE(m_scheduler->shouldYieldForHighPriorityWork());
}


TEST_F(SchedulerTest, TestCompositorTaskDoesTriggerShouldYield_InCompositorPriorityMode)
{
    m_scheduler->enterSchedulerPolicy(SchedulerForTest::CompositorPriority);
    m_scheduler->postCompositorTask(FROM_HERE, WTF::bind(&dummyTask));

    EXPECT_TRUE(m_scheduler->shouldYieldForHighPriorityWork());
}

TEST_F(SchedulerTest, TestIpcTaskDoesNotTriggerShouldYield_InCompositorPriorityMode)
{
    m_scheduler->enterSchedulerPolicy(SchedulerForTest::CompositorPriority);
    m_scheduler->postIpcTask(FROM_HERE, WTF::bind(&dummyTask));

    EXPECT_FALSE(m_scheduler->shouldYieldForHighPriorityWork());
}

TEST_F(SchedulerTest, testDidReceiveInputEvent_DoesntTriggerLowLatencyModeForLong)
{
    m_platformSupport.setMonotonicTimeForTest(1000.0);

    // Note the latency mode gets reset by executeHighPriorityTasks, so we need a dummy task here
    // to make sure runPendingTasks triggers executeHighPriorityTasks.
    m_scheduler->postCompositorTask(FROM_HERE, WTF::bind(&dummyTask));
    m_scheduler->didReceiveInputEvent();
    m_platformSupport.setMonotonicTimeForTest(1000.5);
    runPendingTasks();

    ASSERT_FALSE(m_scheduler->shouldYieldForHighPriorityWork());
    m_scheduler->postCompositorTask(FROM_HERE, WTF::bind(&dummyTask));

    EXPECT_FALSE(m_scheduler->shouldYieldForHighPriorityWork());
}

} // namespace
