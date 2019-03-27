// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/grpc_support/grpc_async_executor.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "remoting/signaling/grpc_support/grpc_async_test_server.h"
#include "remoting/signaling/grpc_support/grpc_support_test_services.grpc.pb.h"
#include "remoting/signaling/grpc_support/grpc_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"

namespace remoting {

namespace {

using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Property;

using EchoStreamResponder = test::GrpcServerStreamResponder<EchoResponse>;

base::RepeatingCallback<void(const EchoResponse&)>
NotReachedStreamingCallback() {
  return base::BindRepeating([](const EchoResponse&) { NOTREACHED(); });
}

base::RepeatingCallback<void(const grpc::Status&)> NotReachedStatusCallback() {
  return base::BindRepeating([](const grpc::Status&) { NOTREACHED(); });
}

EchoResponse ResponseForText(const std::string& text) {
  EchoResponse response;
  response.set_text(text);
  return response;
}

}  // namespace

class GrpcAsyncExecutorTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

 protected:
  void AsyncSendText(const std::string& text,
                     GrpcAsyncExecutor::RpcCallback<EchoResponse> callback);
  std::unique_ptr<ScopedGrpcServerStream> StartEchoStream(
      const std::string& request_text,
      const GrpcAsyncExecutor::RpcStreamCallback<EchoResponse> on_incoming_msg,
      GrpcAsyncExecutor::RpcChannelClosedCallback on_channel_closed);

 protected:
  void HandleOneEchoRequest();
  std::unique_ptr<test::GrpcServerResponder<EchoResponse>>
  GetResponderAndFillEchoRequest(EchoRequest* request);
  std::unique_ptr<EchoStreamResponder> HandleEchoStream(
      const base::Location& from_here,
      const std::string& expected_request_text);

  std::unique_ptr<GrpcAsyncExecutor> dispatcher_;

 private:
  base::MessageLoop message_loop_;
  std::unique_ptr<GrpcAsyncExecutorTestService::Stub> stub_;
  std::unique_ptr<test::GrpcAsyncTestServer> server_;
};

void GrpcAsyncExecutorTest::SetUp() {
  dispatcher_ = std::make_unique<GrpcAsyncExecutor>();
  server_ = std::make_unique<test::GrpcAsyncTestServer>(
      std::make_unique<GrpcAsyncExecutorTestService::AsyncService>());
  stub_ =
      GrpcAsyncExecutorTestService::NewStub(server_->CreateInProcessChannel());
}

void GrpcAsyncExecutorTest::TearDown() {
  server_.reset();
  dispatcher_.reset();
  stub_.reset();
}

void GrpcAsyncExecutorTest::AsyncSendText(
    const std::string& text,
    GrpcAsyncExecutor::RpcCallback<EchoResponse> callback) {
  EchoRequest request;
  request.set_text(text);
  dispatcher_->ExecuteAsyncRpc(
      base::BindOnce(&GrpcAsyncExecutorTestService::Stub::AsyncEcho,
                     base::Unretained(stub_.get())),
      std::make_unique<grpc::ClientContext>(), request, std::move(callback));
}

std::unique_ptr<ScopedGrpcServerStream> GrpcAsyncExecutorTest::StartEchoStream(
    const std::string& request_text,
    const GrpcAsyncExecutor::RpcStreamCallback<EchoResponse> on_incoming_msg,
    GrpcAsyncExecutor::RpcChannelClosedCallback on_channel_closed) {
  EchoRequest request;
  request.set_text(request_text);
  return dispatcher_->ExecuteAsyncServerStreamingRpc(
      base::BindOnce(&GrpcAsyncExecutorTestService::Stub::AsyncStreamEcho,
                     base::Unretained(stub_.get())),
      std::make_unique<grpc::ClientContext>(), request, on_incoming_msg,
      std::move(on_channel_closed));
}

void GrpcAsyncExecutorTest::HandleOneEchoRequest() {
  EchoRequest request;
  auto responder = GetResponderAndFillEchoRequest(&request);
  EchoResponse response;
  response.set_text(request.text());
  ASSERT_TRUE(responder->Respond(response, grpc::Status::OK));
}

std::unique_ptr<test::GrpcServerResponder<EchoResponse>>
GrpcAsyncExecutorTest::GetResponderAndFillEchoRequest(EchoRequest* request) {
  return server_->HandleRequest(
      &GrpcAsyncExecutorTestService::AsyncService::RequestEcho, request);
}

std::unique_ptr<EchoStreamResponder> GrpcAsyncExecutorTest::HandleEchoStream(
    const base::Location& from_here,
    const std::string& expected_request_text) {
  EchoRequest request;
  auto responder = server_->HandleStreamRequest(
      &GrpcAsyncExecutorTestService::AsyncService::RequestStreamEcho, &request);
  EXPECT_EQ(expected_request_text, request.text())
      << "Request text mismatched. Location: " << from_here.ToString();
  return responder;
}

