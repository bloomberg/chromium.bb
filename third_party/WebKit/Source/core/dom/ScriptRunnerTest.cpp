// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ScriptRunner.h"

#include "core/dom/MockScriptElementBase.h"
#include "core/dom/ScriptLoader.h"
#include "platform/bindings/RuntimeCallStats.h"
#include "platform/heap/Handle.h"
#include "platform/scheduler/renderer/web_view_scheduler.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/Platform.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Invoke;
using ::testing::ElementsAre;
using ::testing::Return;
using ::testing::WhenSorted;
using ::testing::ElementsAreArray;
using ::testing::_;

namespace blink {

class MockPendingScript : public PendingScript {
 public:
  static MockPendingScript* Create() { return new MockPendingScript; }
  ~MockPendingScript() override {}

  MOCK_CONST_METHOD0(GetScriptType, ScriptType());
  MOCK_CONST_METHOD2(GetSource, Script*(const KURL&, bool&));
  MOCK_CONST_METHOD0(IsReady, bool());
  MOCK_CONST_METHOD0(IsExternal, bool());
  MOCK_CONST_METHOD0(ErrorOccurred, bool());
  MOCK_CONST_METHOD0(WasCanceled, bool());
  MOCK_CONST_METHOD0(UrlForClassicScript, KURL());
  MOCK_METHOD0(RemoveFromMemoryCache, void());

  MOCK_CONST_METHOD0(IsCurrentlyStreaming, bool());
  // The currently released googletest cannot mock methods with C++ move-only
  // types like std::unique_ptr. This has been promised to be fixed in a
  // future release of googletest, but for now we use the recommended
  // workaround of 'bumping' the method-to-be-mocked to another method.
  bool StartStreamingIfPossible(
      ScriptStreamer::Type type,
      std::unique_ptr<WTF::Closure> closure) override {
    bool started = DoStartStreamingIfPossible(type, closure.get());
    if (started)
      closure.release();
    return started;
  }
  MOCK_METHOD2(DoStartStreamingIfPossible,
               bool(ScriptStreamer::Type, WTF::Closure*));

 protected:
  MOCK_METHOD0(DisposeInternal, void());
  MOCK_CONST_METHOD0(CheckState, void());

 private:
  MockPendingScript() : PendingScript(nullptr, TextPosition()) {}
};

class MockScriptLoader final : public ScriptLoader {
 public:
  static MockScriptLoader* Create() {
    return (new MockScriptLoader())->SetupForNonStreaming();
  }
  ~MockScriptLoader() override {}

  MOCK_METHOD0(Execute, void());
  MOCK_CONST_METHOD0(IsReady, bool());
  MOCK_METHOD0(GetPendingScriptIfScriptIsAsync, PendingScript*());

  // Set up the mock for streaming. The closure passed in will receive the
  // 'finish streaming' callback, so that the test case can control when
  // the mock streaming has mock finished.
  MockScriptLoader* SetupForStreaming(
      std::unique_ptr<WTF::Closure>& finished_callback);
  MockScriptLoader* SetupForNonStreaming();

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit MockScriptLoader()
      : ScriptLoader(MockScriptElementBase::Create(), false, false, false) {}
  Member<MockPendingScript> mock_pending_script_;
};

void MockScriptLoader::Trace(blink::Visitor* visitor) {
  ScriptLoader::Trace(visitor);
  visitor->Trace(mock_pending_script_);
}

MockScriptLoader* MockScriptLoader::SetupForStreaming(
    std::unique_ptr<WTF::Closure>& finished_callback) {
  mock_pending_script_ = MockPendingScript::Create();
  SetupForNonStreaming();

  // Mock the streaming functions and save the 'streaming done' callback
  // into the callback done variable. This way, we can control when
  // streamig is done.
  EXPECT_CALL(*mock_pending_script_, DoStartStreamingIfPossible(_, _))
      .WillOnce(Return(false))
      .WillOnce(Invoke([&finished_callback](ScriptStreamer::Type,
                                            WTF::Closure* callback) -> bool {
        finished_callback.reset(callback);
        return true;
      }))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pending_script_, IsCurrentlyStreaming())
      .WillRepeatedly(
          Invoke([&finished_callback] { return !!finished_callback; }));
  return this;
}

