// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/scheduler/Scheduler.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TestingPlatformSupport.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebTraceLocation.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using blink::Scheduler;
using blink::WebScheduler;
using blink::WebTraceLocation;

namespace {

class SchedulerForTest : public Scheduler {
public:
    SchedulerForTest(WebScheduler* webScheduler):
        Scheduler(webScheduler)
    {
    }
};

class WebSchedulerForTest : public WebScheduler {
public:
    WebSchedulerForTest()
        : m_shouldYieldForHighPriorityWork(false)
    {
    }

    // WebScheduler implementation:
    bool shouldYieldForHighPriorityWork() override
    {
        return m_shouldYieldForHighPriorityWork;
    }

    void postIdleTask(const WebTraceLocation&, IdleTask* task) override
    {
        m_latestIdleTask = adoptPtr(task);
    }

    void setShouldYieldForHighPriorityWork(bool shouldYieldForHighPriorityWork)
    {
        m_shouldYieldForHighPriorityWork = shouldYieldForHighPriorityWork;
    }

    void runLatestIdleTask(double deadlineSeconds)
    {
        m_latestIdleTask->run(deadlineSeconds);
        m_latestIdleTask.clear();
    }

protected:
    bool m_shouldYieldForHighPriorityWork;

    OwnPtr<WebScheduler::IdleTask> m_latestIdleTask;
};

class SchedulerTest : public testing::Test {
public:
    SchedulerTest()
    {
        blink::RuntimeEnabledFeatures::setBlinkSchedulerEnabled(true);
        m_webScheduler = adoptPtr(new WebSchedulerForTest());
        m_scheduler = adoptPtr(new SchedulerForTest(m_webScheduler.get()));
    }

protected:
    OwnPtr<WebSchedulerForTest> m_webScheduler;
    OwnPtr<SchedulerForTest> m_scheduler;
};

TEST_F(SchedulerTest, TestShouldYield)
{
    EXPECT_FALSE(m_scheduler->shouldYieldForHighPriorityWork());
    m_webScheduler->setShouldYieldForHighPriorityWork(true);
    EXPECT_TRUE(m_scheduler->shouldYieldForHighPriorityWork());
}

void idleTestTask(double expectedDeadline, double deadlineSeconds)
{
    EXPECT_EQ(expectedDeadline, deadlineSeconds);
}

TEST_F(SchedulerTest, TestIdleTasks)
{
    double deadline = 1.1;
    m_scheduler->postIdleTask(FROM_HERE, WTF::bind<double>(&idleTestTask, deadline));
    m_webScheduler->runLatestIdleTask(deadline);
}

} // namespace
