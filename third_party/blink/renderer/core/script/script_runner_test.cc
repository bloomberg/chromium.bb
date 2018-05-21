// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/script_runner.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/script/mock_script_element_base.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"

using testing::Invoke;
using testing::ElementsAre;
using testing::Return;
using testing::WhenSorted;
using testing::ElementsAreArray;
using testing::_;

namespace blink {

class MockPendingScript : public PendingScript {
 public:
  static MockPendingScript* Create(ScriptElementBase* element,
                                   ScriptSchedulingType scheduling_type) {
    return new MockPendingScript(element, scheduling_type);
  }
  ~MockPendingScript() override {}

  MOCK_CONST_METHOD0(GetScriptType, ScriptType());
  MOCK_CONST_METHOD1(CheckMIMETypeBeforeRunScript, bool(Document*));
  MOCK_CONST_METHOD2(GetSource, Script*(const KURL&, bool&));
  MOCK_CONST_METHOD0(IsExternal, bool());
  MOCK_CONST_METHOD0(ErrorOccurred, bool());
  MOCK_CONST_METHOD0(WasCanceled, bool());
  MOCK_CONST_METHOD0(UrlForTracing, KURL());
  MOCK_METHOD0(RemoveFromMemoryCache, void());

  enum class State {
    kStreamingNotReady,
    kReadyToBeStreamed,
    kStreaming,
    kStreamingFinished,
  };

  bool IsCurrentlyStreaming() const override {
    return state_ == State::kStreaming;
  }

  void PrepareForStreaming() {
    DCHECK_EQ(state_, State::kStreamingNotReady);
    state_ = State::kReadyToBeStreamed;
  }

  bool StartStreamingIfPossible(ScriptStreamer::Type type,
                                base::OnceClosure closure) override {
    if (state_ != State::kReadyToBeStreamed)
      return false;

    state_ = State::kStreaming;
    streaming_finished_callback_ = std::move(closure);
    return true;
  }

  void SimulateStreamingEnd() {
    DCHECK_EQ(state_, State::kStreaming);
    state_ = State::kStreamingFinished;
    std::move(streaming_finished_callback_).Run();
  }

  State state() const { return state_; }

  bool IsReady() const { return is_ready_; }
  void SetIsReady(bool is_ready) { is_ready_ = is_ready; }

 protected:
  MOCK_METHOD0(DisposeInternal, void());
  MOCK_CONST_METHOD0(CheckState, void());

 private:
  MockPendingScript(ScriptElementBase* element,
                    ScriptSchedulingType scheduling_type)
      : PendingScript(element, TextPosition()) {
    SetSchedulingType(scheduling_type);
  }

  State state_ = State::kStreamingNotReady;
  bool is_ready_ = false;
  base::OnceClosure streaming_finished_callback_;
};

class MockScriptLoader final : public ScriptLoader {
 public:
  static MockScriptLoader* CreateInOrder(Document* document) {
    return Create(document, ScriptSchedulingType::kInOrder);
  }

  static MockScriptLoader* CreateAsync(Document* document) {
    return Create(document, ScriptSchedulingType::kAsync);
  }

  ~MockScriptLoader() override {}

  MOCK_METHOD0(Execute, void());

  void Trace(blink::Visitor*) override;

  MockPendingScript* GetPendingScriptIfControlledByScriptRunner() override {
    return mock_pending_script_.Get();
  }

 private:
  explicit MockScriptLoader(ScriptElementBase* element)
      : ScriptLoader(element, false, false, false) {}

  static MockScriptLoader* Create(Document* document,
                                  ScriptSchedulingType scheduling_type) {
    MockScriptElementBase* element = MockScriptElementBase::Create();
    auto* loader = new MockScriptLoader(element);
    EXPECT_CALL(*element, Loader()).WillRepeatedly(Return(loader));
    EXPECT_CALL(*element, GetDocument())
        .WillRepeatedly(testing::ReturnRef(*document));
    MockPendingScript* pending_script =
        MockPendingScript::Create(element, scheduling_type);
    EXPECT_CALL(*pending_script, IsExternal()).WillRepeatedly(Return(true));
    loader->mock_pending_script_ = pending_script;
    return loader;
  }

