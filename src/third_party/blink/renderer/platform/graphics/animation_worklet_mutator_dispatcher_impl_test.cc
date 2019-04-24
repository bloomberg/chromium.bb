// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator_dispatcher_impl.h"

#include "base/single_thread_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_thread_type.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator.h"
#include "third_party/blink/renderer/platform/graphics/compositor_mutator_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

#include <memory>

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Mock;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::StrictMock;
using ::testing::Truly;

// This test uses actual threads since mutator logic requires it. This means we
// have dependency on Blink platform to create threads.

namespace blink {
namespace {

std::unique_ptr<Thread> CreateThread(const char* name) {
  return Platform::Current()->CreateThread(
      ThreadCreationParams(WebThreadType::kTestThread)
          .SetThreadNameForTest(name));
}

class MockAnimationWorkletMutator
    : public GarbageCollectedFinalized<MockAnimationWorkletMutator>,
      public AnimationWorkletMutator {
  USING_GARBAGE_COLLECTED_MIXIN(MockAnimationWorkletMutator);

 public:
  MockAnimationWorkletMutator(
      scoped_refptr<base::SingleThreadTaskRunner> expected_runner)
      : expected_runner_(expected_runner) {}

  ~MockAnimationWorkletMutator() override {}

  std::unique_ptr<AnimationWorkletOutput> Mutate(
      std::unique_ptr<AnimationWorkletInput> input) override {
    return std::unique_ptr<AnimationWorkletOutput>(MutateRef(*input));
  }

  // Blocks the worklet thread by posting a task that will complete only when
  // signaled. This blocking ensures that tests of async mutations do not
  // encounter race conditions when validating queuing strategies.
  void BlockWorkletThread() {
    PostCrossThreadTask(
        *expected_runner_, FROM_HERE,
        CrossThreadBind(
            [](base::WaitableEvent* start_processing_event) {
              start_processing_event->Wait();
            },
            WTF::CrossThreadUnretained(&start_processing_event_)));
  }

  void UnblockWorkletThread() { start_processing_event_.Signal(); }

  MOCK_CONST_METHOD0(GetWorkletId, int());
  MOCK_METHOD1(MutateRef,
               AnimationWorkletOutput*(const AnimationWorkletInput&));

  scoped_refptr<base::SingleThreadTaskRunner> expected_runner_;
  base::WaitableEvent start_processing_event_;
};

class MockCompositorMutatorClient : public CompositorMutatorClient {
 public:
  MockCompositorMutatorClient(
      std::unique_ptr<AnimationWorkletMutatorDispatcherImpl> mutator)
      : CompositorMutatorClient(std::move(mutator)) {}
  ~MockCompositorMutatorClient() override {}
  // gmock cannot mock methods with move-only args so we forward it to ourself.
  void SetMutationUpdate(
      std::unique_ptr<cc::MutatorOutputState> output_state) override {
    SetMutationUpdateRef(output_state.get());
  }

  MOCK_METHOD1(SetMutationUpdateRef,
               void(cc::MutatorOutputState* output_state));
};

class AnimationWorkletMutatorDispatcherImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    auto mutator = std::make_unique<AnimationWorkletMutatorDispatcherImpl>(
        /*main_thread_task_runner=*/true);
    mutator_ = mutator.get();
    client_ =
        std::make_unique<::testing::StrictMock<MockCompositorMutatorClient>>(
            std::move(mutator));
  }

  void TearDown() override { mutator_ = nullptr; }

  std::unique_ptr<::testing::StrictMock<MockCompositorMutatorClient>> client_;
  AnimationWorkletMutatorDispatcherImpl* mutator_;
};

std::unique_ptr<AnimationWorkletDispatcherInput> CreateTestMutatorInput() {
  AnimationWorkletInput::AddAndUpdateState state1{
      {11, 1}, "test1", 5000, nullptr, 1};

  AnimationWorkletInput::AddAndUpdateState state2{
      {22, 2}, "test2", 5000, nullptr, 1};

  auto input = std::make_unique<AnimationWorkletDispatcherInput>();
  input->Add(std::move(state1));
  input->Add(std::move(state2));

  return input;
}

bool OnlyIncludesAnimation1(const AnimationWorkletInput& in) {
  return in.added_and_updated_animations.size() == 1 &&
         in.added_and_updated_animations[0].worklet_animation_id.animation_id ==
             1;
}

TEST_F(AnimationWorkletMutatorDispatcherImplTest,
       RegisteredAnimatorShouldOnlyReceiveInputForItself) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(Truly(OnlyIncludesAnimation1)))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(1);
  mutator_->MutateSynchronously(CreateTestMutatorInput());
}

