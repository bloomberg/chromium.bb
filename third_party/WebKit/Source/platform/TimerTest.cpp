// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/Timer.h"

#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebThread.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <queue>

using testing::ElementsAre;

namespace blink {
namespace {
double gCurrentTimeSecs = 0.0;

double currentTime()
{
    return gCurrentTimeSecs;
}

class MockWebScheduler : public WebScheduler {
public:
    explicit MockWebScheduler() { }
    ~MockWebScheduler() override { }

    bool shouldYieldForHighPriorityWork() override
    {
        return false;
    }

    bool canExceedIdleDeadlineIfRequired() override
    {
        return false;
    }

    void postIdleTask(const WebTraceLocation&, WebThread::IdleTask*) override
    {
    }

    void postNonNestableIdleTask(const WebTraceLocation&, WebThread::IdleTask*) override
    {
    }

    void postIdleTaskAfterWakeup(const WebTraceLocation&, WebThread::IdleTask*) override
    {
    }

    void postLoadingTask(const WebTraceLocation&, WebThread::Task*) override
    {
    }

    void postTimerTask(const WebTraceLocation&, WebThread::Task* task, long long delayMs) override
    {
    }
};

class FakeWebThread : public WebThread {
public:
    explicit FakeWebThread(WebScheduler* webScheduler) : m_webScheduler(webScheduler) { }
    ~FakeWebThread() override { }

    // WebThread implementation:
    void postTask(const WebTraceLocation&, Task*)
    {
        ASSERT_NOT_REACHED();
    }

    virtual void postDelayedTask(const WebTraceLocation&, Task*, long long)
    {
        ASSERT_NOT_REACHED();
    }

    virtual bool isCurrentThread() const
    {
        ASSERT_NOT_REACHED();
        return true;
    }

    virtual PlatformThreadId threadId() const
    {
        ASSERT_NOT_REACHED();
        return 0;
    }

    WebScheduler* scheduler() const override
    {
        return m_webScheduler;
    }

    virtual void enterRunLoop()
    {
        ASSERT_NOT_REACHED();
    }

    virtual void exitRunLoop()
    {
        ASSERT_NOT_REACHED();
    }

private:
    WebScheduler* m_webScheduler;
};

class TimerTestPlatform : public Platform {
public:
    explicit TimerTestPlatform(WebThread* webThread)
        : m_webThread(webThread)
        , m_timerInterval(-1) { }
    ~TimerTestPlatform() override { }

    WebThread* currentThread() override
    {
        return m_webThread;
    }

    void cryptographicallyRandomValues(unsigned char*, size_t) override
    {
        ASSERT_NOT_REACHED();
    }

    const unsigned char* getTraceCategoryEnabledFlag(const char* categoryName) override
    {
        static const unsigned char enabled[] = {0};
        return enabled;
    }

    void setSharedTimerFiredFunction(SharedTimerFunction timerFunction) override
    {
        s_timerFunction = timerFunction;
    }

    void setSharedTimerFireInterval(double interval) override
    {
        m_timerInterval = interval;
    }

    virtual void stopSharedTimer() override
    {
        m_timerInterval = -1;
    }

    void runUntilIdle()
    {
        while (hasOneTimerTask()) {
            gCurrentTimeSecs += m_timerInterval;
            s_timerFunction();
        }
    }

    void runUntilIdleOrDeadlinePassed(double deadline)
    {
        while (hasOneTimerTask()) {
            double newTime = gCurrentTimeSecs + m_timerInterval;
            if (newTime >= deadline) {
                gCurrentTimeSecs = deadline;
                break;
            }
            gCurrentTimeSecs = newTime;
            s_timerFunction();
        }
    }

    bool hasOneTimerTask() const
    {
        return s_timerFunction && m_timerInterval >= 0;
    }

    long nextTimerTaskDelayMillis() const
    {
        ASSERT(hasOneTimerTask());
        return static_cast<long>(m_timerInterval * 1000);
    }

private:
    WebThread* m_webThread;
    double m_timerInterval;

