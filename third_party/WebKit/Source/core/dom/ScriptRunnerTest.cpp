// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ScriptRunner.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ScriptLoader.h"
#include "platform/heap/Handle.h"
#include "platform/scheduler/CancellableTaskFactory.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/Platform.h"
#include "public/platform/WebViewScheduler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Invoke;
using ::testing::ElementsAre;
using ::testing::Return;
using ::testing::WhenSorted;
using ::testing::ElementsAreArray;

namespace blink {

class MockScriptLoader final : public ScriptLoader {
public:
    static PassOwnPtrWillBeRawPtr<MockScriptLoader> create(Element* element)
    {
        return adoptPtrWillBeNoop(new MockScriptLoader(element));
    }
    ~MockScriptLoader() override { }

    MOCK_METHOD0(execute, void());
    MOCK_CONST_METHOD0(isReady, bool());
private:
    explicit MockScriptLoader(Element* element) : ScriptLoader(element, false, false)
    {
    }
};

class ScriptRunnerTest : public testing::Test {
public:
    ScriptRunnerTest()
        : m_document(Document::create())
        , m_element(m_document->createElement("foo", ASSERT_NO_EXCEPTION))
    {
    }

    void SetUp() override
    {
        // We have to create ScriptRunner after initializing platform, because we need
        // Platform::current()->currentThread()->scheduler()->loadingTaskRunner()
        // to be initialized before creating ScriptRunner to save it in constructor.
        m_scriptRunner = ScriptRunner::create(m_document.get());
    }

    void TearDown() override
    {
        m_scriptRunner.release();
    }

