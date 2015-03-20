// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/ScriptRunner.h"

#include "core/dom/Document.h"
#include "core/dom/ScriptLoader.h"
#include "platform/heap/Handle.h"
#include "platform/scheduler/Scheduler.h"
#include "public/platform/Platform.h"
#include "wtf/PassOwnPtr.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::Invoke;
using ::testing::ElementsAre;
using ::testing::Return;

namespace blink {

class MockScriptLoader: public ScriptLoader {
public:
    explicit MockScriptLoader(Element* element) : ScriptLoader(element, false, false) { }

    ~MockScriptLoader() override { }

    MOCK_METHOD0(execute, void());
    MOCK_CONST_METHOD0(isReady, bool());
};

class MockPlatform : public Platform, private WebScheduler {
public:
    MockPlatform() : m_shouldYield(false), m_shouldYieldEveryOtherTime(false) { }

    WebScheduler* scheduler() override
    {
        return this;
    }

    void postLoadingTask(const WebTraceLocation&, WebThread::Task* task) override
    {
        m_tasks.append(adoptPtr(task));
    }

    void cryptographicallyRandomValues(unsigned char* buffer, size_t length) override { }

    void runSingleTask()
    {
        if (m_tasks.isEmpty())
            return;
        m_tasks.takeFirst()->run();
    }

    void runAllTasks()
    {
        while (!m_tasks.isEmpty())
            m_tasks.takeFirst()->run();
    }

    bool shouldYieldForHighPriorityWork() override
    {
        if (m_shouldYieldEveryOtherTime)
            m_shouldYield = !m_shouldYield;
        return m_shouldYield;
    }

    void setShouldYield(bool shouldYield)
    {
        m_shouldYield = shouldYield;
    }

    // NOTE if we yield 100% of the time, nothing will get run.
    void setShouldYieldEveryOtherTime(bool shouldYieldEveryOtherTime)
    {
        m_shouldYieldEveryOtherTime = shouldYieldEveryOtherTime;
    }

private:
    Deque<OwnPtr<WebThread::Task>> m_tasks;
    bool m_shouldYield;
    bool m_shouldYieldEveryOtherTime;
};

class ScriptRunnerTest : public testing::Test {
public:
    void SetUp() override
    {
        m_document = Document::create();
        m_element = m_document->createElement("foo", ASSERT_NO_EXCEPTION);

        m_scriptRunner = ScriptRunner::create(m_document.get());
        m_oldPlatform = Platform::current();
        m_oldScheduler = Scheduler::shared();

        // Force Platform::initialize to create a new one pointing at MockPlatform.
        Scheduler::setForTesting(nullptr);
        Platform::initialize(&m_platform);
        m_platform.setShouldYield(false);
        m_platform.setShouldYieldEveryOtherTime(false);
    }

    void TearDown() override
    {
        m_scriptRunner.release();
        Scheduler::shutdown();
        Scheduler::setForTesting(m_oldScheduler);
        Platform::initialize(m_oldPlatform);
    }