MockScriptLoader* MockScriptLoader::SetupForNonStreaming() {
  EXPECT_CALL(*this, GetPendingScriptIfScriptIsAsync())
      .WillRepeatedly(Invoke([this]() -> PendingScript* {
        return this->mock_pending_script_.Get();
      }));
  return this;
}

class ScriptRunnerTest : public ::testing::Test {
 public:
  ScriptRunnerTest() : document_(Document::CreateForTest()) {}

  void SetUp() override {
    // We have to create ScriptRunner after initializing platform, because we
    // need Platform::current()->currentThread()->scheduler()->
    // loadingTaskRunner() to be initialized before creating ScriptRunner to
    // save it in constructor.
    script_runner_ = ScriptRunner::Create(document_.Get());
    RuntimeCallStats::SetRuntimeCallStatsForTesting();
  }
  void TearDown() override {
    script_runner_.Release();
    RuntimeCallStats::ClearRuntimeCallStatsForTesting();
  }

 protected:
  Persistent<Document> document_;
  Persistent<ScriptRunner> script_runner_;
  WTF::Vector<int> order_;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
};

TEST_F(ScriptRunnerTest, QueueSingleScript_Async) {
  MockScriptLoader* script_loader = MockScriptLoader::Create();
  script_runner_->QueueScriptForExecution(script_loader, ScriptRunner::kAsync);
  script_runner_->NotifyScriptReady(script_loader, ScriptRunner::kAsync);

  EXPECT_CALL(*script_loader, Execute());
  platform_->RunUntilIdle();
}

TEST_F(ScriptRunnerTest, QueueSingleScript_InOrder) {
  MockScriptLoader* script_loader = MockScriptLoader::Create();
  script_runner_->QueueScriptForExecution(script_loader,
                                          ScriptRunner::kInOrder);

  EXPECT_CALL(*script_loader, IsReady()).WillOnce(Return(true));
  EXPECT_CALL(*script_loader, Execute());

  script_runner_->NotifyScriptReady(script_loader, ScriptRunner::kInOrder);

  platform_->RunUntilIdle();
}

TEST_F(ScriptRunnerTest, QueueMultipleScripts_InOrder) {
  MockScriptLoader* script_loader1 = MockScriptLoader::Create();
  MockScriptLoader* script_loader2 = MockScriptLoader::Create();
  MockScriptLoader* script_loader3 = MockScriptLoader::Create();

  HeapVector<Member<MockScriptLoader>> script_loaders;
  script_loaders.push_back(script_loader1);
  script_loaders.push_back(script_loader2);
  script_loaders.push_back(script_loader3);

  for (ScriptLoader* script_loader : script_loaders) {
    script_runner_->QueueScriptForExecution(script_loader,
                                            ScriptRunner::kInOrder);
  }

  for (size_t i = 0; i < script_loaders.size(); ++i) {
    EXPECT_CALL(*script_loaders[i], Execute()).WillOnce(Invoke([this, i] {
      order_.push_back(i + 1);
    }));
  }

  // Make the scripts become ready in reverse order.
  bool is_ready[] = {false, false, false};

  for (size_t i = 0; i < script_loaders.size(); ++i) {
    EXPECT_CALL(*script_loaders[i], IsReady())
        .WillRepeatedly(Invoke([&is_ready, i] { return is_ready[i]; }));
  }

  for (int i = 2; i >= 0; i--) {
    is_ready[i] = true;
    script_runner_->NotifyScriptReady(script_loaders[i],
                                      ScriptRunner::kInOrder);
    platform_->RunUntilIdle();
  }

  // But ensure the scripts were run in the expected order.
  EXPECT_THAT(order_, ElementsAre(1, 2, 3));
}