    // This needs to be static because the callback is registered only once by
    // PlatformThreadData.
    static SharedTimerFunction s_timerFunction;
};

Platform::SharedTimerFunction TimerTestPlatform::s_timerFunction;

class TimerTest : public testing::Test {
public:
    void SetUp() override
    {
        m_mockWebScheduler = adoptPtr(new MockWebScheduler());
        m_fakeWebThread = adoptPtr(new FakeWebThread(m_mockWebScheduler.get()));
        m_platform = adoptPtr(new TimerTestPlatform(m_fakeWebThread.get()));
        m_oldPlatform = Platform::current();
        Platform::initialize(m_platform.get());
        WTF::setMonotonicallyIncreasingTimeFunction(currentTime);

        m_runTimes.clear();
        gCurrentTimeSecs = 10.0;
        m_startTime = gCurrentTimeSecs;
    }

    void TearDown() override
    {
        Platform::initialize(m_oldPlatform);
    }

    void countingTask(Timer<TimerTest>*)
    {
        m_runTimes.push_back(monotonicallyIncreasingTime());
    }

    void advanceTimeBy(double timeSecs)
    {
        gCurrentTimeSecs += timeSecs;
    }

    void runUntilIdle()
    {
        m_platform->runUntilIdle();
    }

    void runUntilIdleOrDeadlinePassed(double deadline)
    {
        m_platform->runUntilIdleOrDeadlinePassed(deadline);
    }

    bool hasOneTimerTask() const
    {
        return m_platform->hasOneTimerTask();
    }

    long nextTimerTaskDelayMillis() const
    {
        return m_platform->nextTimerTaskDelayMillis();
    }

protected:
    double m_startTime;
    std::vector<double> m_runTimes;

private:
    OwnPtr<MockWebScheduler> m_mockWebScheduler;
    OwnPtr<FakeWebThread> m_fakeWebThread;
    OwnPtr<TimerTestPlatform> m_platform;
    Platform* m_oldPlatform;
};

TEST_F(TimerTest, StartOneShot_Zero)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(0, FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_EQ(0ll, nextTimerTaskDelayMillis());

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime));
}

TEST_F(TimerTest, StartOneShot_ZeroAndCancel)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(0, FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_EQ(0ll, nextTimerTaskDelayMillis());

    timer.stop();

    runUntilIdle();
    EXPECT_TRUE(m_runTimes.empty());
}

TEST_F(TimerTest, StartOneShot_ZeroAndCancelThenRepost)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(0, FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_EQ(0ll, nextTimerTaskDelayMillis());

    timer.stop();

    runUntilIdle();
    EXPECT_TRUE(m_runTimes.empty());

    timer.startOneShot(0, FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_EQ(0ll, nextTimerTaskDelayMillis());

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime));
}

TEST_F(TimerTest, StartOneShot_Zero_RepostingAfterRunning)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(0, FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_EQ(0ll, nextTimerTaskDelayMillis());

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime));

    timer.startOneShot(0, FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_EQ(0ll, nextTimerTaskDelayMillis());

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime, m_startTime));
}

TEST_F(TimerTest, StartOneShot_NonZero)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10.0, FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_EQ(10000ll, nextTimerTaskDelayMillis());

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 10.0));
}

TEST_F(TimerTest, StartOneShot_NonZeroAndCancel)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10, FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_EQ(10000ll, nextTimerTaskDelayMillis());

    timer.stop();

    runUntilIdle();
    EXPECT_TRUE(m_runTimes.empty());
}

TEST_F(TimerTest, StartOneShot_NonZeroAndCancelThenRepost)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10, FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_EQ(10000ll, nextTimerTaskDelayMillis());

    timer.stop();

    runUntilIdle();
    EXPECT_TRUE(m_runTimes.empty());

    double secondPostTime = monotonicallyIncreasingTime();
    timer.startOneShot(10, FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_EQ(10000ll, nextTimerTaskDelayMillis());

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(secondPostTime + 10.0));
}

TEST_F(TimerTest, StartOneShot_NonZero_RepostingAfterRunning)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10, FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_EQ(10000ll, nextTimerTaskDelayMillis());

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 10.0));

    timer.startOneShot(20, FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_EQ(20000ll, nextTimerTaskDelayMillis());

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 10.0, m_startTime + 30.0));
}