    RefPtrWillBePersistent<Document> m_document;
    RefPtrWillBePersistent<Element> m_element;
    OwnPtrWillBePersistent<ScriptRunner> m_scriptRunner;
    std::vector<int> m_order; // gmock matchers don't work nicely with WTF::Vector
    MockPlatform m_platform;
    Platform* m_oldPlatform; // NOT OWNED
    Scheduler* m_oldScheduler; // NOT OWNED
};

TEST_F(ScriptRunnerTest, QueueSingleScript_Async)
{
    MockScriptLoader scriptLoader(m_element.get());
    m_scriptRunner->queueScriptForExecution(&scriptLoader, ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->notifyScriptReady(&scriptLoader, ScriptRunner::ASYNC_EXECUTION);

    EXPECT_CALL(scriptLoader, execute());
    m_platform.runAllTasks();
}

TEST_F(ScriptRunnerTest, QueueSingleScript_InOrder)
{
    MockScriptLoader scriptLoader(m_element.get());
    m_scriptRunner->queueScriptForExecution(&scriptLoader, ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->resume();

    EXPECT_CALL(scriptLoader, isReady()).WillOnce(Return(true));
    EXPECT_CALL(scriptLoader, execute());
    m_platform.runAllTasks();
}

TEST_F(ScriptRunnerTest, QueueMultipleScripts_InOrder)
{
    MockScriptLoader scriptLoader1(m_element.get());
    MockScriptLoader scriptLoader2(m_element.get());
    MockScriptLoader scriptLoader3(m_element.get());

    m_scriptRunner->queueScriptForExecution(&scriptLoader1, ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->queueScriptForExecution(&scriptLoader2, ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->queueScriptForExecution(&scriptLoader3, ScriptRunner::IN_ORDER_EXECUTION);

    EXPECT_CALL(scriptLoader1, execute()).WillOnce(Invoke([this] {
        m_order.push_back(1);
    }));
    EXPECT_CALL(scriptLoader2, execute()).WillOnce(Invoke([this] {
        m_order.push_back(2);
    }));
    EXPECT_CALL(scriptLoader3, execute()).WillOnce(Invoke([this] {
        m_order.push_back(3);
    }));

    // Make the scripts become ready in reverse order.
    bool isReady[] = { false, false, false };
    EXPECT_CALL(scriptLoader1, isReady()).WillRepeatedly(Invoke([&isReady] {
        return isReady[0];
    }));
    EXPECT_CALL(scriptLoader2, isReady()).WillRepeatedly(Invoke([&isReady] {
        return isReady[1];
    }));
    EXPECT_CALL(scriptLoader3, isReady()).WillRepeatedly(Invoke([&isReady] {
        return isReady[2];
    }));

    for (int i = 2; i >= 0; i--) {
        isReady[i] = true;
        m_scriptRunner->resume();
        m_platform.runAllTasks();
    }

    // But ensure the scripts were run in the expected order.
    EXPECT_THAT(m_order, ElementsAre(1, 2, 3));
}

TEST_F(ScriptRunnerTest, QueueMixedScripts)
{
    MockScriptLoader scriptLoader1(m_element.get());
    MockScriptLoader scriptLoader2(m_element.get());
    MockScriptLoader scriptLoader3(m_element.get());
    MockScriptLoader scriptLoader4(m_element.get());
    MockScriptLoader scriptLoader5(m_element.get());

    EXPECT_CALL(scriptLoader1, isReady()).WillRepeatedly(Return(true));
    EXPECT_CALL(scriptLoader2, isReady()).WillRepeatedly(Return(true));
    EXPECT_CALL(scriptLoader3, isReady()).WillRepeatedly(Return(true));

    m_scriptRunner->queueScriptForExecution(&scriptLoader1, ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->queueScriptForExecution(&scriptLoader2, ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->queueScriptForExecution(&scriptLoader3, ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->queueScriptForExecution(&scriptLoader4, ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->queueScriptForExecution(&scriptLoader5, ScriptRunner::ASYNC_EXECUTION);

    m_scriptRunner->notifyScriptReady(&scriptLoader4, ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->notifyScriptReady(&scriptLoader5, ScriptRunner::ASYNC_EXECUTION);

    EXPECT_CALL(scriptLoader1, execute()).WillOnce(Invoke([this] {
        m_order.push_back(1);
    }));
    EXPECT_CALL(scriptLoader2, execute()).WillOnce(Invoke([this] {
        m_order.push_back(2);
    }));
    EXPECT_CALL(scriptLoader3, execute()).WillOnce(Invoke([this] {
        m_order.push_back(3);
    }));
    EXPECT_CALL(scriptLoader4, execute()).WillOnce(Invoke([this] {
        m_order.push_back(4);
    }));
    EXPECT_CALL(scriptLoader5, execute()).WillOnce(Invoke([this] {
        m_order.push_back(5);
    }));

    m_platform.runAllTasks();

    // Make sure the async scripts were run before the in-order ones.
    EXPECT_THAT(m_order, ElementsAre(4, 5, 1, 2, 3));
}

TEST_F(ScriptRunnerTest, QueueMixedScripts_YieldAfterEveryExecution)
{
    MockScriptLoader scriptLoader1(m_element.get());
    MockScriptLoader scriptLoader2(m_element.get());
    MockScriptLoader scriptLoader3(m_element.get());
    MockScriptLoader scriptLoader4(m_element.get());
    MockScriptLoader scriptLoader5(m_element.get());

    m_platform.setShouldYieldEveryOtherTime(true);

    EXPECT_CALL(scriptLoader1, isReady()).WillRepeatedly(Return(true));
    EXPECT_CALL(scriptLoader2, isReady()).WillRepeatedly(Return(true));
    EXPECT_CALL(scriptLoader3, isReady()).WillRepeatedly(Return(true));

    m_scriptRunner->queueScriptForExecution(&scriptLoader1, ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->queueScriptForExecution(&scriptLoader2, ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->queueScriptForExecution(&scriptLoader3, ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->queueScriptForExecution(&scriptLoader4, ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->queueScriptForExecution(&scriptLoader5, ScriptRunner::ASYNC_EXECUTION);

    m_scriptRunner->notifyScriptReady(&scriptLoader4, ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->notifyScriptReady(&scriptLoader5, ScriptRunner::ASYNC_EXECUTION);

    EXPECT_CALL(scriptLoader1, execute()).WillOnce(Invoke([this] {
        m_order.push_back(1);
    }));
    EXPECT_CALL(scriptLoader2, execute()).WillOnce(Invoke([this] {
        m_order.push_back(2);
    }));
    EXPECT_CALL(scriptLoader3, execute()).WillOnce(Invoke([this] {
        m_order.push_back(3);
    }));
    EXPECT_CALL(scriptLoader4, execute()).WillOnce(Invoke([this] {
        m_order.push_back(4);
    }));
    EXPECT_CALL(scriptLoader5, execute()).WillOnce(Invoke([this] {
        m_order.push_back(5);
    }));

    m_platform.runAllTasks();

    // Make sure the async scripts were run before the in-order ones.
    EXPECT_THAT(m_order, ElementsAre(4, 5, 1, 2, 3));
}

TEST_F(ScriptRunnerTest, QueueReentrantScript_Async)
{
    MockScriptLoader scriptLoader1(m_element.get());
    MockScriptLoader scriptLoader2(m_element.get());
    MockScriptLoader scriptLoader3(m_element.get());

    m_scriptRunner->queueScriptForExecution(&scriptLoader1, ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->queueScriptForExecution(&scriptLoader2, ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->queueScriptForExecution(&scriptLoader3, ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->notifyScriptReady(&scriptLoader1, ScriptRunner::ASYNC_EXECUTION);

    EXPECT_CALL(scriptLoader1, execute()).WillOnce(Invoke([&scriptLoader2, this] {
        m_order.push_back(1);
        m_scriptRunner->notifyScriptReady(&scriptLoader2, ScriptRunner::ASYNC_EXECUTION);
    }));

    EXPECT_CALL(scriptLoader2, execute()).WillOnce(Invoke([&scriptLoader3, this] {
        m_order.push_back(2);
        m_scriptRunner->notifyScriptReady(&scriptLoader3, ScriptRunner::ASYNC_EXECUTION);
    }));

    EXPECT_CALL(scriptLoader3, execute()).WillOnce(Invoke([this] {
        m_order.push_back(3);
    }));

    // Make sure that re-entrant calls to notifyScriptReady don't cause ScriptRunner::execute to do
    // more work than expected.
    m_platform.runSingleTask();
    EXPECT_THAT(m_order, ElementsAre(1));

    m_platform.runSingleTask();
    EXPECT_THAT(m_order, ElementsAre(1, 2));

    m_platform.runSingleTask();
    EXPECT_THAT(m_order, ElementsAre(1, 2, 3));
}

TEST_F(ScriptRunnerTest, QueueReentrantScript_InOrder)
{
    MockScriptLoader scriptLoader1(m_element.get());
    MockScriptLoader scriptLoader2(m_element.get());
    MockScriptLoader scriptLoader3(m_element.get());

    EXPECT_CALL(scriptLoader1, isReady()).WillRepeatedly(Return(true));
    EXPECT_CALL(scriptLoader2, isReady()).WillRepeatedly(Return(true));
    EXPECT_CALL(scriptLoader3, isReady()).WillRepeatedly(Return(true));

    m_scriptRunner->queueScriptForExecution(&scriptLoader1, ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->resume();

    EXPECT_CALL(scriptLoader1, execute()).WillOnce(Invoke([&scriptLoader2, this] {
        m_order.push_back(1);
        m_scriptRunner->queueScriptForExecution(&scriptLoader2, ScriptRunner::IN_ORDER_EXECUTION);
        m_scriptRunner->resume();
    }));

    EXPECT_CALL(scriptLoader2, execute()).WillOnce(Invoke([&scriptLoader3, this] {
        m_order.push_back(2);
        m_scriptRunner->queueScriptForExecution(&scriptLoader3, ScriptRunner::IN_ORDER_EXECUTION);
        m_scriptRunner->resume();
    }));

    EXPECT_CALL(scriptLoader3, execute()).WillOnce(Invoke([this] {
        m_order.push_back(3);
    }));

    // Make sure that re-entrant calls to queueScriptForExecution don't cause ScriptRunner::execute to do
    // more work than expected.
    m_platform.runSingleTask();
    EXPECT_THAT(m_order, ElementsAre(1));

    m_platform.runSingleTask();
    EXPECT_THAT(m_order, ElementsAre(1, 2));

    m_platform.runSingleTask();
    EXPECT_THAT(m_order, ElementsAre(1, 2, 3));
}

TEST_F(ScriptRunnerTest, ShouldYield_AsyncScripts)
{
    MockScriptLoader scriptLoader1(m_element.get());
    MockScriptLoader scriptLoader2(m_element.get());
    MockScriptLoader scriptLoader3(m_element.get());

    m_scriptRunner->queueScriptForExecution(&scriptLoader1, ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->queueScriptForExecution(&scriptLoader2, ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->queueScriptForExecution(&scriptLoader3, ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->notifyScriptReady(&scriptLoader1, ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->notifyScriptReady(&scriptLoader2, ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->notifyScriptReady(&scriptLoader3, ScriptRunner::ASYNC_EXECUTION);

    EXPECT_CALL(scriptLoader1, execute()).WillOnce(Invoke([this] {
        m_order.push_back(1);
        m_platform.setShouldYield(true);
    }));
    EXPECT_CALL(scriptLoader2, execute()).WillOnce(Invoke([this] {
        m_order.push_back(2);
    }));
    EXPECT_CALL(scriptLoader3, execute()).WillOnce(Invoke([this] {
        m_order.push_back(3);
    }));

    m_platform.runSingleTask();
    EXPECT_THAT(m_order, ElementsAre(1));

    // Make sure the interrupted tasks are executed next 'tick'.
    m_platform.setShouldYield(false);
    m_platform.runSingleTask();
    EXPECT_THAT(m_order, ElementsAre(1, 2, 3));
}

TEST_F(ScriptRunnerTest, QueueReentrantScript_ManyAsyncScripts)
{
    OwnPtr<MockScriptLoader> scriptLoaders[20];
    for (int i = 0; i < 20; i++) {
        scriptLoaders[i] = adoptPtr(new MockScriptLoader(m_element.get()));
        EXPECT_CALL(*scriptLoaders[i], isReady()).WillRepeatedly(Return(true));

        m_scriptRunner->queueScriptForExecution(scriptLoaders[i].get(), ScriptRunner::ASYNC_EXECUTION);

        if (i > 0) {
            EXPECT_CALL(*scriptLoaders[i], execute()).WillOnce(Invoke([this, i] {
                m_order.push_back(i);
            }));
        }
    }

    m_scriptRunner->notifyScriptReady(scriptLoaders[0].get(), ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->notifyScriptReady(scriptLoaders[1].get(), ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->resume();

    EXPECT_CALL(*scriptLoaders[0], execute()).WillOnce(Invoke([&scriptLoaders, this] {
        for (int i = 2; i < 20; i++)
            m_scriptRunner->notifyScriptReady(scriptLoaders[i].get(), ScriptRunner::ASYNC_EXECUTION);
        m_scriptRunner->resume();
        m_order.push_back(0);
    }));

    m_platform.runAllTasks();

    int expected[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19
    };

    EXPECT_THAT(m_order, testing::ElementsAreArray(expected));
}

TEST_F(ScriptRunnerTest, ShouldYield_InOrderScripts)
{
    MockScriptLoader scriptLoader1(m_element.get());
    MockScriptLoader scriptLoader2(m_element.get());
    MockScriptLoader scriptLoader3(m_element.get());

    EXPECT_CALL(scriptLoader1, isReady()).WillRepeatedly(Return(true));
    EXPECT_CALL(scriptLoader2, isReady()).WillRepeatedly(Return(true));
    EXPECT_CALL(scriptLoader3, isReady()).WillRepeatedly(Return(true));

    m_scriptRunner->queueScriptForExecution(&scriptLoader1, ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->queueScriptForExecution(&scriptLoader2, ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->queueScriptForExecution(&scriptLoader3, ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->resume();

    EXPECT_CALL(scriptLoader1, execute()).WillOnce(Invoke([this] {
        m_order.push_back(1);
        m_platform.setShouldYield(true);
    }));
    EXPECT_CALL(scriptLoader2, execute()).WillOnce(Invoke([this] {
        m_order.push_back(2);
    }));
    EXPECT_CALL(scriptLoader3, execute()).WillOnce(Invoke([this] {
        m_order.push_back(3);
    }));

    m_platform.runSingleTask();
    EXPECT_THAT(m_order, ElementsAre(1));

    // Make sure the interrupted tasks are executed next 'tick'.
    m_platform.setShouldYield(false);
    m_platform.runSingleTask();
    EXPECT_THAT(m_order, ElementsAre(1, 2, 3));
}

TEST_F(ScriptRunnerTest, ShouldYield_RunsAtLastOneTask_AsyncScripts)
{
    MockScriptLoader scriptLoader1(m_element.get());
    MockScriptLoader scriptLoader2(m_element.get());
    MockScriptLoader scriptLoader3(m_element.get());

    m_scriptRunner->queueScriptForExecution(&scriptLoader1, ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->queueScriptForExecution(&scriptLoader2, ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->queueScriptForExecution(&scriptLoader3, ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->notifyScriptReady(&scriptLoader1, ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->notifyScriptReady(&scriptLoader2, ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->notifyScriptReady(&scriptLoader3, ScriptRunner::ASYNC_EXECUTION);

    m_platform.setShouldYield(true);
    EXPECT_CALL(scriptLoader1, execute()).Times(1);
    EXPECT_CALL(scriptLoader2, execute()).Times(0);
    EXPECT_CALL(scriptLoader3, execute()).Times(0);

    m_platform.runSingleTask();

    // We can't safely distruct ScriptRunner with unexecuted MockScriptLoaders (real ScriptLoader is fine) so drain them.
    testing::Mock::VerifyAndClear(&scriptLoader2);
    testing::Mock::VerifyAndClear(&scriptLoader3);
    EXPECT_CALL(scriptLoader2, execute()).Times(1);
    EXPECT_CALL(scriptLoader3, execute()).Times(1);

    m_platform.runAllTasks();
}

TEST_F(ScriptRunnerTest, ShouldYield_RunsAtLastOneTask_InOrderScripts)
{
    MockScriptLoader scriptLoader1(m_element.get());
    MockScriptLoader scriptLoader2(m_element.get());
    MockScriptLoader scriptLoader3(m_element.get());

    EXPECT_CALL(scriptLoader1, isReady()).WillRepeatedly(Return(true));
    EXPECT_CALL(scriptLoader2, isReady()).WillRepeatedly(Return(true));
    EXPECT_CALL(scriptLoader3, isReady()).WillRepeatedly(Return(true));

    m_scriptRunner->queueScriptForExecution(&scriptLoader1, ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->queueScriptForExecution(&scriptLoader2, ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->queueScriptForExecution(&scriptLoader3, ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->resume();

    m_platform.setShouldYield(true);
    EXPECT_CALL(scriptLoader1, execute()).Times(1);
    EXPECT_CALL(scriptLoader2, execute()).Times(0);
    EXPECT_CALL(scriptLoader3, execute()).Times(0);

    m_platform.runSingleTask();

    // We can't safely distruct ScriptRunner with unexecuted MockScriptLoaders (real ScriptLoader is fine) so drain them.
    testing::Mock::VerifyAndClear(&scriptLoader2);
    testing::Mock::VerifyAndClear(&scriptLoader3);
    EXPECT_CALL(scriptLoader2, execute()).Times(1);
    EXPECT_CALL(scriptLoader3, execute()).Times(1);
    EXPECT_CALL(scriptLoader2, isReady()).WillRepeatedly(Return(true));
    EXPECT_CALL(scriptLoader3, isReady()).WillRepeatedly(Return(true));
    m_platform.runAllTasks();
}

} // namespace blink