TEST_F(AnimationWorkletMutatorDispatcherImplTest,
       RegisteredAnimatorShouldNotBeMutatedWhenNoInput) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_)).Times(0);
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(0);

  AnimationWorkletInput::AddAndUpdateState state2{
      {22, 2}, "test2", 5000, nullptr, 1};

  auto input = std::make_unique<AnimationWorkletDispatcherInput>();
  input->Add(std::move(state2));

  mutator_->MutateSynchronously(std::move(input));
}

TEST_F(AnimationWorkletMutatorDispatcherImplTest,
       MutationUpdateIsNotInvokedWithNoRegisteredAnimators) {
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(0);
  std::unique_ptr<AnimationWorkletDispatcherInput> input =
      std::make_unique<AnimationWorkletDispatcherInput>();
  mutator_->MutateSynchronously(std::move(input));
}

TEST_F(AnimationWorkletMutatorDispatcherImplTest,
       MutationUpdateIsNotInvokedWithNullOutput) {
  // Create a thread to run mutator tasks.
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_)).Times(1).WillOnce(Return(nullptr));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(0);
  mutator_->MutateSynchronously(CreateTestMutatorInput());
}

TEST_F(AnimationWorkletMutatorDispatcherImplTest,
       MutationUpdateIsInvokedCorrectlyWithSingleRegisteredAnimator) {
  // Create a thread to run mutator tasks.
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(1);
  mutator_->MutateSynchronously(CreateTestMutatorInput());

  // The above call blocks on mutator threads running their tasks so we can
  // safely verify here.
  Mock::VerifyAndClearExpectations(client_.get());

  // Ensure mutator is not invoked after unregistration.
  EXPECT_CALL(*first_mutator, MutateRef(_)).Times(0);
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(0);
  mutator_->UnregisterAnimationWorkletMutator(first_mutator);

  mutator_->MutateSynchronously(CreateTestMutatorInput());
  Mock::VerifyAndClearExpectations(client_.get());
}

TEST_F(AnimationWorkletMutatorDispatcherImplTest,
       MutationUpdateInvokedCorrectlyWithTwoRegisteredAnimatorsOnSameThread) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());
  MockAnimationWorkletMutator* second_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());
  mutator_->RegisterAnimationWorkletMutator(second_mutator,
                                            first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*second_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(22));
  EXPECT_CALL(*second_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(2);
  mutator_->MutateSynchronously(CreateTestMutatorInput());
}

TEST_F(
    AnimationWorkletMutatorDispatcherImplTest,
    MutationUpdateInvokedCorrectlyWithTwoRegisteredAnimatorsOnDifferentThreads) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  std::unique_ptr<Thread> second_thread = CreateThread("SecondAnimationThread");
  MockAnimationWorkletMutator* second_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          second_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());
  mutator_->RegisterAnimationWorkletMutator(second_mutator,
                                            second_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*second_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(22));
  EXPECT_CALL(*second_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(2);
  mutator_->MutateSynchronously(CreateTestMutatorInput());

  // The above call blocks on mutator threads running their tasks so we can
  // safely verify here.
  Mock::VerifyAndClearExpectations(client_.get());

  // Ensure first_mutator is not invoked after unregistration.
  mutator_->UnregisterAnimationWorkletMutator(first_mutator);

  EXPECT_CALL(*first_mutator, GetWorkletId()).Times(0);
  EXPECT_CALL(*first_mutator, MutateRef(_)).Times(0);
  EXPECT_CALL(*second_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(22));
  EXPECT_CALL(*second_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(1);
  mutator_->MutateSynchronously(CreateTestMutatorInput());

  Mock::VerifyAndClearExpectations(client_.get());
}

TEST_F(AnimationWorkletMutatorDispatcherImplTest,
       DispatcherShouldNotHangWhenMutatorGoesAway) {
  // Create a thread to run mutator tasks.
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId()).WillRepeatedly(Return(11));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(0);

  // Shutdown the thread so its task runner no longer executes tasks.
  first_thread.reset();

  mutator_->MutateSynchronously(CreateTestMutatorInput());

  Mock::VerifyAndClearExpectations(client_.get());
}

// -----------------------------------------------------------------------
// Asynchronous version of tests.

// Callback wrapping portion of the async test that is required to run on the
// compositor thread.
using MutateAsyncCallback = WTF::CrossThreadFunction<void()>;