TEST_F(ScriptRunnerTest, QueueMixedScripts) {
  MockScriptLoader* script_loader1 = MockScriptLoader::Create();
  MockScriptLoader* script_loader2 = MockScriptLoader::Create();
  MockScriptLoader* script_loader3 = MockScriptLoader::Create();
  MockScriptLoader* script_loader4 = MockScriptLoader::Create();
  MockScriptLoader* script_loader5 = MockScriptLoader::Create();

  script_runner_->QueueScriptForExecution(script_loader1,
                                          ScriptRunner::kInOrder);
  script_runner_->QueueScriptForExecution(script_loader2,
                                          ScriptRunner::kInOrder);
  script_runner_->QueueScriptForExecution(script_loader3,
                                          ScriptRunner::kInOrder);
  script_runner_->QueueScriptForExecution(script_loader4, ScriptRunner::kAsync);
  script_runner_->QueueScriptForExecution(script_loader5, ScriptRunner::kAsync);

  EXPECT_CALL(*script_loader1, IsReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(*script_loader2, IsReady()).WillRepeatedly(Return(false));
  script_runner_->NotifyScriptReady(script_loader1, ScriptRunner::kInOrder);

  EXPECT_CALL(*script_loader2, IsReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(*script_loader3, IsReady()).WillRepeatedly(Return(false));
  script_runner_->NotifyScriptReady(script_loader2, ScriptRunner::kInOrder);

  EXPECT_CALL(*script_loader3, IsReady()).WillRepeatedly(Return(true));
  script_runner_->NotifyScriptReady(script_loader3, ScriptRunner::kInOrder);

  script_runner_->NotifyScriptReady(script_loader4, ScriptRunner::kAsync);
  script_runner_->NotifyScriptReady(script_loader5, ScriptRunner::kAsync);

  EXPECT_CALL(*script_loader1, Execute()).WillOnce(Invoke([this] {
    order_.push_back(1);
  }));
  EXPECT_CALL(*script_loader2, Execute()).WillOnce(Invoke([this] {
    order_.push_back(2);
  }));
  EXPECT_CALL(*script_loader3, Execute()).WillOnce(Invoke([this] {
    order_.push_back(3);
  }));
  EXPECT_CALL(*script_loader4, Execute()).WillOnce(Invoke([this] {
    order_.push_back(4);
  }));
  EXPECT_CALL(*script_loader5, Execute()).WillOnce(Invoke([this] {
    order_.push_back(5);
  }));

  platform_->RunUntilIdle();

  // Async tasks are expected to run first.
  EXPECT_THAT(order_, ElementsAre(4, 5, 1, 2, 3));
}

TEST_F(ScriptRunnerTest, QueueReentrantScript_Async) {
  MockScriptLoader* script_loader1 = MockScriptLoader::Create();
  MockScriptLoader* script_loader2 = MockScriptLoader::Create();
  MockScriptLoader* script_loader3 = MockScriptLoader::Create();

  script_runner_->QueueScriptForExecution(script_loader1, ScriptRunner::kAsync);
  script_runner_->QueueScriptForExecution(script_loader2, ScriptRunner::kAsync);
  script_runner_->QueueScriptForExecution(script_loader3, ScriptRunner::kAsync);
  script_runner_->NotifyScriptReady(script_loader1, ScriptRunner::kAsync);

  MockScriptLoader* script_loader = script_loader2;
  EXPECT_CALL(*script_loader1, Execute())
      .WillOnce(Invoke([script_loader, this] {
        order_.push_back(1);
        script_runner_->NotifyScriptReady(script_loader, ScriptRunner::kAsync);
      }));

  script_loader = script_loader3;
  EXPECT_CALL(*script_loader2, Execute())
      .WillOnce(Invoke([script_loader, this] {
        order_.push_back(2);
        script_runner_->NotifyScriptReady(script_loader, ScriptRunner::kAsync);
      }));

  EXPECT_CALL(*script_loader3, Execute()).WillOnce(Invoke([this] {
    order_.push_back(3);
  }));

  // Make sure that re-entrant calls to notifyScriptReady don't cause
  // ScriptRunner::execute to do more work than expected.
  platform_->RunSingleTask();
  EXPECT_THAT(order_, ElementsAre(1));

  platform_->RunSingleTask();
  EXPECT_THAT(order_, ElementsAre(1, 2));

  platform_->RunSingleTask();
  EXPECT_THAT(order_, ElementsAre(1, 2, 3));
}

TEST_F(ScriptRunnerTest, QueueReentrantScript_InOrder) {
  MockScriptLoader* script_loader1 = MockScriptLoader::Create();
  MockScriptLoader* script_loader2 = MockScriptLoader::Create();
  MockScriptLoader* script_loader3 = MockScriptLoader::Create();

  EXPECT_CALL(*script_loader1, IsReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(*script_loader2, IsReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(*script_loader3, IsReady()).WillRepeatedly(Return(true));

  script_runner_->QueueScriptForExecution(script_loader1,
                                          ScriptRunner::kInOrder);
  script_runner_->NotifyScriptReady(script_loader1, ScriptRunner::kInOrder);

  MockScriptLoader* script_loader = script_loader2;
  EXPECT_CALL(*script_loader1, Execute())
      .WillOnce(Invoke([script_loader, &script_loader2, this] {
        order_.push_back(1);
        script_runner_->QueueScriptForExecution(script_loader,
                                                ScriptRunner::kInOrder);
        script_runner_->NotifyScriptReady(script_loader2,
                                          ScriptRunner::kInOrder);
      }));

  script_loader = script_loader3;
  EXPECT_CALL(*script_loader2, Execute())
      .WillOnce(Invoke([script_loader, &script_loader3, this] {
        order_.push_back(2);
        script_runner_->QueueScriptForExecution(script_loader,
                                                ScriptRunner::kInOrder);
        script_runner_->NotifyScriptReady(script_loader3,
                                          ScriptRunner::kInOrder);
      }));

  EXPECT_CALL(*script_loader3, Execute()).WillOnce(Invoke([this] {
    order_.push_back(3);
  }));

  // Make sure that re-entrant calls to queueScriptForExecution don't cause
  // ScriptRunner::execute to do more work than expected.
  platform_->RunSingleTask();
  EXPECT_THAT(order_, ElementsAre(1));

  platform_->RunSingleTask();
  EXPECT_THAT(order_, ElementsAre(1, 2));

  platform_->RunSingleTask();
  EXPECT_THAT(order_, ElementsAre(1, 2, 3));
}

TEST_F(ScriptRunnerTest, QueueReentrantScript_ManyAsyncScripts) {
  MockScriptLoader* script_loaders[20];
  for (int i = 0; i < 20; i++)
    script_loaders[i] = nullptr;

  for (int i = 0; i < 20; i++) {
    script_loaders[i] = MockScriptLoader::Create();
    EXPECT_CALL(*script_loaders[i], IsReady()).WillRepeatedly(Return(true));

    script_runner_->QueueScriptForExecution(script_loaders[i],
                                            ScriptRunner::kAsync);

    if (i > 0) {
      EXPECT_CALL(*script_loaders[i], Execute()).WillOnce(Invoke([this, i] {
        order_.push_back(i);
      }));
    }
  }

  script_runner_->NotifyScriptReady(script_loaders[0], ScriptRunner::kAsync);
  script_runner_->NotifyScriptReady(script_loaders[1], ScriptRunner::kAsync);

  EXPECT_CALL(*script_loaders[0], Execute())
      .WillOnce(Invoke([&script_loaders, this] {
        for (int i = 2; i < 20; i++)
          script_runner_->NotifyScriptReady(script_loaders[i],
                                            ScriptRunner::kAsync);
        order_.push_back(0);
      }));

  platform_->RunUntilIdle();

  int expected[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
                    10, 11, 12, 13, 14, 15, 16, 17, 18, 19};

  EXPECT_THAT(order_, ::testing::ElementsAreArray(expected));
}

TEST_F(ScriptRunnerTest, ResumeAndSuspend_InOrder) {
  MockScriptLoader* script_loader1 = MockScriptLoader::Create();
  MockScriptLoader* script_loader2 = MockScriptLoader::Create();
  MockScriptLoader* script_loader3 = MockScriptLoader::Create();

  script_runner_->QueueScriptForExecution(script_loader1,
                                          ScriptRunner::kInOrder);
  script_runner_->QueueScriptForExecution(script_loader2,
                                          ScriptRunner::kInOrder);
  script_runner_->QueueScriptForExecution(script_loader3,
                                          ScriptRunner::kInOrder);

  EXPECT_CALL(*script_loader1, Execute()).WillOnce(Invoke([this] {
    order_.push_back(1);
  }));
  EXPECT_CALL(*script_loader2, Execute()).WillOnce(Invoke([this] {
    order_.push_back(2);
  }));
  EXPECT_CALL(*script_loader3, Execute()).WillOnce(Invoke([this] {
    order_.push_back(3);
  }));

  EXPECT_CALL(*script_loader2, IsReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(*script_loader3, IsReady()).WillRepeatedly(Return(true));

  EXPECT_CALL(*script_loader1, IsReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(*script_loader2, IsReady()).WillRepeatedly(Return(false));
  script_runner_->NotifyScriptReady(script_loader1, ScriptRunner::kInOrder);

  EXPECT_CALL(*script_loader2, IsReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(*script_loader3, IsReady()).WillRepeatedly(Return(false));
  script_runner_->NotifyScriptReady(script_loader2, ScriptRunner::kInOrder);

  EXPECT_CALL(*script_loader3, IsReady()).WillRepeatedly(Return(true));
  script_runner_->NotifyScriptReady(script_loader3, ScriptRunner::kInOrder);

  platform_->RunSingleTask();
  script_runner_->Suspend();
  script_runner_->Resume();
  platform_->RunUntilIdle();

  // Make sure elements are correct and in right order.
  EXPECT_THAT(order_, ElementsAre(1, 2, 3));
}

TEST_F(ScriptRunnerTest, ResumeAndSuspend_Async) {
  MockScriptLoader* script_loader1 = MockScriptLoader::Create();
  MockScriptLoader* script_loader2 = MockScriptLoader::Create();
  MockScriptLoader* script_loader3 = MockScriptLoader::Create();

  script_runner_->QueueScriptForExecution(script_loader1, ScriptRunner::kAsync);
  script_runner_->QueueScriptForExecution(script_loader2, ScriptRunner::kAsync);
  script_runner_->QueueScriptForExecution(script_loader3, ScriptRunner::kAsync);

  script_runner_->NotifyScriptReady(script_loader1, ScriptRunner::kAsync);
  script_runner_->NotifyScriptReady(script_loader2, ScriptRunner::kAsync);
  script_runner_->NotifyScriptReady(script_loader3, ScriptRunner::kAsync);

  EXPECT_CALL(*script_loader1, Execute()).WillOnce(Invoke([this] {
    order_.push_back(1);
  }));
  EXPECT_CALL(*script_loader2, Execute()).WillOnce(Invoke([this] {
    order_.push_back(2);
  }));
  EXPECT_CALL(*script_loader3, Execute()).WillOnce(Invoke([this] {
    order_.push_back(3);
  }));

  platform_->RunSingleTask();
  script_runner_->Suspend();
  script_runner_->Resume();
  platform_->RunUntilIdle();

  // Make sure elements are correct.
  EXPECT_THAT(order_, WhenSorted(ElementsAre(1, 2, 3)));
}

TEST_F(ScriptRunnerTest, LateNotifications) {
  MockScriptLoader* script_loader1 = MockScriptLoader::Create();
  MockScriptLoader* script_loader2 = MockScriptLoader::Create();

  EXPECT_CALL(*script_loader1, IsReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(*script_loader2, IsReady()).WillRepeatedly(Return(true));

  script_runner_->QueueScriptForExecution(script_loader1,
                                          ScriptRunner::kInOrder);
  script_runner_->QueueScriptForExecution(script_loader2,
                                          ScriptRunner::kInOrder);

  EXPECT_CALL(*script_loader1, Execute()).WillOnce(Invoke([this] {
    order_.push_back(1);
  }));
  EXPECT_CALL(*script_loader2, Execute()).WillOnce(Invoke([this] {
    order_.push_back(2);
  }));

  script_runner_->NotifyScriptReady(script_loader1, ScriptRunner::kInOrder);
  platform_->RunUntilIdle();

  // At this moment all tasks can be already executed. Make sure that we do not
  // crash here.
  script_runner_->NotifyScriptReady(script_loader2, ScriptRunner::kInOrder);
  platform_->RunUntilIdle();

  EXPECT_THAT(order_, ElementsAre(1, 2));
}

TEST_F(ScriptRunnerTest, TasksWithDeadScriptRunner) {
  Persistent<MockScriptLoader> script_loader1 = MockScriptLoader::Create();
  Persistent<MockScriptLoader> script_loader2 = MockScriptLoader::Create();

  EXPECT_CALL(*script_loader1, IsReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(*script_loader2, IsReady()).WillRepeatedly(Return(true));

  script_runner_->QueueScriptForExecution(script_loader1, ScriptRunner::kAsync);
  script_runner_->QueueScriptForExecution(script_loader2, ScriptRunner::kAsync);

  script_runner_->NotifyScriptReady(script_loader1, ScriptRunner::kAsync);
  script_runner_->NotifyScriptReady(script_loader2, ScriptRunner::kAsync);

  script_runner_.Release();

  ThreadState::Current()->CollectAllGarbage();

  // m_scriptRunner is gone. We need to make sure that ScriptRunner::Task do not
  // access dead object.
  EXPECT_CALL(*script_loader1, Execute()).Times(0);
  EXPECT_CALL(*script_loader2, Execute()).Times(0);

  platform_->RunUntilIdle();
}

TEST_F(ScriptRunnerTest, TryStreamWhenEnqueingScript) {
  Persistent<MockScriptLoader> script_loader1 = MockScriptLoader::Create();
  EXPECT_CALL(*script_loader1, IsReady()).WillRepeatedly(Return(true));
  script_runner_->QueueScriptForExecution(script_loader1, ScriptRunner::kAsync);
}

TEST_F(ScriptRunnerTest, DontExecuteWhileStreaming) {
  std::unique_ptr<WTF::Closure> callback;
  Persistent<MockScriptLoader> script_loader1 =
      MockScriptLoader::Create()->SetupForStreaming(callback);
  EXPECT_CALL(*script_loader1, IsReady()).WillRepeatedly(Return(true));

  // Enqueue script & make it ready.
  script_runner_->QueueScriptForExecution(script_loader1, ScriptRunner::kAsync);
  script_runner_->NotifyScriptReady(script_loader1, ScriptRunner::kAsync);

  // We should have started streaming by now.
  CHECK(callback);

  // Note that there is no expectation for ScriptLoader::Execute() yet,
  // so the mock will fail if it's called anyway.
  platform_->RunUntilIdle();

  // Finish streaming. Note that 'callback' must be empty when the callback
  // is called, since out mock uses it to determine whether we're still
  // streaming.
  std::unique_ptr<WTF::Closure> tmp_callback(std::move(callback));
  (*tmp_callback)();

  // Now that streaming is finished, expect Execute() to be called.
  EXPECT_CALL(*script_loader1, Execute()).Times(1);
  platform_->RunUntilIdle();
}

}  // namespace blink
