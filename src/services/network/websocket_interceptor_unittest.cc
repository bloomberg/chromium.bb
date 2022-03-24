// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/websocket_interceptor.h"
#include <memory>

#include "base/logging.h"
#include "base/test/task_environment.h"
#include "services/network/throttling/network_conditions.h"
#include "services/network/throttling/throttling_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

uint32_t kNetLogSourceId = 123;
absl::optional<base::UnguessableToken> kThrottlingProfileId =
    base::UnguessableToken::Create();

class MockCallback {
 public:
  MOCK_METHOD(void, Callback, ());
};

class WebSocketInterceptorTest : public ::testing::Test {
 protected:
  WebSocketInterceptorTest() {
    interceptor_ = std::make_unique<WebSocketInterceptor>(
        kNetLogSourceId, kThrottlingProfileId, WebSocketInterceptor::kOutgoing);
  }

  base::OnceClosure MakeCallback() {
    return base::BindOnce(&MockCallback::Callback,
                          base::Unretained(&mock_callback_));
  }

  MockCallback mock_callback_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<WebSocketInterceptor> interceptor_;
};

TEST_F(WebSocketInterceptorTest, DoesNotInterferWhenNoEmualatedConditions) {
  EXPECT_CALL(mock_callback_, Callback()).Times(0);
  ThrottlingController::SetConditions(*kThrottlingProfileId, nullptr);
  EXPECT_EQ(WebSocketInterceptor::kContinue,
            interceptor_->Intercept(42, MakeCallback()));
  EXPECT_EQ(WebSocketInterceptor::kContinue,
            interceptor_->Intercept(42, MakeCallback()));
}

TEST_F(WebSocketInterceptorTest, ShouldWaitWhenOffline) {
  EXPECT_CALL(mock_callback_, Callback()).Times(0);
  ThrottlingController::SetConditions(
      *kThrottlingProfileId,
      std::make_unique<NetworkConditions>(/*offline=*/true));
  EXPECT_EQ(WebSocketInterceptor::kShouldWait,
            interceptor_->Intercept(42, MakeCallback()));
}

TEST_F(WebSocketInterceptorTest, ShouldWaitWhenSlow) {
  ThrottlingController::SetConditions(
      *kThrottlingProfileId,
      std::make_unique<NetworkConditions>(/*offline=*/false, /*latency=*/0,
                                          /*download=*/0,
                                          /*upload=*/1));
  EXPECT_EQ(WebSocketInterceptor::kShouldWait,
            interceptor_->Intercept(42, MakeCallback()));
  EXPECT_CALL(mock_callback_, Callback()).Times(1);

  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(WebSocketInterceptorTest, SubsequentInterceptWhenSlow) {
  ThrottlingController::SetConditions(
      *kThrottlingProfileId,
      std::make_unique<NetworkConditions>(/*offline=*/false, /*latency=*/0,
                                          /*download=*/0,
                                          /*upload=*/1));
  EXPECT_EQ(WebSocketInterceptor::kShouldWait,
            interceptor_->Intercept(42, MakeCallback()));

  EXPECT_CALL(mock_callback_, Callback()).Times(1);
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_EQ(WebSocketInterceptor::kShouldWait,
            interceptor_->Intercept(42, MakeCallback()));
}

TEST_F(WebSocketInterceptorTest, OfflineCallbackInvokedWhenBackOnline) {
  ThrottlingController::SetConditions(
      *kThrottlingProfileId,
      std::make_unique<NetworkConditions>(/*offline=*/true));
  EXPECT_CALL(mock_callback_, Callback()).Times(0);
  EXPECT_EQ(WebSocketInterceptor::kShouldWait,
            interceptor_->Intercept(42, MakeCallback()));

  EXPECT_CALL(mock_callback_, Callback()).Times(1);
  ThrottlingController::SetConditions(*kThrottlingProfileId, nullptr);
  interceptor_->Intercept(42, MakeCallback());
}

TEST_F(WebSocketInterceptorTest, SlowAfterOffline) {
  ThrottlingController::SetConditions(
      *kThrottlingProfileId,
      std::make_unique<NetworkConditions>(/*offline=*/true));
  EXPECT_CALL(mock_callback_, Callback()).Times(0);
  EXPECT_EQ(WebSocketInterceptor::kShouldWait,
            interceptor_->Intercept(42, MakeCallback()));

  EXPECT_CALL(mock_callback_, Callback()).Times(1);
  ThrottlingController::SetConditions(
      *kThrottlingProfileId,
      std::make_unique<NetworkConditions>(/*offline=*/false, /*latency=*/0,
                                          /*download=*/0,
                                          /*upload=*/1));
  task_environment_.FastForwardUntilNoTasksRemain();
  interceptor_->Intercept(43, MakeCallback());
  EXPECT_CALL(mock_callback_, Callback()).Times(1);
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(WebSocketInterceptorTest, UsesRightDirection) {
  WebSocketInterceptor incoming_interceptor(
      kNetLogSourceId, kThrottlingProfileId, WebSocketInterceptor::kIncoming);
  WebSocketInterceptor outgoing_interceptor(
      kNetLogSourceId, kThrottlingProfileId, WebSocketInterceptor::kOutgoing);
  ThrottlingController::SetConditions(
      *kThrottlingProfileId,
      std::make_unique<NetworkConditions>(/*offline=*/false, /*latency=*/0,
                                          /*download=*/1,
                                          /*upload=*/0));
  incoming_interceptor.Intercept(42, MakeCallback());
  EXPECT_CALL(mock_callback_, Callback()).Times(1);
  task_environment_.FastForwardUntilNoTasksRemain();

  outgoing_interceptor.Intercept(42, MakeCallback());
  EXPECT_CALL(mock_callback_, Callback()).Times(0);
  task_environment_.FastForwardUntilNoTasksRemain();
}

}  // namespace
}  // namespace network