using MutatorDispatcherRef =
    scoped_refptr<AnimationWorkletMutatorDispatcherImpl>;

class AnimationWorkletMutatorDispatcherImplAsyncTest
    : public AnimationWorkletMutatorDispatcherImplTest {
 public:
  AnimationWorkletMutatorDispatcher::AsyncMutationCompleteCallback
  CreateIntermediateResultCallback(MutateStatus expected_result) {
    return ConvertToBaseCallback(
        CrossThreadBind(&AnimationWorkletMutatorDispatcherImplAsyncTest ::
                            VerifyExpectedMutationResult,
                        CrossThreadUnretained(this), expected_result));
  }

  AnimationWorkletMutatorDispatcher::AsyncMutationCompleteCallback
  CreateNotReachedCallback() {
    return ConvertToBaseCallback(CrossThreadBind([](MutateStatus unused) {
      NOTREACHED() << "Mutate complete callback should not have been triggered";
    }));
  }

  AnimationWorkletMutatorDispatcher::AsyncMutationCompleteCallback
  CreateTestCompleteCallback(
      MutateStatus expected_result = MutateStatus::kCompletedWithUpdate) {
    return ConvertToBaseCallback(
        CrossThreadBind(&AnimationWorkletMutatorDispatcherImplAsyncTest ::
                            VerifyCompletedMutationResultAndFinish,
                        CrossThreadUnretained(this), expected_result));
  }

  // Executes run loop until quit closure is called.
  void WaitForTestCompletion() { run_loop_.Run(); }

  void VerifyExpectedMutationResult(MutateStatus expectation,
                                    MutateStatus result) {
    EXPECT_EQ(expectation, result);
    IntermediateResultCallbackRef();
  }

  void VerifyCompletedMutationResultAndFinish(MutateStatus expectation,
                                              MutateStatus result) {
    EXPECT_EQ(expectation, result);
    run_loop_.QuitClosure().Run();
  }

  // Verifying that intermediate result callbacks are invoked the correct number
  // of times.
  MOCK_METHOD0(IntermediateResultCallbackRef, void());

 private:
  base::RunLoop run_loop_;
};

TEST_F(AnimationWorkletMutatorDispatcherImplAsyncTest,
       RegisteredAnimatorShouldOnlyReceiveInputForItself) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(1);

  EXPECT_TRUE(mutator_->MutateAsynchronously(CreateTestMutatorInput(),
                                             MutateQueuingStrategy::kDrop,
                                             CreateTestCompleteCallback()));

  WaitForTestCompletion();
}

TEST_F(AnimationWorkletMutatorDispatcherImplAsyncTest,
       RegisteredAnimatorShouldNotBeMutatedWhenNoInput) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());

  AnimationWorkletInput::AddAndUpdateState state2{
      {22, 2}, "test2", 5000, nullptr, 1};

  auto input = std::make_unique<AnimationWorkletDispatcherInput>();
  input->Add(std::move(state2));

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));

  EXPECT_FALSE(mutator_->MutateAsynchronously(std::move(input),
                                              MutateQueuingStrategy::kDrop,
                                              CreateNotReachedCallback()));
}

TEST_F(AnimationWorkletMutatorDispatcherImplAsyncTest,
       MutationUpdateIsNotInvokedWithNoRegisteredAnimators) {
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(0);
  std::unique_ptr<AnimationWorkletDispatcherInput> input =
      std::make_unique<AnimationWorkletDispatcherInput>();
  EXPECT_FALSE(mutator_->MutateAsynchronously(std::move(input),
                                              MutateQueuingStrategy::kDrop,
                                              CreateNotReachedCallback()));
}

TEST_F(AnimationWorkletMutatorDispatcherImplAsyncTest,
       MutationUpdateIsNotInvokedWithNullOutput) {
  // Create a thread to run mutator tasks.
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_)).Times(1).WillOnce(Return(nullptr));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(0);

  EXPECT_TRUE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), MutateQueuingStrategy::kDrop,
      CreateTestCompleteCallback(MutateStatus::kCompletedNoUpdate)));

  WaitForTestCompletion();
}

