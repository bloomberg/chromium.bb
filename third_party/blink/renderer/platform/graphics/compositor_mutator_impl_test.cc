// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositor_mutator_impl.h"

#include "base/single_thread_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_thread.h"
#include "third_party/blink/public/platform/web_thread_type.h"
#include "third_party/blink/renderer/platform/graphics/compositor_animator.h"
#include "third_party/blink/renderer/platform/graphics/compositor_mutator_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

#include <memory>

using ::testing::_;
using ::testing::Mock;
using ::testing::StrictMock;
using ::testing::Return;
using ::testing::Truly;

// This test uses actual threads since mutator logic requires it. This means we
// have dependency on Blink platform to create threads.

namespace blink {
namespace {

std::unique_ptr<WebThread> CreateThread(const char* name) {
  return Platform::Current()->CreateThread(
      WebThreadCreationParams(WebThreadType::kTestThread)
          .SetThreadNameForTest(name));
}

class MockCompositorAnimator
    : public GarbageCollectedFinalized<MockCompositorAnimator>,
      public CompositorAnimator {
  USING_GARBAGE_COLLECTED_MIXIN(MockCompositorAnimator);

 public:
  MockCompositorAnimator(
      scoped_refptr<base::SingleThreadTaskRunner> expected_runner)
      : expected_runner_(expected_runner) {}

  ~MockCompositorAnimator() override {}

  std::unique_ptr<AnimationWorkletOutput> Mutate(
      std::unique_ptr<AnimationWorkletInput> input) override {
    return MutateRef(*input);
  }

  MOCK_CONST_METHOD0(GetScopeId, int());
  MOCK_METHOD1(
      MutateRef,
      std::unique_ptr<AnimationWorkletOutput>(const AnimationWorkletInput&));

  scoped_refptr<base::SingleThreadTaskRunner> expected_runner_;
};

class MockCompositorMutatorClient : public CompositorMutatorClient {
 public:
  MockCompositorMutatorClient(std::unique_ptr<CompositorMutatorImpl> mutator)
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

class CompositorMutatorImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    auto mutator = std::make_unique<CompositorMutatorImpl>();
    mutator_ = mutator.get();
    client_ =
        std::make_unique<::testing::StrictMock<MockCompositorMutatorClient>>(
            std::move(mutator));
  }

  void TearDown() override { mutator_ = nullptr; }

  std::unique_ptr<::testing::StrictMock<MockCompositorMutatorClient>> client_;
  CompositorMutatorImpl* mutator_;
};

bool OnlyIncludesAnimation1(const AnimationWorkletInput& in) {
  return in.added_and_updated_animations.size() == 1 &&
         in.added_and_updated_animations[0].worklet_animation_id.animation_id ==
             1;
}

TEST_F(CompositorMutatorImplTest,
       RegisteredAnimatorShouldOnlyReceiveInputForItself) {
  std::unique_ptr<WebThread> first_thread = CreateThread("FirstThread");
  MockCompositorAnimator* first_animator =
      new ::testing::StrictMock<MockCompositorAnimator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterCompositorAnimator(first_animator,
                                       first_thread->GetTaskRunner());

  EXPECT_CALL(*first_animator, GetScopeId()).Times(1).WillOnce(Return(11));
  EXPECT_CALL(*first_animator, MutateRef(Truly(OnlyIncludesAnimation1)))
      .Times(1);
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(1);

  AnimationWorkletInput::AddAndUpdateState state1{
      {11, 1}, "test1", 5000, nullptr};

  AnimationWorkletInput::AddAndUpdateState state2{
      {22, 2}, "test2", 5000, nullptr};

  auto input = std::make_unique<CompositorMutatorInputState>();
  input->Add(std::move(state1));
  input->Add(std::move(state2));

  mutator_->Mutate(std::move(input));
}

TEST_F(CompositorMutatorImplTest, AnimatorShouldNotBeMutatedWhenNoInput) {
  std::unique_ptr<WebThread> first_thread = CreateThread("FirstThread");
  MockCompositorAnimator* first_animator =
      new ::testing::StrictMock<MockCompositorAnimator>(
          first_thread->GetTaskRunner());

  mutator_->RegisterCompositorAnimator(first_animator,
                                       first_thread->GetTaskRunner());

  EXPECT_CALL(*first_animator, GetScopeId()).Times(1).WillOnce(Return(11));
  EXPECT_CALL(*first_animator, MutateRef(_)).Times(0);
  EXPECT_CALL(*client_, SetMutationUpdateRef(_)).Times(0);

  AnimationWorkletInput::AddAndUpdateState state2{
      {22, 2}, "test2", 5000, nullptr};

  auto input = std::make_unique<CompositorMutatorInputState>();
  input->Add(std::move(state2));

  mutator_->Mutate(std::move(input));
}

}  // namespace

}  // namespace blink