TEST_F(GrpcAsyncExecutorTest, DoNothing) {}

TEST_F(GrpcAsyncExecutorTest, SendOneTextAndRespond) {
  base::RunLoop run_loop;
  AsyncSendText("Hello",
                base::BindLambdaForTesting([&](const grpc::Status& status,
                                               const EchoResponse& response) {
                  EXPECT_TRUE(status.ok());
                  EXPECT_EQ("Hello", response.text());
                  run_loop.QuitWhenIdle();
                }));
  HandleOneEchoRequest();
  run_loop.Run();
}

TEST_F(GrpcAsyncExecutorTest, SendTwoTextsAndRespondOneByOne) {
  base::RunLoop run_loop_1;
  AsyncSendText("Hello 1",
                base::BindLambdaForTesting([&](const grpc::Status& status,
                                               const EchoResponse& response) {
                  EXPECT_TRUE(status.ok());
                  EXPECT_EQ("Hello 1", response.text());
                  run_loop_1.QuitWhenIdle();
                }));
  HandleOneEchoRequest();
  run_loop_1.Run();

  base::RunLoop run_loop_2;
  AsyncSendText("Hello 2",
                base::BindLambdaForTesting([&](const grpc::Status& status,
                                               const EchoResponse& response) {
                  EXPECT_TRUE(status.ok());
                  EXPECT_EQ("Hello 2", response.text());
                  run_loop_2.QuitWhenIdle();
                }));
  HandleOneEchoRequest();
  run_loop_2.Run();
}

TEST_F(GrpcAsyncExecutorTest, SendTwoTextsAndRespondTogether) {
  base::RunLoop run_loop;
  size_t response_count = 0;
  auto on_received_one_response = [&]() {
    response_count++;
    if (response_count == 2) {
      run_loop.QuitWhenIdle();
    }
  };
  AsyncSendText("Hello 1",
                base::BindLambdaForTesting([&](const grpc::Status& status,
                                               const EchoResponse& response) {
                  EXPECT_TRUE(status.ok());
                  EXPECT_EQ("Hello 1", response.text());
                  on_received_one_response();
                }));
  AsyncSendText("Hello 2",
                base::BindLambdaForTesting([&](const grpc::Status& status,
                                               const EchoResponse& response) {
                  EXPECT_TRUE(status.ok());
                  EXPECT_EQ("Hello 2", response.text());
                  on_received_one_response();
                }));
  HandleOneEchoRequest();
  HandleOneEchoRequest();
  run_loop.Run();
}

TEST_F(GrpcAsyncExecutorTest,
       ControlGroup_RpcChannelStillOpenAfterRunLoopQuit) {
  base::RunLoop run_loop;
  AsyncSendText("Hello", base::BindLambdaForTesting(
                             [&](const grpc::Status&, const EchoResponse&) {
                               NOTREACHED();
                             }));
  EchoRequest request;
  auto responder = GetResponderAndFillEchoRequest(&request);
  run_loop.RunUntilIdle();
  ASSERT_TRUE(responder->Respond(EchoResponse(), grpc::Status::OK));
}

TEST_F(GrpcAsyncExecutorTest, RpcCanceledOnDestruction) {
  base::RunLoop run_loop;
  AsyncSendText("Hello", base::BindLambdaForTesting(
                             [&](const grpc::Status&, const EchoResponse&) {
                               NOTREACHED();
                             }));
  EchoRequest request;
  auto responder = GetResponderAndFillEchoRequest(&request);
  dispatcher_.reset();
  run_loop.RunUntilIdle();
  ASSERT_FALSE(responder->Respond(EchoResponse(), grpc::Status::OK));
}

TEST_F(GrpcAsyncExecutorTest, ServerStreamNotAcceptedByServer) {
  base::RunLoop run_loop;
  auto scoped_stream = StartEchoStream(
      "Hello", NotReachedStreamingCallback(),
      base::BindLambdaForTesting([&](const grpc::Status&) { NOTREACHED(); }));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        dispatcher_.reset();
        run_loop.QuitWhenIdle();
      }));
  run_loop.Run();
}

TEST_F(GrpcAsyncExecutorTest, ServerStreamImmediatelyClosedByServer) {
  base::RunLoop run_loop;
  auto scoped_stream =
      StartEchoStream("Hello", NotReachedStreamingCallback(),
                      test::CheckStatusThenQuitRunLoopCallback(
                          FROM_HERE, grpc::StatusCode::OK, &run_loop));
  auto responder = HandleEchoStream(FROM_HERE, "Hello");
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { responder.reset(); }));
  run_loop.Run();
}