TEST_F(TimerTest, PostingTimerTwiceWithSameRunTimeDoesNothing)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10, FROM_HERE);
    timer.startOneShot(10, FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_EQ(10000ll, nextTimerTaskDelayMillis());

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 10.0));
}

TEST_F(TimerTest, PostingTimerTwiceWithNewerRunTimeCancelsOriginalTask)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10, FROM_HERE);
    timer.startOneShot(0, FROM_HERE);

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 0.0));
}

TEST_F(TimerTest, PostingTimerTwiceWithLaterRunTimeCancelsOriginalTask)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(0, FROM_HERE);
    timer.startOneShot(10, FROM_HERE);

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 10.0));
}

TEST_F(TimerTest, StartRepeatingTask)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startRepeating(1.0, FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_EQ(1000ll, nextTimerTaskDelayMillis());

    runUntilIdleOrDeadlinePassed(m_startTime + 5.5);
    EXPECT_THAT(m_runTimes, ElementsAre(
        m_startTime + 1.0, m_startTime + 2.0, m_startTime + 3.0, m_startTime + 4.0, m_startTime + 5.0));
}

TEST_F(TimerTest, StartRepeatingTask_ThenCancel)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startRepeating(1.0, FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_EQ(1000ll, nextTimerTaskDelayMillis());

    runUntilIdleOrDeadlinePassed(m_startTime + 2.5);
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 1.0, m_startTime + 2.0));

    timer.stop();
    runUntilIdle();

    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 1.0, m_startTime + 2.0));
}

TEST_F(TimerTest, StartRepeatingTask_ThenPostOneShot)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startRepeating(1.0, FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_EQ(1000ll, nextTimerTaskDelayMillis());

    runUntilIdleOrDeadlinePassed(m_startTime + 2.5);
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 1.0, m_startTime + 2.0));

    timer.startOneShot(0, FROM_HERE);
    runUntilIdle();

    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 1.0, m_startTime + 2.0, m_startTime + 2.5));
}

TEST_F(TimerTest, IsActive_NeverPosted)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);

    EXPECT_FALSE(timer.isActive());
}

TEST_F(TimerTest, IsActive_AfterPosting_OneShotZero)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(0, FROM_HERE);

    EXPECT_TRUE(timer.isActive());
}

TEST_F(TimerTest, IsActive_AfterPosting_OneShotNonZero)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10, FROM_HERE);

    EXPECT_TRUE(timer.isActive());
}

TEST_F(TimerTest, IsActive_AfterPosting_Repeating)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startRepeating(1.0, FROM_HERE);

    EXPECT_TRUE(timer.isActive());
}

TEST_F(TimerTest, IsActive_AfterRunning_OneShotZero)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(0, FROM_HERE);

    runUntilIdle();
    EXPECT_FALSE(timer.isActive());
}

TEST_F(TimerTest, IsActive_AfterRunning_OneShotNonZero)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10, FROM_HERE);

    runUntilIdle();
    EXPECT_FALSE(timer.isActive());
}

TEST_F(TimerTest, IsActive_AfterRunning_Repeating)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startRepeating(1.0, FROM_HERE);

    runUntilIdleOrDeadlinePassed(m_startTime + 10);
    EXPECT_TRUE(timer.isActive()); // It should run until cancelled.
}

TEST_F(TimerTest, NextFireInterval_OneShotZero)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(0, FROM_HERE);

    EXPECT_FLOAT_EQ(0.0, timer.nextFireInterval());
}

TEST_F(TimerTest, NextFireInterval_OneShotNonZero)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10, FROM_HERE);

    EXPECT_FLOAT_EQ(10.0, timer.nextFireInterval());
}

TEST_F(TimerTest, NextFireInterval_OneShotNonZero_AfterAFewSeconds)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10, FROM_HERE);

    advanceTimeBy(2.0);
    EXPECT_FLOAT_EQ(8.0, timer.nextFireInterval());
}

TEST_F(TimerTest, NextFireInterval_Repeating)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startRepeating(20, FROM_HERE);

    EXPECT_FLOAT_EQ(20.0, timer.nextFireInterval());
}

