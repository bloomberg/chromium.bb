// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/grpc_support/grpc_async_dispatcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "remoting/signaling/grpc_support/grpc_async_dispatcher_test_services.grpc.pb.h"
#include "remoting/signaling/grpc_support/grpc_async_test_server.h"
#include "remoting/signaling/grpc_support/grpc_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"

namespace remoting {

namespace {

using EchoStreamResponder = test::GrpcServerStreamResponder<EchoResponse>;

base::RepeatingCallback<void(const EchoResponse&)>
NotReachedStreamingCallback() {
  return base::BindRepeating([](const EchoResponse&) { NOTREACHED(); });
}

EchoResponse ResponseForText(const std::string& text) {
  EchoResponse response;
  response.set_text(text);
  return response;
}

}  // namespace

class GrpcAsyncDispatcherTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

 protected:
  void AsyncSendText(const std::string& text,
                     GrpcAsyncDispatcher::RpcCallback<EchoResponse> callback);
  std::unique_ptr<ScopedGrpcServerStream> StartEchoStream(
      const std::string& request_text,
      const GrpcAsyncDispatcher::RpcStreamCallback<EchoResponse>
          on_incoming_msg,
      GrpcAsyncDispatcher::RpcChannelClosedCallback on_channel_closed);

 protected:
  void HandleOneEchoRequest();
  std::unique_ptr<EchoStreamResponder> HandleEchoStream(
      const base::Location& from_here,
      const std::string& expected_request_text);

  std::unique_ptr<GrpcAsyncDispatcher> dispatcher_;

 private:
  base::MessageLoop message_loop_;
  std::unique_ptr<GrpcAsyncDispatcherTestService::Stub> stub_;
  std::unique_ptr<test::GrpcAsyncTestServer> server_;
};

void GrpcAsyncDispatcherTest::SetUp() {
  dispatcher_ = std::make_unique<GrpcAsyncDispatcher>();
  server_ = std::make_unique<test::GrpcAsyncTestServer>(
      std::make_unique<GrpcAsyncDispatcherTestService::AsyncService>());
  stub_ = GrpcAsyncDispatcherTestService::NewStub(
      server_->CreateInProcessChannel());
}

void GrpcAsyncDispatcherTest::TearDown() {
  server_.reset();
  dispatcher_.reset();
  stub_.reset();
}

void GrpcAsyncDispatcherTest::AsyncSendText(
    const std::string& text,
    GrpcAsyncDispatcher::RpcCallback<EchoResponse> callback) {
  EchoRequest request;
  request.set_text(text);
  dispatcher_->ExecuteAsyncRpc(
      base::BindOnce(&GrpcAsyncDispatcherTestService::Stub::AsyncEcho,
                     base::Unretained(stub_.get())),
      std::make_unique<grpc::ClientContext>(), request, std::move(callback));
}

std::unique_ptr<ScopedGrpcServerStream>
GrpcAsyncDispatcherTest::StartEchoStream(
    const std::string& request_text,
    const GrpcAsyncDispatcher::RpcStreamCallback<EchoResponse> on_incoming_msg,
    GrpcAsyncDispatcher::RpcChannelClosedCallback on_channel_closed) {
  EchoRequest request;
  request.set_text(request_text);
  return dispatcher_->ExecuteAsyncServerStreamingRpc(
      base::BindOnce(&GrpcAsyncDispatcherTestService::Stub::AsyncStreamEcho,
                     base::Unretained(stub_.get())),
      std::make_unique<grpc::ClientContext>(), request, on_incoming_msg,
      std::move(on_channel_closed));
}

void GrpcAsyncDispatcherTest::HandleOneEchoRequest() {
  EchoRequest request;
  auto responder = server_->HandleRequest(
      &GrpcAsyncDispatcherTestService::AsyncService::RequestEcho, &request);
  EchoResponse response;
  response.set_text(request.text());
  responder->Respond(response, grpc::Status::OK);
}

std::unique_ptr<EchoStreamResponder> GrpcAsyncDispatcherTest::HandleEchoStream(
    const base::Location& from_here,
    const std::string& expected_request_text) {
  EchoRequest request;
  auto responder = server_->HandleStreamRequest(
      &GrpcAsyncDispatcherTestService::AsyncService::RequestStreamEcho,
      &request);
  EXPECT_EQ(expected_request_text, request.text())
      << "Request text mismatched. Location: " << from_here.ToString();
  return responder;
}

TEST_F(GrpcAsyncDispatcherTest, DoNothing) {}

