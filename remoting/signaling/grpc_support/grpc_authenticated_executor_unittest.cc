// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/grpc_support/grpc_authenticated_executor.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/scoped_task_environment.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/signaling/grpc_support/grpc_async_unary_request.h"
#include "remoting/signaling/grpc_support/grpc_support_test_services.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using testing::_;
using testing::Invoke;

class MockExecutor : public GrpcExecutor {
 public:
  MOCK_METHOD1(ExecuteRpc, void(std::unique_ptr<GrpcAsyncRequest>));
};

}  // namespace

class GrpcAuthenticatedExecutorTest : public testing::Test {
 public:
  void SetUp() override;

 protected:
  MockExecutor* mock_executor_;
  std::unique_ptr<GrpcAuthenticatedExecutor> executor_;

 private:
  FakeOAuthTokenGetter token_getter_{OAuthTokenGetter::Status::SUCCESS,
                                     "fake_user", "fake_token"};
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

void GrpcAuthenticatedExecutorTest::SetUp() {
  executor_ = std::make_unique<GrpcAuthenticatedExecutor>(&token_getter_);
  auto mock_executor = std::make_unique<MockExecutor>();
  mock_executor_ = mock_executor.get();
  executor_->executor_ = std::move(mock_executor);
}

// Unfortunately we can't verify whether the credentials are set because
// grpc::ClientContext has not getter for the credentials.
TEST_F(GrpcAuthenticatedExecutorTest, VerifyExecuteRpcCallIsForwarded) {
  base::RunLoop run_loop;
  auto request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce([](grpc::ClientContext*, const EchoRequest&,
                        grpc::CompletionQueue*) {
        return std::unique_ptr<grpc::ClientAsyncResponseReader<EchoResponse>>();
      }),
      std::make_unique<grpc::ClientContext>(), EchoRequest(),
      base::DoNothing::Once<const grpc::Status&, const EchoResponse&>());
  auto* request_ptr = request.get();
  EXPECT_CALL(*mock_executor_, ExecuteRpc(_))
      .WillOnce(Invoke([&](std::unique_ptr<GrpcAsyncRequest> request) {
        ASSERT_EQ(request_ptr, request.get());
        run_loop.QuitWhenIdle();
      }));
  executor_->ExecuteRpc(std::move(request));
  run_loop.Run();
}

}  // namespace remoting
