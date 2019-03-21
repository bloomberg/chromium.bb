// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_messaging_client.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "remoting/signaling/ftl_grpc_test_environment.h"
#include "remoting/signaling/ftl_services.grpc.pb.h"
#include "remoting/signaling/grpc_support/grpc_async_test_server.h"
#include "remoting/signaling/grpc_support/grpc_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using ::testing::Return;

using PullMessagesResponder =
    test::GrpcServerResponder<ftl::PullMessagesResponse>;
using AckMessagesResponder =
    test::GrpcServerResponder<ftl::AckMessagesResponse>;

constexpr char kFakeSenderId[] = "fake_sender@gmail.com";
constexpr char kFakeReceiverId[] = "fake_receiver@gmail.com";
constexpr char kMessage1Id[] = "msg_1";
constexpr char kMessage2Id[] = "msg_1";
constexpr char kMessage1Text[] = "Message 1";
constexpr char kMessage2Text[] = "Message 2";

ftl::InboxMessage CreateMessage(const std::string& message_id,
                                const std::string& message_text) {
  ftl::InboxMessage message;
  message.mutable_sender_id()->set_id(kFakeSenderId);
  message.mutable_receiver_id()->set_id(kFakeReceiverId);
  message.set_message_type(ftl::InboxMessage_MessageType_CHROMOTING_MESSAGE);
  message.set_message_id(message_id);
  ftl::ChromotingMessage crd_message;
  crd_message.set_message(message_text);
  std::string serialized_message;
  bool succeeded = crd_message.SerializeToString(&serialized_message);
  EXPECT_TRUE(succeeded);
  message.set_message(serialized_message);
  return message;
}

}  // namespace

class FtlMessagingClientTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

 protected:
  // Calls are scheduled sequentially and handled on the server thread.
  void ServerWaitAndRespondToPullMessagesRequest(
      const ftl::PullMessagesResponse& response,
      const grpc::Status& status);
  void ServerWaitAndRespondToAckMessagesRequest(
      base::OnceCallback<grpc::Status(const ftl::AckMessagesRequest&)> handler,
      base::OnceClosure on_done);

  std::unique_ptr<FtlMessagingClient> messaging_client_;

 private:
  using Messaging =
      google::internal::communications::instantmessaging::v1::Messaging;

  std::unique_ptr<test::GrpcAsyncTestServer> server_;
  std::unique_ptr<test::FtlGrpcTestEnvironment> test_environment_;
  scoped_refptr<base::SequencedTaskRunner> server_task_runner_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

void FtlMessagingClientTest::SetUp() {
  server_task_runner_ =
      base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()});
  server_ = std::make_unique<test::GrpcAsyncTestServer>(
      std::make_unique<Messaging::AsyncService>());
  test_environment_ = std::make_unique<test::FtlGrpcTestEnvironment>(
      server_->CreateInProcessChannel());
  messaging_client_ =
      std::make_unique<FtlMessagingClient>(test_environment_->context());
}

void FtlMessagingClientTest::TearDown() {
  messaging_client_.reset();
  test_environment_.reset();
}

void FtlMessagingClientTest::ServerWaitAndRespondToPullMessagesRequest(
    const ftl::PullMessagesResponse& response,
    const grpc::Status& status) {
  server_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const ftl::PullMessagesResponse& response,
             const grpc::Status& status, test::GrpcAsyncTestServer* server) {
            ftl::PullMessagesRequest request;
            auto responder = server->HandleRequest(
                &Messaging::AsyncService::RequestPullMessages, &request);
            responder->Respond(response, status);
          },
          response, status, server_.get()));
}

void FtlMessagingClientTest::ServerWaitAndRespondToAckMessagesRequest(
    base::OnceCallback<grpc::Status(const ftl::AckMessagesRequest&)> handler,
    base::OnceClosure on_done) {
  server_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceCallback<grpc::Status(const ftl::AckMessagesRequest&)>
                 handler,
             test::GrpcAsyncTestServer* server) {
            ftl::AckMessagesRequest request;
            auto responder = server->HandleRequest(
                &Messaging::AsyncService::RequestAckMessages, &request);
            grpc::Status status = std::move(handler).Run(request);
            responder->Respond(ftl::AckMessagesResponse(), status);
          },
          std::move(handler), server_.get()),
      std::move(on_done));
}