    RefPtrWillBePersistent<Document> m_document;
    RefPtrWillBePersistent<Element> m_element;
    TestingPlatformSupportWithMockScheduler m_platform;
    OwnPtrWillBePersistent<ScriptRunner> m_scriptRunner;
    WTF::Vector<int> m_order;
};

TEST_F(ScriptRunnerTest, QueueSingleScript_Async)
{
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader = MockScriptLoader::create(m_element.get());
    m_scriptRunner->queueScriptForExecution(scriptLoader.get(), ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->notifyScriptReady(scriptLoader.get(), ScriptRunner::ASYNC_EXECUTION);

    EXPECT_CALL(*scriptLoader, execute());
    m_platform.mockWebScheduler()->runAllTasks();
}

TEST_F(ScriptRunnerTest, QueueSingleScript_InOrder)
{
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader = MockScriptLoader::create(m_element.get());
    m_scriptRunner->queueScriptForExecution(scriptLoader.get(), ScriptRunner::IN_ORDER_EXECUTION);

    EXPECT_CALL(*scriptLoader, isReady()).WillOnce(Return(true));
    EXPECT_CALL(*scriptLoader, execute());

    m_scriptRunner->notifyScriptReady(scriptLoader.get(), ScriptRunner::IN_ORDER_EXECUTION);

    m_platform.mockWebScheduler()->runAllTasks();
}

TEST_F(ScriptRunnerTest, QueueMultipleScripts_InOrder)
{
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader1 = MockScriptLoader::create(m_element.get());
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader2 = MockScriptLoader::create(m_element.get());
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader3 = MockScriptLoader::create(m_element.get());

    WillBeHeapVector<RawPtrWillBeMember<MockScriptLoader>> scriptLoaders;
    scriptLoaders.append(scriptLoader1.get());
    scriptLoaders.append(scriptLoader2.get());
    scriptLoaders.append(scriptLoader3.get());

    for (ScriptLoader* scriptLoader : scriptLoaders) {
        m_scriptRunner->queueScriptForExecution(scriptLoader, ScriptRunner::IN_ORDER_EXECUTION);
    }

    for (size_t i = 0; i < scriptLoaders.size(); ++i) {
        EXPECT_CALL(*scriptLoaders[i], execute())
            .WillOnce(Invoke([this, i] {
                m_order.append(i + 1);
            }));
    }

    // Make the scripts become ready in reverse order.
    bool isReady[] = { false, false, false };

    for (size_t i = 0; i < scriptLoaders.size(); ++i) {
        EXPECT_CALL(*scriptLoaders[i], isReady())
            .WillRepeatedly(Invoke([&isReady, i] {
                return isReady[i];
            }));
    }

    for (int i = 2; i >= 0; i--) {
        isReady[i] = true;
        m_scriptRunner->notifyScriptReady(scriptLoaders[i], ScriptRunner::IN_ORDER_EXECUTION);
        m_platform.mockWebScheduler()->runAllTasks();
    }

    // But ensure the scripts were run in the expected order.
    EXPECT_THAT(m_order, ElementsAre(1, 2, 3));
}

TEST_F(ScriptRunnerTest, QueueMixedScripts)
{
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader1 = MockScriptLoader::create(m_element.get());
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader2 = MockScriptLoader::create(m_element.get());
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader3 = MockScriptLoader::create(m_element.get());
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader4 = MockScriptLoader::create(m_element.get());
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader5 = MockScriptLoader::create(m_element.get());


    m_scriptRunner->queueScriptForExecution(scriptLoader1.get(), ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->queueScriptForExecution(scriptLoader2.get(), ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->queueScriptForExecution(scriptLoader3.get(), ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->queueScriptForExecution(scriptLoader4.get(), ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->queueScriptForExecution(scriptLoader5.get(), ScriptRunner::ASYNC_EXECUTION);

    EXPECT_CALL(*scriptLoader1, isReady()).WillRepeatedly(Return(true));
    EXPECT_CALL(*scriptLoader2, isReady())
        .WillRepeatedly(Return(false));
    m_scriptRunner->notifyScriptReady(scriptLoader1.get(), ScriptRunner::IN_ORDER_EXECUTION);

    EXPECT_CALL(*scriptLoader2, isReady()).WillRepeatedly(Return(true));
    EXPECT_CALL(*scriptLoader3, isReady())
        .WillRepeatedly(Return(false));
    m_scriptRunner->notifyScriptReady(scriptLoader2.get(), ScriptRunner::IN_ORDER_EXECUTION);

    EXPECT_CALL(*scriptLoader3, isReady())
        .WillRepeatedly(Return(true));
    m_scriptRunner->notifyScriptReady(scriptLoader3.get(), ScriptRunner::IN_ORDER_EXECUTION);

    m_scriptRunner->notifyScriptReady(scriptLoader4.get(), ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->notifyScriptReady(scriptLoader5.get(), ScriptRunner::ASYNC_EXECUTION);

    EXPECT_CALL(*scriptLoader1, execute()).WillOnce(Invoke([this] {
        m_order.append(1);
    }));
    EXPECT_CALL(*scriptLoader2, execute()).WillOnce(Invoke([this] {
        m_order.append(2);
    }));
    EXPECT_CALL(*scriptLoader3, execute()).WillOnce(Invoke([this] {
        m_order.append(3);
    }));
    EXPECT_CALL(*scriptLoader4, execute()).WillOnce(Invoke([this] {
        m_order.append(4);
    }));
    EXPECT_CALL(*scriptLoader5, execute()).WillOnce(Invoke([this] {
        m_order.append(5);
    }));

    m_platform.mockWebScheduler()->runAllTasks();

    // Async tasks are expected to run first.
    EXPECT_THAT(m_order, ElementsAre(4, 5, 1, 2, 3));
}

TEST_F(ScriptRunnerTest, QueueReentrantScript_Async)
{
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader1 = MockScriptLoader::create(m_element.get());
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader2 = MockScriptLoader::create(m_element.get());
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader3 = MockScriptLoader::create(m_element.get());

    m_scriptRunner->queueScriptForExecution(scriptLoader1.get(), ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->queueScriptForExecution(scriptLoader2.get(), ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->queueScriptForExecution(scriptLoader3.get(), ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->notifyScriptReady(scriptLoader1.get(), ScriptRunner::ASYNC_EXECUTION);

    MockScriptLoader* scriptLoader = scriptLoader2.get();
    EXPECT_CALL(*scriptLoader1, execute()).WillOnce(Invoke([scriptLoader, this] {
        m_order.append(1);
        m_scriptRunner->notifyScriptReady(scriptLoader, ScriptRunner::ASYNC_EXECUTION);
    }));

    scriptLoader = scriptLoader3.get();
    EXPECT_CALL(*scriptLoader2, execute()).WillOnce(Invoke([scriptLoader, this] {
        m_order.append(2);
        m_scriptRunner->notifyScriptReady(scriptLoader, ScriptRunner::ASYNC_EXECUTION);
    }));

    EXPECT_CALL(*scriptLoader3, execute()).WillOnce(Invoke([this] {
        m_order.append(3);
    }));

    // Make sure that re-entrant calls to notifyScriptReady don't cause ScriptRunner::execute to do
    // more work than expected.
    m_platform.mockWebScheduler()->runSingleTask();
    EXPECT_THAT(m_order, ElementsAre(1));

    m_platform.mockWebScheduler()->runSingleTask();
    EXPECT_THAT(m_order, ElementsAre(1, 2));

    m_platform.mockWebScheduler()->runSingleTask();
    EXPECT_THAT(m_order, ElementsAre(1, 2, 3));
}

TEST_F(ScriptRunnerTest, QueueReentrantScript_InOrder)
{
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader1 = MockScriptLoader::create(m_element.get());
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader2 = MockScriptLoader::create(m_element.get());
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader3 = MockScriptLoader::create(m_element.get());

    EXPECT_CALL(*scriptLoader1, isReady()).WillRepeatedly(Return(true));
    EXPECT_CALL(*scriptLoader2, isReady()).WillRepeatedly(Return(true));
    EXPECT_CALL(*scriptLoader3, isReady()).WillRepeatedly(Return(true));

    m_scriptRunner->queueScriptForExecution(scriptLoader1.get(), ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->notifyScriptReady(scriptLoader1.get(), ScriptRunner::IN_ORDER_EXECUTION);

    MockScriptLoader* scriptLoader = scriptLoader2.get();
    EXPECT_CALL(*scriptLoader1, execute())
        .WillOnce(Invoke([scriptLoader, &scriptLoader2, this] {
            m_order.append(1);
            m_scriptRunner->queueScriptForExecution(scriptLoader, ScriptRunner::IN_ORDER_EXECUTION);
            m_scriptRunner->notifyScriptReady(scriptLoader2.get(), ScriptRunner::IN_ORDER_EXECUTION);
        }));

    scriptLoader = scriptLoader3.get();
    EXPECT_CALL(*scriptLoader2, execute())
        .WillOnce(Invoke([scriptLoader, &scriptLoader3, this] {
            m_order.append(2);
            m_scriptRunner->queueScriptForExecution(scriptLoader, ScriptRunner::IN_ORDER_EXECUTION);
            m_scriptRunner->notifyScriptReady(scriptLoader3.get(), ScriptRunner::IN_ORDER_EXECUTION);
        }));

    EXPECT_CALL(*scriptLoader3, execute()).WillOnce(Invoke([this] {
        m_order.append(3);
    }));

    // Make sure that re-entrant calls to queueScriptForExecution don't cause ScriptRunner::execute to do
    // more work than expected.
    m_platform.mockWebScheduler()->runSingleTask();
    EXPECT_THAT(m_order, ElementsAre(1));

    m_platform.mockWebScheduler()->runSingleTask();
    EXPECT_THAT(m_order, ElementsAre(1, 2));

    m_platform.mockWebScheduler()->runSingleTask();
    EXPECT_THAT(m_order, ElementsAre(1, 2, 3));
}

TEST_F(ScriptRunnerTest, QueueReentrantScript_ManyAsyncScripts)
{
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoaders[20];
    for (int i = 0; i < 20; i++)
        scriptLoaders[i] = nullptr;

    for (int i = 0; i < 20; i++) {
        scriptLoaders[i] = MockScriptLoader::create(m_element.get());
        EXPECT_CALL(*scriptLoaders[i], isReady()).WillRepeatedly(Return(true));

        m_scriptRunner->queueScriptForExecution(scriptLoaders[i].get(), ScriptRunner::ASYNC_EXECUTION);

        if (i > 0) {
            EXPECT_CALL(*scriptLoaders[i], execute()).WillOnce(Invoke([this, i] {
                m_order.append(i);
            }));
        }
    }

    m_scriptRunner->notifyScriptReady(scriptLoaders[0].get(), ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->notifyScriptReady(scriptLoaders[1].get(), ScriptRunner::ASYNC_EXECUTION);

    EXPECT_CALL(*scriptLoaders[0], execute()).WillOnce(Invoke([&scriptLoaders, this] {
        for (int i = 2; i < 20; i++)
            m_scriptRunner->notifyScriptReady(scriptLoaders[i].get(), ScriptRunner::ASYNC_EXECUTION);
        m_order.append(0);
    }));

    m_platform.mockWebScheduler()->runAllTasks();

    int expected[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19
    };

    EXPECT_THAT(m_order, testing::ElementsAreArray(expected));
}

TEST_F(ScriptRunnerTest, ResumeAndSuspend_InOrder)
{
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader1 = MockScriptLoader::create(m_element.get());
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader2 = MockScriptLoader::create(m_element.get());
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader3 = MockScriptLoader::create(m_element.get());

    m_scriptRunner->queueScriptForExecution(scriptLoader1.get(), ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->queueScriptForExecution(scriptLoader2.get(), ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->queueScriptForExecution(scriptLoader3.get(), ScriptRunner::IN_ORDER_EXECUTION);

    EXPECT_CALL(*scriptLoader1, execute()).WillOnce(Invoke([this] {
        m_order.append(1);
    }));
    EXPECT_CALL(*scriptLoader2, execute()).WillOnce(Invoke([this] {
        m_order.append(2);
    }));
    EXPECT_CALL(*scriptLoader3, execute()).WillOnce(Invoke([this] {
        m_order.append(3);
    }));

    EXPECT_CALL(*scriptLoader2, isReady())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*scriptLoader3, isReady())
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*scriptLoader1, isReady())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*scriptLoader2, isReady())
        .WillRepeatedly(Return(false));
    m_scriptRunner->notifyScriptReady(scriptLoader1.get(), ScriptRunner::IN_ORDER_EXECUTION);

    EXPECT_CALL(*scriptLoader2, isReady())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*scriptLoader3, isReady())
        .WillRepeatedly(Return(false));
    m_scriptRunner->notifyScriptReady(scriptLoader2.get(), ScriptRunner::IN_ORDER_EXECUTION);

    EXPECT_CALL(*scriptLoader3, isReady())
        .WillRepeatedly(Return(true));
    m_scriptRunner->notifyScriptReady(scriptLoader3.get(), ScriptRunner::IN_ORDER_EXECUTION);

    m_platform.mockWebScheduler()->runSingleTask();
    m_scriptRunner->suspend();
    m_scriptRunner->resume();
    m_platform.mockWebScheduler()->runAllTasks();

    // Make sure elements are correct and in right order.
    EXPECT_THAT(m_order, ElementsAre(1, 2, 3));
}

TEST_F(ScriptRunnerTest, ResumeAndSuspend_Async)
{
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader1 = MockScriptLoader::create(m_element.get());
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader2 = MockScriptLoader::create(m_element.get());
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader3 = MockScriptLoader::create(m_element.get());

    m_scriptRunner->queueScriptForExecution(scriptLoader1.get(), ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->queueScriptForExecution(scriptLoader2.get(), ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->queueScriptForExecution(scriptLoader3.get(), ScriptRunner::ASYNC_EXECUTION);

    m_scriptRunner->notifyScriptReady(scriptLoader1.get(), ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->notifyScriptReady(scriptLoader2.get(), ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->notifyScriptReady(scriptLoader3.get(), ScriptRunner::ASYNC_EXECUTION);

    EXPECT_CALL(*scriptLoader1, execute())
        .WillOnce(Invoke([this] {
            m_order.append(1);
        }));
    EXPECT_CALL(*scriptLoader2, execute())
        .WillOnce(Invoke([this] {
            m_order.append(2);
        }));
    EXPECT_CALL(*scriptLoader3, execute())
        .WillOnce(Invoke([this] {
            m_order.append(3);
        }));

    m_platform.mockWebScheduler()->runSingleTask();
    m_scriptRunner->suspend();
    m_scriptRunner->resume();
    m_platform.mockWebScheduler()->runAllTasks();

    // Make sure elements are correct.
    EXPECT_THAT(m_order, WhenSorted(ElementsAre(1, 2, 3)));
}

TEST_F(ScriptRunnerTest, LateNotifications)
{
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader1 = MockScriptLoader::create(m_element.get());
    OwnPtrWillBeRawPtr<MockScriptLoader> scriptLoader2 = MockScriptLoader::create(m_element.get());

    EXPECT_CALL(*scriptLoader1, isReady()).WillRepeatedly(Return(true));
    EXPECT_CALL(*scriptLoader2, isReady()).WillRepeatedly(Return(true));

    m_scriptRunner->queueScriptForExecution(scriptLoader1.get(), ScriptRunner::IN_ORDER_EXECUTION);
    m_scriptRunner->queueScriptForExecution(scriptLoader2.get(), ScriptRunner::IN_ORDER_EXECUTION);

    EXPECT_CALL(*scriptLoader1, execute()).WillOnce(Invoke([this] {
        m_order.append(1);
    }));
    EXPECT_CALL(*scriptLoader2, execute()).WillOnce(Invoke([this] {
        m_order.append(2);
    }));

    m_scriptRunner->notifyScriptReady(scriptLoader1.get(), ScriptRunner::IN_ORDER_EXECUTION);
    m_platform.mockWebScheduler()->runAllTasks();

    // At this moment all tasks can be already executed. Make sure that we do not crash here.
    m_scriptRunner->notifyScriptReady(scriptLoader2.get(), ScriptRunner::IN_ORDER_EXECUTION);
    m_platform.mockWebScheduler()->runAllTasks();

    EXPECT_THAT(m_order, ElementsAre(1, 2));
}

TEST_F(ScriptRunnerTest, TasksWithDeadScriptRunner)
{
    OwnPtrWillBePersistent<MockScriptLoader> scriptLoader1 = MockScriptLoader::create(m_element.get());
    OwnPtrWillBePersistent<MockScriptLoader> scriptLoader2 = MockScriptLoader::create(m_element.get());

    EXPECT_CALL(*scriptLoader1, isReady()).WillRepeatedly(Return(true));
    EXPECT_CALL(*scriptLoader2, isReady()).WillRepeatedly(Return(true));

    m_scriptRunner->queueScriptForExecution(scriptLoader1.get(), ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->queueScriptForExecution(scriptLoader2.get(), ScriptRunner::ASYNC_EXECUTION);

    m_scriptRunner->notifyScriptReady(scriptLoader1.get(), ScriptRunner::ASYNC_EXECUTION);
    m_scriptRunner->notifyScriptReady(scriptLoader2.get(), ScriptRunner::ASYNC_EXECUTION);

    m_scriptRunner.release();

#if ENABLE(OILPAN)
    Heap::collectAllGarbage();
#endif

    // m_scriptRunner is gone. We need to make sure that ScriptRunner::Task do not access dead object.
    EXPECT_CALL(*scriptLoader1, execute()).Times(0);
    EXPECT_CALL(*scriptLoader2, execute()).Times(0);

    m_platform.mockWebScheduler()->runAllTasks();
}

} // namespace blink