  Member<MockPendingScript> mock_pending_script_;
};

void MockScriptLoader::Trace(blink::Visitor* visitor) {
  ScriptLoader::Trace(visitor);
  visitor->Trace(mock_pending_script_);
}

class ScriptRunnerTest : public testing::Test {
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
  void NotifyScriptReady(MockScriptLoader* script_loader) {
    script_loader->GetPendingScriptIfControlledByScriptRunner()->SetIsReady(
        true);
    script_runner_->NotifyScriptReady(script_loader);
  }

  Persistent<Document> document_;
  Persistent<ScriptRunner> script_runner_;
  WTF::Vector<int> order_;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
};

TEST_F(ScriptRunnerTest, QueueSingleScript_Async) {
  auto* script_loader = MockScriptLoader::CreateAsync(document_);

  script_runner_->QueueScriptForExecution(script_loader);
  NotifyScriptReady(script_loader);

  EXPECT_CALL(*script_loader, Execute());
  platform_->RunUntilIdle();
}

TEST_F(ScriptRunnerTest, QueueSingleScript_InOrder) {
  auto* script_loader = MockScriptLoader::CreateInOrder(document_);
  script_runner_->QueueScriptForExecution(script_loader);

  EXPECT_CALL(*script_loader, Execute());

  NotifyScriptReady(script_loader);

  platform_->RunUntilIdle();
}

TEST_F(ScriptRunnerTest, QueueMultipleScripts_InOrder) {
  auto* script_loader1 = MockScriptLoader::CreateInOrder(document_);
  auto* script_loader2 = MockScriptLoader::CreateInOrder(document_);
  auto* script_loader3 = MockScriptLoader::CreateInOrder(document_);

  HeapVector<Member<MockScriptLoader>> script_loaders;
  script_loaders.push_back(script_loader1);
  script_loaders.push_back(script_loader2);
  script_loaders.push_back(script_loader3);

  for (ScriptLoader* script_loader : script_loaders) {
    script_runner_->QueueScriptForExecution(script_loader);
  }

  for (size_t i = 0; i < script_loaders.size(); ++i) {
    EXPECT_CALL(*script_loaders[i], Execute()).WillOnce(Invoke([this, i] {
      order_.push_back(i + 1);
    }));
  }

  for (int i = 2; i >= 0; i--) {
    NotifyScriptReady(script_loaders[i]);
    platform_->RunUntilIdle();
  }

  // But ensure the scripts were run in the expected order.
  EXPECT_THAT(order_, ElementsAre(1, 2, 3));
}

TEST_F(ScriptRunnerTest, QueueMixedScripts) {
  auto* script_loader1 = MockScriptLoader::CreateInOrder(document_);
  auto* script_loader2 = MockScriptLoader::CreateInOrder(document_);
  auto* script_loader3 = MockScriptLoader::CreateInOrder(document_);
  auto* script_loader4 = MockScriptLoader::CreateAsync(document_);
  auto* script_loader5 = MockScriptLoader::CreateAsync(document_);

  script_runner_->QueueScriptForExecution(script_loader1);
  script_runner_->QueueScriptForExecution(script_loader2);
  script_runner_->QueueScriptForExecution(script_loader3);
  script_runner_->QueueScriptForExecution(script_loader4);
  script_runner_->QueueScriptForExecution(script_loader5);

  NotifyScriptReady(script_loader1);
  NotifyScriptReady(script_loader2);
  NotifyScriptReady(script_loader3);
  NotifyScriptReady(script_loader4);
  NotifyScriptReady(script_loader5);

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
  auto* script_loader1 = MockScriptLoader::CreateAsync(document_);
  auto* script_loader2 = MockScriptLoader::CreateAsync(document_);
  auto* script_loader3 = MockScriptLoader::CreateAsync(document_);

  script_runner_->QueueScriptForExecution(script_loader1);
  script_runner_->QueueScriptForExecution(script_loader2);
  script_runner_->QueueScriptForExecution(script_loader3);
  NotifyScriptReady(script_loader1);

  auto* script_loader = script_loader2;
  EXPECT_CALL(*script_loader1, Execute())
      .WillOnce(Invoke([script_loader, this] {
        order_.push_back(1);
        NotifyScriptReady(script_loader);
      }));

  script_loader = script_loader3;
  EXPECT_CALL(*script_loader2, Execute())
      .WillOnce(Invoke([script_loader, this] {
        order_.push_back(2);
        NotifyScriptReady(script_loader);
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
  auto* script_loader1 = MockScriptLoader::CreateInOrder(document_);
  auto* script_loader2 = MockScriptLoader::CreateInOrder(document_);
  auto* script_loader3 = MockScriptLoader::CreateInOrder(document_);

  script_runner_->QueueScriptForExecution(script_loader1);
  NotifyScriptReady(script_loader1);

  MockScriptLoader* script_loader = script_loader2;
  EXPECT_CALL(*script_loader1, Execute())
      .WillOnce(Invoke([script_loader, &script_loader2, this] {
        order_.push_back(1);
        script_runner_->QueueScriptForExecution(script_loader);
        NotifyScriptReady(script_loader2);
      }));

  script_loader = script_loader3;
  EXPECT_CALL(*script_loader2, Execute())
      .WillOnce(Invoke([script_loader, &script_loader3, this] {
        order_.push_back(2);
        script_runner_->QueueScriptForExecution(script_loader);
        NotifyScriptReady(script_loader3);
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
    script_loaders[i] = MockScriptLoader::CreateAsync(document_);

    script_runner_->QueueScriptForExecution(script_loaders[i]);

    if (i > 0) {
      EXPECT_CALL(*script_loaders[i], Execute()).WillOnce(Invoke([this, i] {
        order_.push_back(i);
      }));
    }
  }

  NotifyScriptReady(script_loaders[0]);
  NotifyScriptReady(script_loaders[1]);

  EXPECT_CALL(*script_loaders[0], Execute())
      .WillOnce(Invoke([&script_loaders, this] {
        for (int i = 2; i < 20; i++) {
          NotifyScriptReady(script_loaders[i]);
        }
        order_.push_back(0);
      }));

  platform_->RunUntilIdle();

  int expected[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
                    10, 11, 12, 13, 14, 15, 16, 17, 18, 19};

  EXPECT_THAT(order_, testing::ElementsAreArray(expected));
}

TEST_F(ScriptRunnerTest, ResumeAndSuspend_InOrder) {
  auto* script_loader1 = MockScriptLoader::CreateInOrder(document_);
  auto* script_loader2 = MockScriptLoader::CreateInOrder(document_);
  auto* script_loader3 = MockScriptLoader::CreateInOrder(document_);

  script_runner_->QueueScriptForExecution(script_loader1);
  script_runner_->QueueScriptForExecution(script_loader2);
  script_runner_->QueueScriptForExecution(script_loader3);

  EXPECT_CALL(*script_loader1, Execute()).WillOnce(Invoke([this] {
    order_.push_back(1);
  }));
  EXPECT_CALL(*script_loader2, Execute()).WillOnce(Invoke([this] {
    order_.push_back(2);
  }));
  EXPECT_CALL(*script_loader3, Execute()).WillOnce(Invoke([this] {
    order_.push_back(3);
  }));

  NotifyScriptReady(script_loader1);
  NotifyScriptReady(script_loader2);
  NotifyScriptReady(script_loader3);

  platform_->RunSingleTask();
  script_runner_->Suspend();
  script_runner_->Resume();
  platform_->RunUntilIdle();

  // Make sure elements are correct and in right order.
  EXPECT_THAT(order_, ElementsAre(1, 2, 3));
}

TEST_F(ScriptRunnerTest, ResumeAndSuspend_Async) {
  auto* script_loader1 = MockScriptLoader::CreateAsync(document_);
  auto* script_loader2 = MockScriptLoader::CreateAsync(document_);
  auto* script_loader3 = MockScriptLoader::CreateAsync(document_);

  script_runner_->QueueScriptForExecution(script_loader1);
  script_runner_->QueueScriptForExecution(script_loader2);
  script_runner_->QueueScriptForExecution(script_loader3);

  NotifyScriptReady(script_loader1);
  NotifyScriptReady(script_loader2);
  NotifyScriptReady(script_loader3);

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
  auto* script_loader1 = MockScriptLoader::CreateInOrder(document_);
  auto* script_loader2 = MockScriptLoader::CreateInOrder(document_);

  script_runner_->QueueScriptForExecution(script_loader1);
  script_runner_->QueueScriptForExecution(script_loader2);

  EXPECT_CALL(*script_loader1, Execute()).WillOnce(Invoke([this] {
    order_.push_back(1);
  }));
  EXPECT_CALL(*script_loader2, Execute()).WillOnce(Invoke([this] {
    order_.push_back(2);
  }));

  NotifyScriptReady(script_loader1);
  platform_->RunUntilIdle();

  // At this moment all tasks can be already executed. Make sure that we do not
  // crash here.
  NotifyScriptReady(script_loader2);
  platform_->RunUntilIdle();

  EXPECT_THAT(order_, ElementsAre(1, 2));
}

TEST_F(ScriptRunnerTest, TasksWithDeadScriptRunner) {
  Persistent<MockScriptLoader> script_loader1 =
      MockScriptLoader::CreateAsync(document_);
  Persistent<MockScriptLoader> script_loader2 =
      MockScriptLoader::CreateAsync(document_);

  script_runner_->QueueScriptForExecution(script_loader1);
  script_runner_->QueueScriptForExecution(script_loader2);

  NotifyScriptReady(script_loader1);
  NotifyScriptReady(script_loader2);

  script_runner_.Release();

  ThreadState::Current()->CollectAllGarbage();

  // m_scriptRunner is gone. We need to make sure that ScriptRunner::Task do not
  // access dead object.
  EXPECT_CALL(*script_loader1, Execute()).Times(0);
  EXPECT_CALL(*script_loader2, Execute()).Times(0);

  platform_->RunUntilIdle();
}

TEST_F(ScriptRunnerTest, TryStreamWhenEnqueingScript) {
  auto* script_loader1 = MockScriptLoader::CreateAsync(document_);
  script_loader1->GetPendingScriptIfControlledByScriptRunner()->SetIsReady(
      true);
  script_runner_->QueueScriptForExecution(script_loader1);
}

TEST_F(ScriptRunnerTest, DontExecuteWhileStreaming) {
  auto* script_loader = MockScriptLoader::CreateAsync(document_);

  // Enqueue script.
  script_runner_->QueueScriptForExecution(script_loader);

  // Simulate script load and mark the pending script as streaming ready.
  script_loader->GetPendingScriptIfControlledByScriptRunner()->SetIsReady(true);
  script_loader->GetPendingScriptIfControlledByScriptRunner()
      ->PrepareForStreaming();
  NotifyScriptReady(script_loader);

  // ScriptLoader should have started streaming by now.
  EXPECT_EQ(
      script_loader->GetPendingScriptIfControlledByScriptRunner()->state(),
      MockPendingScript::State::kStreaming);

  // Note that there is no expectation for ScriptLoader::Execute() yet,
  // so the mock will fail if it's called anyway.
  platform_->RunUntilIdle();

  // Finish streaming.
  script_loader->GetPendingScriptIfControlledByScriptRunner()
      ->SimulateStreamingEnd();

  // Now that streaming is finished, expect Execute() to be called.
  EXPECT_CALL(*script_loader, Execute()).Times(1);
  platform_->RunUntilIdle();
}

}  // namespace blink