TEST_F(FtlMessagingClientTest, TestPullMessages_ReturnsNoMessage) {
  base::RunLoop run_loop;
  auto subscription = messaging_client_->RegisterMessageCallback(
      base::BindRepeating([](const std::string& sender_id,
                             const std::string& message) { NOTREACHED(); }));
  messaging_client_->PullMessages(test::CheckStatusThenQuitRunLoopCallback(
      FROM_HERE, grpc::StatusCode::OK, &run_loop));
  ServerWaitAndRespondToPullMessagesRequest(ftl::PullMessagesResponse(),
                                            grpc::Status::OK);
  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, TestPullMessages_Unauthenticated) {
  base::RunLoop run_loop;
  auto subscription = messaging_client_->RegisterMessageCallback(
      base::BindRepeating([](const std::string& sender_id,
                             const std::string& message) { NOTREACHED(); }));
  messaging_client_->PullMessages(test::CheckStatusThenQuitRunLoopCallback(
      FROM_HERE, grpc::StatusCode::UNAUTHENTICATED, &run_loop));
  ServerWaitAndRespondToPullMessagesRequest(
      ftl::PullMessagesResponse(),
      grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Unauthenticated"));
  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, TestPullMessages_IgnoresUnknownMessageType) {
  base::RunLoop run_loop;

  auto subscription = messaging_client_->RegisterMessageCallback(
      base::BindRepeating([](const std::string& sender_id,
                             const std::string& message) { NOTREACHED(); }));

  messaging_client_->PullMessages(test::CheckStatusThenQuitRunLoopCallback(
      FROM_HERE, grpc::StatusCode::OK, &run_loop));

  ftl::PullMessagesResponse response;
  ftl::InboxMessage* message = response.add_messages();
  message->set_message_id(kMessage1Id);
  message->mutable_sender_id()->set_id(kFakeSenderId);
  message->mutable_receiver_id()->set_id(kFakeReceiverId);
  message->set_message_type(ftl::InboxMessage_MessageType_UNKNOWN);
  ServerWaitAndRespondToPullMessagesRequest(response, grpc::Status::OK);
  ServerWaitAndRespondToAckMessagesRequest(
      base::BindLambdaForTesting([&](const ftl::AckMessagesRequest& request) {
        EXPECT_EQ(1, request.messages_size());
        EXPECT_EQ(kFakeReceiverId, request.messages(0).receiver_id().id());
        EXPECT_EQ(kMessage1Id, request.messages(0).message_id());
        return grpc::Status::OK;
      }),
      base::DoNothing());
  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, TestPullMessages_ReturnsAndAcksTwoMessages) {
  base::RunLoop run_loop;

  base::MockCallback<base::RepeatingCallback<void(const std::string& sender_id,
                                                  const std::string& message)>>
      mock_on_incoming_msg;

  EXPECT_CALL(mock_on_incoming_msg, Run(kFakeSenderId, kMessage1Text))
      .WillOnce(Return());
  EXPECT_CALL(mock_on_incoming_msg, Run(kFakeSenderId, kMessage2Text))
      .WillOnce(Return());

  auto subscription =
      messaging_client_->RegisterMessageCallback(mock_on_incoming_msg.Get());

  messaging_client_->PullMessages(test::CheckStatusThenQuitRunLoopCallback(
      FROM_HERE, grpc::StatusCode::OK, &run_loop));

  ftl::PullMessagesResponse pull_messages_response;
  ftl::InboxMessage* message = pull_messages_response.add_messages();
  *message = CreateMessage(kMessage1Id, kMessage1Text);
  message = pull_messages_response.add_messages();
  *message = CreateMessage(kMessage2Id, kMessage2Text);
  ServerWaitAndRespondToPullMessagesRequest(pull_messages_response,
                                            grpc::Status::OK);

  ServerWaitAndRespondToAckMessagesRequest(
      base::BindLambdaForTesting([&](const ftl::AckMessagesRequest& request) {
        EXPECT_EQ(2, request.messages_size());
        EXPECT_EQ(kFakeReceiverId, request.messages(0).receiver_id().id());
        EXPECT_EQ(kFakeReceiverId, request.messages(1).receiver_id().id());
        EXPECT_EQ(kMessage1Id, request.messages(0).message_id());
        EXPECT_EQ(kMessage2Id, request.messages(1).message_id());
        return grpc::Status::OK;
      }),
      base::DoNothing());

  run_loop.Run();
}

}  // namespace remoting
