// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_grpc_context.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/signaling/ftl_services.grpc.pb.h"
#include "remoting/signaling/grpc_support/grpc_async_test_server.h"
#include "remoting/signaling/grpc_support/grpc_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"

namespace remoting {

namespace {

constexpr char kFakeUserEmail[] = "fake@gmail.com";
constexpr char kFakeAccessToken[] = "Dummy Token";
constexpr char kFakeFtlAuthToken[] = "Dummy FTL Token";

using PullMessagesCallback =
    FtlGrpcContext::RpcCallback<ftl::PullMessagesResponse>;
using IncomingMessageCallback =
    base::RepeatingCallback<void(const ftl::ReceiveMessagesResponse&)>;
using PullMessagesResponder =
    test::GrpcServerResponder<ftl::PullMessagesResponse>;
using ReceiveMessagesResponder =
    test::GrpcServerStreamResponder<ftl::ReceiveMessagesResponse>;

PullMessagesCallback QuitRunLoopOnPullMessagesCallback(
    base::RunLoop* run_loop) {
  return base::BindOnce(
      [](base::RunLoop* run_loop, const grpc::Status&,
         const ftl::PullMessagesResponse&) { run_loop->QuitWhenIdle(); },
      run_loop);
}

}  // namespace

class FtlGrpcContextTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

 protected:
  using Messaging =
      google::internal::communications::instantmessaging::v1::Messaging;

  void SendFakePullMessagesRequest(PullMessagesCallback on_response);
  void SendFakeReceiveMessagesRequest(
      FtlGrpcContext::StreamStartedCallback on_stream_started,
      const IncomingMessageCallback& on_incoming_msg,
      base::OnceCallback<void(const grpc::Status&)> on_channel_closed);
  std::unique_ptr<PullMessagesResponder> HandlePullMessages(
      ftl::PullMessagesRequest* request);
  std::unique_ptr<ReceiveMessagesResponder> HandleReceiveMessages(
      ftl::ReceiveMessagesRequest* request);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<FtlGrpcContext> context_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<Messaging::Stub> stub_;
  std::unique_ptr<test::GrpcAsyncTestServer> server_;
  std::unique_ptr<FakeOAuthTokenGetter> token_getter_;
};

void FtlGrpcContextTest::SetUp() {
  task_runner_ = base::SequencedTaskRunnerHandle::Get();
  server_ = std::make_unique<test::GrpcAsyncTestServer>(
      std::make_unique<Messaging::AsyncService>());
  stub_ = Messaging::NewStub(server_->CreateInProcessChannel());
  token_getter_ = std::make_unique<FakeOAuthTokenGetter>(
      OAuthTokenGetter::Status::SUCCESS, kFakeUserEmail, kFakeAccessToken);
  context_ = std::make_unique<FtlGrpcContext>(token_getter_.get());
}

void FtlGrpcContextTest::TearDown() {
  context_.reset();
  token_getter_.reset();
  server_.reset();
  stub_.reset();
}

void FtlGrpcContextTest::SendFakePullMessagesRequest(
    PullMessagesCallback on_response) {
  DCHECK(context_);
  ftl::PullMessagesRequest request;
  context_->ExecuteRpc(base::BindOnce(&Messaging::Stub::AsyncPullMessages,
                                      base::Unretained(stub_.get())),
                       request, std::move(on_response));
}

void FtlGrpcContextTest::SendFakeReceiveMessagesRequest(
    FtlGrpcContext::StreamStartedCallback on_stream_started,
    const IncomingMessageCallback& on_incoming_msg,
    base::OnceCallback<void(const grpc::Status&)> on_channel_closed) {
  context_->ExecuteServerStreamingRpc(
      base::BindOnce(&Messaging::Stub::AsyncReceiveMessages,
                     base::Unretained(stub_.get())),
      ftl::ReceiveMessagesRequest(), std::move(on_stream_started),
      on_incoming_msg, std::move(on_channel_closed));
}

std::unique_ptr<PullMessagesResponder> FtlGrpcContextTest::HandlePullMessages(
    ftl::PullMessagesRequest* request) {
  return server_->HandleRequest(&Messaging::AsyncService::RequestPullMessages,
                                request);
}

std::unique_ptr<ReceiveMessagesResponder>
FtlGrpcContextTest::HandleReceiveMessages(
    ftl::ReceiveMessagesRequest* request) {
  return server_->HandleStreamRequest(
      &Messaging::AsyncService::RequestReceiveMessages, request);
}

