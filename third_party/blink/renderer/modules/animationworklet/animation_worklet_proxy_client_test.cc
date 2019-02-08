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
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/modules/worklet/worklet_thread_test_common.h"
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
    reporting_proxy_ = std::make_unique<WorkerReportingProxy>();
  }

  void AddGlobalScopeForTesting(WorkerThread* thread,
                                AnimationWorkletProxyClient* proxy_client,
                                WaitableEvent* waitable_event) {
    proxy_client->AddGlobalScopeForTesting(
        To<WorkletGlobalScope>(thread->GlobalScope()));
    waitable_event->Signal();
  }

  Persistent<AnimationWorkletProxyClient> proxy_client_;
  std::unique_ptr<MockMutatorClient> mutator_client_;
  scoped_refptr<base::TestSimpleTaskRunner> mutator_task_runner_;
  std::unique_ptr<WorkerReportingProxy> reporting_proxy_;
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

TEST_F(AnimationWorkletProxyClientTest, SelectGlobalScope) {
  // Global scopes must be created on worker threads.
  std::unique_ptr<WorkerThread> first_worklet =
      CreateAnimationAndPaintWorkletThread(
          &GetDocument(), reporting_proxy_.get(), proxy_client_);
  std::unique_ptr<WorkerThread> second_worklet =
      CreateAnimationAndPaintWorkletThread(
          &GetDocument(), reporting_proxy_.get(), proxy_client_);

  ASSERT_NE(first_worklet, second_worklet);

  // Register global scopes with proxy client. This step must be performed on
  // the worker threads.
  WaitableEvent waitable_event;
  PostCrossThreadTask(
      *first_worklet->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBind(
          &AnimationWorkletProxyClientTest::AddGlobalScopeForTesting,
          CrossThreadUnretained(this),
          CrossThreadUnretained(first_worklet.get()),
          CrossThreadPersistent<AnimationWorkletProxyClient>(proxy_client_),
          CrossThreadUnretained(&waitable_event)));
  waitable_event.Wait();

  waitable_event.Reset();
  PostCrossThreadTask(
      *second_worklet->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBind(
          &AnimationWorkletProxyClientTest::AddGlobalScopeForTesting,
          CrossThreadUnretained(this),
          CrossThreadUnretained(second_worklet.get()),
          CrossThreadPersistent<AnimationWorkletProxyClient>(proxy_client_),
          CrossThreadUnretained(&waitable_event)));
  waitable_event.Wait();

  AnimationWorkletGlobalScope* stateful_global_scope =
      proxy_client_->global_scopes_[0];
  AnimationWorkletGlobalScope* first_stateless_global_scope =
      proxy_client_->global_scopes_[0];
  AnimationWorkletGlobalScope* second_stateless_global_scope =
      proxy_client_->global_scopes_[1];

  // Initialize switch countdown to 1, to force a switch in the stateless
  // global scope on the second call.
  proxy_client_->next_global_scope_switch_countdown_ = 1;
  EXPECT_EQ(proxy_client_->SelectStatefulGlobalScope(), stateful_global_scope);
  EXPECT_EQ(proxy_client_->SelectStatelessGlobalScope(),
            first_stateless_global_scope);
  EXPECT_EQ(proxy_client_->SelectStatefulGlobalScope(), stateful_global_scope);
  EXPECT_EQ(proxy_client_->SelectStatelessGlobalScope(),
            second_stateless_global_scope);

  // Increase countdown and verify that the switchover adjusts as expected.
  proxy_client_->next_global_scope_switch_countdown_ = 3;
  EXPECT_EQ(proxy_client_->SelectStatefulGlobalScope(), stateful_global_scope);
  EXPECT_EQ(proxy_client_->SelectStatelessGlobalScope(),
            second_stateless_global_scope);
  EXPECT_EQ(proxy_client_->SelectStatefulGlobalScope(), stateful_global_scope);
  EXPECT_EQ(proxy_client_->SelectStatelessGlobalScope(),
            second_stateless_global_scope);
  EXPECT_EQ(proxy_client_->SelectStatefulGlobalScope(), stateful_global_scope);
  EXPECT_EQ(proxy_client_->SelectStatelessGlobalScope(),
            second_stateless_global_scope);
  EXPECT_EQ(proxy_client_->SelectStatefulGlobalScope(), stateful_global_scope);
  EXPECT_EQ(proxy_client_->SelectStatelessGlobalScope(),
            first_stateless_global_scope);

  first_worklet->Terminate();
  first_worklet->WaitForShutdownForTesting();
  second_worklet->Terminate();
  second_worklet->WaitForShutdownForTesting();
}

}  // namespace blink