TEST_F(GrpcAsyncDispatcherTest, SendOneTextAndRespond) {
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

TEST_F(GrpcAsyncDispatcherTest, SendTwoTextsAndRespondOneByOne) {
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

TEST_F(GrpcAsyncDispatcherTest, SendTwoTextsAndRespondTogether) {
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

TEST_F(GrpcAsyncDispatcherTest, RpcCanceledOnDestruction) {
  base::RunLoop run_loop;
  AsyncSendText("Hello",
                base::BindLambdaForTesting([&](const grpc::Status& status,
                                               const EchoResponse& response) {
                  EXPECT_EQ(grpc::StatusCode::CANCELLED, status.error_code());
                  run_loop.QuitWhenIdle();
                }));
  dispatcher_.reset();
  run_loop.Run();
}

TEST_F(GrpcAsyncDispatcherTest, ServerStreamNotAcceptedByServer) {
  base::RunLoop run_loop;
  auto scoped_stream =
      StartEchoStream("Hello", NotReachedStreamingCallback(),
                      test::CheckStatusThenQuitRunLoopCallback(
                          FROM_HERE, grpc::StatusCode::CANCELLED, &run_loop));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { dispatcher_.reset(); }));
  run_loop.Run();
}

TEST_F(GrpcAsyncDispatcherTest, ServerStreamImmediatelyClosedByServer) {
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

TEST_F(GrpcAsyncDispatcherTest,
       ServerStreamImmediatelyClosedByServerWithError) {
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

TEST_F(GrpcAsyncDispatcherTest, ServerStreamsOneMessageThenClosedByServer) {
  base::RunLoop run_loop;
  std::unique_ptr<EchoStreamResponder> responder;
  auto scoped_stream = StartEchoStream(
      "Hello", base::BindLambdaForTesting([&](const EchoResponse& response) {
        ASSERT_EQ("Echo 1", response.text());
        responder->OnClientReceivedMessage();
        responder.reset();
      }),
      test::CheckStatusThenQuitRunLoopCallback(FROM_HERE, grpc::StatusCode::OK,
                                               &run_loop));
  responder = HandleEchoStream(FROM_HERE, "Hello");
  responder->SendMessage(ResponseForText("Echo 1"));
  run_loop.Run();
}

TEST_F(GrpcAsyncDispatcherTest, ServerStreamsTwoMessagesThenClosedByServer) {
  base::RunLoop run_loop;
  std::unique_ptr<EchoStreamResponder> responder;
  int received_messages_count = 0;
  auto scoped_stream = StartEchoStream(
      "Hello", base::BindLambdaForTesting([&](const EchoResponse& response) {
        if (received_messages_count == 0) {
          ASSERT_EQ("Echo 1", response.text());
          responder->OnClientReceivedMessage();
          responder->SendMessage(ResponseForText("Echo 2"));
          received_messages_count++;
          return;
        }
        if (received_messages_count == 1) {
          ASSERT_EQ("Echo 2", response.text());
          responder->OnClientReceivedMessage();
          responder.reset();
          received_messages_count++;
          return;
        }
        NOTREACHED();
      }),
      test::CheckStatusThenQuitRunLoopCallback(FROM_HERE, grpc::StatusCode::OK,
                                               &run_loop));
  responder = HandleEchoStream(FROM_HERE, "Hello");
  responder->SendMessage(ResponseForText("Echo 1"));
  run_loop.Run();
  ASSERT_EQ(2, received_messages_count);
}

TEST_F(GrpcAsyncDispatcherTest,
       ServerStreamOpenThenClosedByClientAtDestruction) {
  base::RunLoop run_loop;
  auto scoped_stream =
      StartEchoStream("Hello", NotReachedStreamingCallback(),
                      test::CheckStatusThenQuitRunLoopCallback(
                          FROM_HERE, grpc::StatusCode::CANCELLED, &run_loop));
  auto responder = HandleEchoStream(FROM_HERE, "Hello");
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { dispatcher_.reset(); }));
  run_loop.Run();
}

TEST_F(GrpcAsyncDispatcherTest, ServerStreamClosedByStreamHolder) {
  base::RunLoop run_loop;
  std::unique_ptr<EchoStreamResponder> responder;
  std::unique_ptr<ScopedGrpcServerStream> scoped_stream =
      StartEchoStream("Hello", NotReachedStreamingCallback(),
                      test::CheckStatusThenQuitRunLoopCallback(
                          FROM_HERE, grpc::StatusCode::CANCELLED, &run_loop));
  responder = HandleEchoStream(FROM_HERE, "Hello");
  scoped_stream.reset();
  run_loop.Run();
}

TEST_F(GrpcAsyncDispatcherTest,
       ServerStreamsOneMessageThenClosedByStreamHolder) {
  base::RunLoop run_loop;
  std::unique_ptr<EchoStreamResponder> responder;
  std::unique_ptr<ScopedGrpcServerStream> scoped_stream = StartEchoStream(
      "Hello", base::BindLambdaForTesting([&](const EchoResponse& response) {
        ASSERT_EQ("Echo 1", response.text());
        responder->OnClientReceivedMessage();
        scoped_stream.reset();
      }),
      test::CheckStatusThenQuitRunLoopCallback(
          FROM_HERE, grpc::StatusCode::CANCELLED, &run_loop));
  responder = HandleEchoStream(FROM_HERE, "Hello");
  responder->SendMessage(ResponseForText("Echo 1"));
  run_loop.Run();
}

}  // namespace remoting