TEST_F(AnimationWorkletMutatorDispatcherImplAsyncTest,
       MutationUpdateIsInvokedCorrectlyWithSingleRegisteredAnimator) {
  // Create a thread to run mutator tasks.
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(1);

  EXPECT_TRUE(mutator_->MutateAsynchronously(CreateTestMutatorInput(),
                                             MutateQueuingStrategy::kDrop,
                                             CreateTestCompleteCallback()));

  WaitForTestCompletion();

  // Above call blocks until complete signal is received.
  Mock::VerifyAndClearExpectations(client_.get());

  // Ensure mutator is not invoked after unregistration.
  mutator_->UnregisterAnimationWorkletMutator(first_mutator);
  EXPECT_FALSE(mutator_->MutateAsynchronously(CreateTestMutatorInput(),
                                              MutateQueuingStrategy::kDrop,
                                              CreateNotReachedCallback()));

  Mock::VerifyAndClearExpectations(client_.get());
}

TEST_F(AnimationWorkletMutatorDispatcherImplAsyncTest,
       MutationUpdateInvokedCorrectlyWithTwoRegisteredAnimatorsOnSameThread) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());
  MockAnimationWorkletMutator* second_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());
  mutator_->RegisterAnimationWorkletMutator(second_mutator,
                                            first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*second_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(22));
  EXPECT_CALL(*second_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(2);

  EXPECT_TRUE(mutator_->MutateAsynchronously(CreateTestMutatorInput(),
                                             MutateQueuingStrategy::kDrop,
                                             CreateTestCompleteCallback()));

  WaitForTestCompletion();
}

TEST_F(
    AnimationWorkletMutatorDispatcherImplAsyncTest,
    MutationUpdateInvokedCorrectlyWithTwoRegisteredAnimatorsOnDifferentThreads) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstAnimationThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());

  std::unique_ptr<Thread> second_thread = CreateThread("SecondAnimationThread");
  MockAnimationWorkletMutator* second_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          second_thread->GetTaskRunner());

  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());
  mutator_->RegisterAnimationWorkletMutator(second_mutator,
                                            second_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*second_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(22));
  EXPECT_CALL(*second_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(2);

  EXPECT_TRUE(mutator_->MutateAsynchronously(CreateTestMutatorInput(),
                                             MutateQueuingStrategy::kDrop,
                                             CreateTestCompleteCallback()));

  WaitForTestCompletion();
}

TEST_F(AnimationWorkletMutatorDispatcherImplAsyncTest,
       MutationUpdateDroppedWhenBusy) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstThread");
  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());
  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(1)
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(1);

  // Block Responses until all requests have been queued.
  first_mutator->BlockWorkletThread();
  // Response for first mutator call is blocked until after the second
  // call is sent.
  EXPECT_TRUE(mutator_->MutateAsynchronously(CreateTestMutatorInput(),
                                             MutateQueuingStrategy::kDrop,
                                             CreateTestCompleteCallback()));
  // Second request dropped since busy processing first.
  EXPECT_FALSE(mutator_->MutateAsynchronously(CreateTestMutatorInput(),
                                              MutateQueuingStrategy::kDrop,
                                              CreateNotReachedCallback()));
  // Unblock first request.
  first_mutator->UnblockWorkletThread();

  WaitForTestCompletion();
}

TEST_F(AnimationWorkletMutatorDispatcherImplAsyncTest,
       MutationUpdateQueuedWhenBusy) {
  std::unique_ptr<Thread> first_thread = CreateThread("FirstThread");

  MockAnimationWorkletMutator* first_mutator =
      MakeGarbageCollected<MockAnimationWorkletMutator>(
          first_thread->GetTaskRunner());
  mutator_->RegisterAnimationWorkletMutator(first_mutator,
                                            first_thread->GetTaskRunner());

  EXPECT_CALL(*first_mutator, GetWorkletId())
      .Times(AtLeast(2))
      .WillRepeatedly(Return(11));
  EXPECT_CALL(*first_mutator, MutateRef(_))
      .Times(2)
      .WillOnce(Return(new AnimationWorkletOutput()))
      .WillOnce(Return(new AnimationWorkletOutput()));
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(2);
  EXPECT_CALL(*this, IntermediateResultCallbackRef()).Times(1);

  // Block Responses until all requests have been queued.
  first_mutator->BlockWorkletThread();
  // Response for first mutator call is blocked until after the second
  // call is sent.
  EXPECT_TRUE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), MutateQueuingStrategy::kDrop,
      CreateIntermediateResultCallback(MutateStatus::kCompletedWithUpdate)));
  // First request still processing, queue request.
  EXPECT_TRUE(mutator_->MutateAsynchronously(
      CreateTestMutatorInput(), MutateQueuingStrategy::kQueueAndReplace,
      CreateTestCompleteCallback()));
  // Unblock first request.
  first_mutator->UnblockWorkletThread();

  WaitForTestCompletion();
}

}  // namespace

}  // namespace blink
