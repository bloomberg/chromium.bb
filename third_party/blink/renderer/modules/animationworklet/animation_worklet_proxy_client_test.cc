// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/animation_worklet_proxy_client.h"

#include <memory>
#include <utility>

#include "base/test/test_simple_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator_dispatcher_impl.h"

namespace blink {

class MockMutatorClient : public MutatorClient {
 public:
  explicit MockMutatorClient(
      std::unique_ptr<AnimationWorkletMutatorDispatcherImpl>);

  void SetMutationUpdate(std::unique_ptr<AnimationWorkletOutput>) override {}
  void NotifyAnimationsPending() override {}
  void NotifyAnimationsReady() override {}
  MOCK_METHOD1(SynchronizeAnimatorName, void(const String&));

  std::unique_ptr<AnimationWorkletMutatorDispatcherImpl> mutator_;
};

MockMutatorClient::MockMutatorClient(
    std::unique_ptr<AnimationWorkletMutatorDispatcherImpl> mutator)
    : mutator_(std::move(mutator)) {
  mutator_->SetClient(this);
}

class AnimationWorkletProxyClientTest : public RenderingTest {
 public:
  AnimationWorkletProxyClientTest() = default;

  void SetUp() override {
    RenderingTest::SetUp();
    auto mutator =
        std::make_unique<AnimationWorkletMutatorDispatcherImpl>(true);
    mutator_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();

    proxy_client_ = MakeGarbageCollected<AnimationWorkletProxyClient>(
        1, nullptr, nullptr, mutator->GetWeakPtr(), mutator_task_runner_);
    mutator_client_ = std::make_unique<MockMutatorClient>(std::move(mutator));
  }

  Persistent<AnimationWorkletProxyClient> proxy_client_;
  std::unique_ptr<MockMutatorClient> mutator_client_;
  scoped_refptr<base::TestSimpleTaskRunner> mutator_task_runner_;
};

TEST_F(AnimationWorkletProxyClientTest,
       AnimationWorkletProxyClientConstruction) {
  AnimationWorkletProxyClient* proxy_client =
      MakeGarbageCollected<AnimationWorkletProxyClient>(1, nullptr, nullptr,
                                                        nullptr, nullptr);
  EXPECT_TRUE(proxy_client->mutator_items_.IsEmpty());

  auto mutator = std::make_unique<AnimationWorkletMutatorDispatcherImpl>(true);
  scoped_refptr<base::SingleThreadTaskRunner> mutator_task_runner =
      mutator->GetTaskRunner();

  proxy_client = MakeGarbageCollected<AnimationWorkletProxyClient>(
      1, nullptr, nullptr, mutator->GetWeakPtr(), mutator_task_runner);
  EXPECT_EQ(proxy_client->mutator_items_.size(), 1u);

  proxy_client = MakeGarbageCollected<AnimationWorkletProxyClient>(
      1, mutator->GetWeakPtr(), mutator_task_runner, mutator->GetWeakPtr(),
      mutator_task_runner);
  EXPECT_EQ(proxy_client->mutator_items_.size(), 2u);
}

// Only sync when the animator is registered kNumStatelessGlobalScopes times.
TEST_F(AnimationWorkletProxyClientTest, RegisteredAnimatorNameShouldSyncOnce) {
  String animator_name = "test_animator";
  ASSERT_FALSE(proxy_client_->registered_animators_.Contains(animator_name));

  for (int8_t i = 0;
       i < AnimationWorkletProxyClient::kNumStatelessGlobalScopes - 1; ++i) {
    EXPECT_CALL(*mutator_client_, SynchronizeAnimatorName(animator_name))
        .Times(0);
    proxy_client_->SynchronizeAnimatorName(animator_name);
    testing::Mock::VerifyAndClearExpectations(mutator_client_.get());
  }

  EXPECT_CALL(*mutator_client_, SynchronizeAnimatorName(animator_name))
      .Times(1);
  proxy_client_->SynchronizeAnimatorName(animator_name);
  mutator_task_runner_->RunUntilIdle();
}

}  // namespace blink