TEST_F(TimerTest, RepeatInterval_NeverStarted)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);

    EXPECT_FLOAT_EQ(0.0, timer.repeatInterval());
}

TEST_F(TimerTest, RepeatInterval_OneShotZero)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(0, FROM_HERE);

    EXPECT_FLOAT_EQ(0.0, timer.repeatInterval());
}

TEST_F(TimerTest, RepeatInterval_OneShotNonZero)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10, FROM_HERE);

    EXPECT_FLOAT_EQ(0.0, timer.repeatInterval());
}

TEST_F(TimerTest, RepeatInterval_Repeating)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startRepeating(20, FROM_HERE);

    EXPECT_FLOAT_EQ(20.0, timer.repeatInterval());
}

TEST_F(TimerTest, AugmentRepeatInterval)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startRepeating(10, FROM_HERE);
    EXPECT_FLOAT_EQ(10.0, timer.repeatInterval());
    EXPECT_FLOAT_EQ(10.0, timer.nextFireInterval());

    advanceTimeBy(2.0);
    timer.augmentRepeatInterval(10);

    EXPECT_FLOAT_EQ(20.0, timer.repeatInterval());
    EXPECT_FLOAT_EQ(18.0, timer.nextFireInterval());

    runUntilIdleOrDeadlinePassed(m_startTime + 50.0);
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 20.0, m_startTime + 40.0));
}

class MockTimerWithAlignment : public TimerBase {
public:
    MockTimerWithAlignment() : m_lastFireTime(0.0), m_alignedFireTime(0.0) { }

    virtual void fired() override
    {
    }

    double alignedFireTime(double fireTime) const override
    {
        m_lastFireTime = fireTime;
        return m_alignedFireTime;
    }

    void setAlignedFireTime(double alignedFireTime)
    {
        m_alignedFireTime = alignedFireTime;
    }

    double lastFireTime() const
    {
        return m_lastFireTime;
    }

private:
    mutable double m_lastFireTime;
    double m_alignedFireTime;
};

TEST_F(TimerTest, TimerAlignment_OneShotZero)
{
    MockTimerWithAlignment timer;
    timer.setAlignedFireTime(m_startTime + 1.0);

    timer.start(0.0, 0.0, FROM_HERE);

    // The nextFireInterval gets overrriden.
    EXPECT_FLOAT_EQ(1.0, timer.nextFireInterval());
    EXPECT_FLOAT_EQ(0.0, timer.nextUnalignedFireInterval());
    EXPECT_FLOAT_EQ(m_startTime, timer.lastFireTime());
}

TEST_F(TimerTest, TimerAlignment_OneShotNonZero)
{
    MockTimerWithAlignment timer;
    timer.setAlignedFireTime(m_startTime + 1.0);

    timer.start(0.5, 0.0, FROM_HERE);

    // The nextFireInterval gets overrriden.
    EXPECT_FLOAT_EQ(1.0, timer.nextFireInterval());
    EXPECT_FLOAT_EQ(0.5, timer.nextUnalignedFireInterval());
    EXPECT_FLOAT_EQ(m_startTime + 0.5, timer.lastFireTime());
}

TEST_F(TimerTest, DidChangeAlignmentInterval)
{
    MockTimerWithAlignment timer;
    timer.setAlignedFireTime(m_startTime + 1.0);

    timer.start(0.0, 0.0, FROM_HERE);

    EXPECT_FLOAT_EQ(1.0, timer.nextFireInterval());
    EXPECT_FLOAT_EQ(0.0, timer.nextUnalignedFireInterval());
    EXPECT_FLOAT_EQ(m_startTime, timer.lastFireTime());

    timer.setAlignedFireTime(m_startTime);
    timer.didChangeAlignmentInterval();

    EXPECT_FLOAT_EQ(0.0, timer.nextFireInterval());
    EXPECT_FLOAT_EQ(0.0, timer.nextUnalignedFireInterval());
    EXPECT_FLOAT_EQ(m_startTime, timer.lastFireTime());
}


} // namespace
} // namespace blink