TEST_F(GrpcAsyncExecutorTest, ServerStreamImmediatelyClosedByServerWithError) {
  base::RunLoop run_loop;
  auto scoped_stream = StartEchoStream(
      "Hello", NotReachedStreamingCallback(),
      test::CheckStatusThenQuitRunLoopCallback(
          FROM_HERE, grpc::StatusCode::UNAUTHENTICATED, &run_loop));
  auto responder = HandleEchoStream(FROM_HERE, "Hello");
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        responder->Close(grpc::Status(grpc::StatusCode::UNAUTHENTICATED, ""));
      }));
  run_loop.Run();
}

TEST_F(GrpcAsyncExecutorTest, ServerStreamsOneMessageThenClosedByServer) {
  base::RunLoop run_loop;
  std::unique_ptr<EchoStreamResponder> responder;
  auto scoped_stream = StartEchoStream(
      "Hello", base::BindLambdaForTesting([&](const EchoResponse& response) {
        ASSERT_EQ("Echo 1", response.text());
        responder.reset();
      }),
      test::CheckStatusThenQuitRunLoopCallback(FROM_HERE, grpc::StatusCode::OK,
                                               &run_loop));
  responder = HandleEchoStream(FROM_HERE, "Hello");
  responder->SendMessage(ResponseForText("Echo 1"));
  run_loop.Run();
}

TEST_F(GrpcAsyncExecutorTest, ServerStreamsTwoMessagesThenClosedByServer) {
  base::RunLoop run_loop;
  std::unique_ptr<EchoStreamResponder> responder;

  base::MockCallback<base::RepeatingCallback<void(const EchoResponse&)>>
      mock_on_incoming_msg;

  {
    InSequence sequence;
    EXPECT_CALL(mock_on_incoming_msg,
                Run(Property(&EchoResponse::text, "Echo 1")))
        .WillOnce(Invoke([&](const EchoResponse&) {
          responder->SendMessage(ResponseForText("Echo 2"));
        }));
    EXPECT_CALL(mock_on_incoming_msg,
                Run(Property(&EchoResponse::text, "Echo 2")))
        .WillOnce(Invoke([&](const EchoResponse&) { responder.reset(); }));
  }

  auto scoped_stream =
      StartEchoStream("Hello", mock_on_incoming_msg.Get(),
                      test::CheckStatusThenQuitRunLoopCallback(
                          FROM_HERE, grpc::StatusCode::OK, &run_loop));
  responder = HandleEchoStream(FROM_HERE, "Hello");
  ASSERT_TRUE(responder->SendMessage(ResponseForText("Echo 1")));
  run_loop.Run();
}

TEST_F(GrpcAsyncExecutorTest,
       ControlGroup_ServerStreamStillOpenAfterRunLoopQuit) {
  base::RunLoop run_loop;
  auto scoped_stream = StartEchoStream("Hello", NotReachedStreamingCallback(),
                                       NotReachedStatusCallback());
  auto responder = HandleEchoStream(FROM_HERE, "Hello");
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindLambdaForTesting([&]() { run_loop.QuitWhenIdle(); }));
  run_loop.Run();
  ASSERT_TRUE(responder->SendMessage(ResponseForText("Echo 1")));
}

TEST_F(GrpcAsyncExecutorTest, ServerStreamOpenThenClosedByClientAtDestruction) {
  base::RunLoop run_loop;
  auto scoped_stream = StartEchoStream("Hello", NotReachedStreamingCallback(),
                                       NotReachedStatusCallback());
  auto responder = HandleEchoStream(FROM_HERE, "Hello");
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        dispatcher_.reset();
        run_loop.QuitWhenIdle();
      }));
  run_loop.Run();
  ASSERT_FALSE(responder->SendMessage(ResponseForText("Echo 1")));
}

TEST_F(GrpcAsyncExecutorTest, ServerStreamClosedByStreamHolder) {
  base::RunLoop run_loop;
  auto scoped_stream = StartEchoStream("Hello", NotReachedStreamingCallback(),
                                       NotReachedStatusCallback());
  auto responder = HandleEchoStream(FROM_HERE, "Hello");
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        scoped_stream.reset();
        run_loop.QuitWhenIdle();
      }));
  run_loop.Run();
  ASSERT_FALSE(responder->SendMessage(ResponseForText("Echo 1")));
}

TEST_F(GrpcAsyncExecutorTest, ServerStreamsOneMessageThenClosedByStreamHolder) {
  base::RunLoop run_loop;
  std::unique_ptr<ScopedGrpcServerStream> scoped_stream = StartEchoStream(
      "Hello", base::BindLambdaForTesting([&](const EchoResponse& response) {
        ASSERT_EQ("Echo 1", response.text());
        scoped_stream.reset();
        run_loop.QuitWhenIdle();
      }),
      NotReachedStatusCallback());
  auto responder = HandleEchoStream(FROM_HERE, "Hello");
  ASSERT_TRUE(responder->SendMessage(ResponseForText("Echo 1")));
  run_loop.Run();
  ASSERT_FALSE(responder->SendMessage(ResponseForText("Echo 2")));
}

}  // namespace remoting