TEST_F(FtlGrpcContextTest, VerifyAPIKeyIsProvided) {
  base::RunLoop run_loop;

  SendFakePullMessagesRequest(QuitRunLoopOnPullMessagesCallback(&run_loop));

  task_runner_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ftl::PullMessagesRequest request;
        auto responder = HandlePullMessages(&request);

        const std::multimap<grpc::string_ref, grpc::string_ref>&
            client_metadata = responder->context()->client_metadata();
        auto api_key_iter = client_metadata.find("x-goog-api-key");
        ASSERT_NE(client_metadata.end(), api_key_iter);
        ASSERT_FALSE(api_key_iter->second.empty());
        responder->Respond(ftl::PullMessagesResponse(), grpc::Status::OK);
      }));
  run_loop.Run();
}

TEST_F(FtlGrpcContextTest, VerifyRequestHeaderIsSet) {
  base::RunLoop run_loop;

  SendFakePullMessagesRequest(QuitRunLoopOnPullMessagesCallback(&run_loop));

  task_runner_->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                           ftl::PullMessagesRequest request;
                           auto responder = HandlePullMessages(&request);
                           ASSERT_TRUE(request.has_header());
                           ASSERT_FALSE(request.header().request_id().empty());
                           ASSERT_FALSE(request.header().app().empty());
                           ASSERT_TRUE(request.header().has_client_info());
                           responder->Respond(ftl::PullMessagesResponse(),
                                              grpc::Status::OK);
                         }));
  run_loop.Run();
}

TEST_F(FtlGrpcContextTest, NoAuthTokenIfNotSet) {
  base::RunLoop run_loop;

  SendFakePullMessagesRequest(QuitRunLoopOnPullMessagesCallback(&run_loop));

  task_runner_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ftl::PullMessagesRequest request;
        auto responder = HandlePullMessages(&request);
        ASSERT_TRUE(request.header().auth_token_payload().empty());
        responder->Respond(ftl::PullMessagesResponse(), grpc::Status::OK);
      }));
  run_loop.Run();
}

TEST_F(FtlGrpcContextTest, HasAuthTokenIfSet) {
  base::RunLoop run_loop;
  context_->SetAuthToken(kFakeFtlAuthToken);

  SendFakePullMessagesRequest(QuitRunLoopOnPullMessagesCallback(&run_loop));

  task_runner_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ftl::PullMessagesRequest request;
        auto responder = HandlePullMessages(&request);
        ASSERT_EQ(kFakeFtlAuthToken, request.header().auth_token_payload());
        responder->Respond(ftl::PullMessagesResponse(), grpc::Status::OK);
      }));
  run_loop.Run();
}

TEST_F(FtlGrpcContextTest, ServerStreamingScenario) {
  base::RunLoop run_loop;
  context_->SetAuthToken(kFakeFtlAuthToken);

  ftl::InboxMessage fake_inbox_message;
  fake_inbox_message.set_message_id("msg_1");

  std::unique_ptr<ScopedGrpcServerStream> server_stream;
  std::unique_ptr<ReceiveMessagesResponder> responder;

  SendFakeReceiveMessagesRequest(
      // On stream started
      base::BindLambdaForTesting(
          [&](std::unique_ptr<ScopedGrpcServerStream> stream) {
            server_stream = std::move(stream);

            ftl::ReceiveMessagesRequest request;
            responder = HandleReceiveMessages(&request);
            ASSERT_TRUE(request.has_header());
            ASSERT_FALSE(request.header().request_id().empty());
            ASSERT_FALSE(request.header().app().empty());
            ASSERT_TRUE(request.header().has_client_info());
            ASSERT_EQ(kFakeFtlAuthToken, request.header().auth_token_payload());

            ftl::ReceiveMessagesResponse response;
            response.set_allocated_inbox_message(
                new ftl::InboxMessage(fake_inbox_message));
            responder->SendMessage(response);
          }),

      // On message received
      base::BindLambdaForTesting(
          [&](const ftl::ReceiveMessagesResponse& response) {
            ASSERT_EQ(fake_inbox_message.message_id(),
                      response.inbox_message().message_id());
            ASSERT_TRUE(responder->WaitForSendMessageResult());
            responder->Close(grpc::Status::OK);
          }),

      // On channel closed
      test::CheckStatusThenQuitRunLoopCallback(FROM_HERE, grpc::StatusCode::OK,
                                               &run_loop));

  run_loop.Run();
}

// TODO(yuweih): Ideally we should verify access token is properly attached too,
// but currently this information seems to be lost in ServiceContext.

}  // namespace remoting
