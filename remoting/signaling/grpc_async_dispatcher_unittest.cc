// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/grpc_async_dispatcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "remoting/signaling/grpc_async_dispatcher_test_services.grpc.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"

namespace remoting {

namespace {

void* TagForInt(int num) {
  return reinterpret_cast<void*>(num);
}

base::RepeatingCallback<void(const EchoResponse&)>
NotReachedStreamingCallback() {
  return base::BindRepeating([](const EchoResponse&) { NOTREACHED(); });
}

GrpcAsyncDispatcher::RpcChannelClosedCallback
CheckStatusThenQuitRunLoopCallback(grpc::StatusCode expected_status_code,
                                   base::RunLoop* run_loop) {
  return base::BindLambdaForTesting([=](const grpc::Status& status) {
    ASSERT_EQ(expected_status_code, status.error_code());
    run_loop->QuitWhenIdle();
  });
}

class EchoStream {
 public:
  EchoStream(std::unique_ptr<grpc::ServerContext> context,
             grpc::ServerCompletionQueue* completion_queue,
             std::unique_ptr<grpc::ServerAsyncWriter<EchoResponse>> writer);
  ~EchoStream();

  // SendEcho() must be followed by a call to OnClientReceivedEcho().
  void SendEcho(const std::string& text);
  void OnClientReceivedEcho();
  void Close(const grpc::Status& status);

 private:
  std::unique_ptr<grpc::ServerContext> context_;
  grpc::ServerCompletionQueue* completion_queue_;
  std::unique_ptr<grpc::ServerAsyncWriter<EchoResponse>> writer_;
  bool closed_ = false;
};

EchoStream::EchoStream(
    std::unique_ptr<grpc::ServerContext> context,
    grpc::ServerCompletionQueue* completion_queue,
    std::unique_ptr<grpc::ServerAsyncWriter<EchoResponse>> writer)
    : context_(std::move(context)),
      completion_queue_(completion_queue),
      writer_(std::move(writer)) {}

EchoStream::~EchoStream() {
  Close(grpc::Status::OK);
}

void EchoStream::SendEcho(const std::string& text) {
  EchoResponse response;
  response.set_text(text);
  writer_->Write(response, this);
}

void EchoStream::OnClientReceivedEcho() {
  void* tag;
  bool ok;
  completion_queue_->Next(&tag, &ok);
  ASSERT_TRUE(ok);
  ASSERT_EQ(this, tag);
}

void EchoStream::Close(const grpc::Status& status) {
  if (closed_) {
    return;
  }

  writer_->Finish(status, this);

  void* tag;
  bool ok;
  completion_queue_->Next(&tag, &ok);
  if (!ok) {
    LOG(WARNING) << "Failed to finish stream. Connection might be dropped.";
  }
  ASSERT_EQ(this, tag);
  closed_ = true;
}

// EchoStream

class EchoServerImpl {
 public:
  EchoServerImpl();
  ~EchoServerImpl();

  void Start();
  std::shared_ptr<grpc::Channel> CreateInProcessChannel();
  void HandleOneEchoRequest();
  std::unique_ptr<EchoStream> AcceptEchoStream(
      const std::string& expected_request_text);

 private:
  GrpcAsyncDispatcherTestService::AsyncService async_service_;
  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<grpc::ServerCompletionQueue> completion_queue_;
};

EchoServerImpl::EchoServerImpl() = default;

EchoServerImpl::~EchoServerImpl() {
  server_->Shutdown();
  completion_queue_->Shutdown();

  // gRPC requires draining the completion queue before destroying it.
  void* tag;
  bool ok;
  while (completion_queue_->Next(&tag, &ok)) {
  }
}

void EchoServerImpl::Start() {
  DCHECK(!server_);
  grpc::ServerBuilder builder;
  builder.RegisterService(&async_service_);
  completion_queue_ = builder.AddCompletionQueue();
  server_ = builder.BuildAndStart();
}

std::shared_ptr<grpc::Channel> EchoServerImpl::CreateInProcessChannel() {
  return server_->InProcessChannel(grpc::ChannelArguments());
}

void EchoServerImpl::HandleOneEchoRequest() {
  grpc::ServerContext context;
  EchoRequest request;
  grpc::ServerAsyncResponseWriter<EchoResponse> responder(&context);
  async_service_.RequestEcho(&context, &request, &responder,
                             completion_queue_.get(), completion_queue_.get(),
                             TagForInt(1));

  void* tag;
  bool ok;

  completion_queue_->Next(&tag, &ok);
  ASSERT_TRUE(ok);
  ASSERT_EQ(TagForInt(1), tag);

  EchoResponse response;
  response.set_text(request.text());
  responder.Finish(response, grpc::Status::OK, TagForInt(2));

  completion_queue_->Next(&tag, &ok);
  ASSERT_TRUE(ok);
  ASSERT_EQ(TagForInt(2), tag);
}

std::unique_ptr<EchoStream> EchoServerImpl::AcceptEchoStream(
    const std::string& expected_request_text) {
  auto context = std::make_unique<grpc::ServerContext>();
  EchoRequest request;
  auto writer =
      std::make_unique<grpc::ServerAsyncWriter<EchoResponse>>(context.get());
  async_service_.RequestStreamEcho(context.get(), &request, writer.get(),
                                   completion_queue_.get(),
                                   completion_queue_.get(), TagForInt(3));

  void* tag;
  bool ok;
  completion_queue_->Next(&tag, &ok);
  EXPECT_TRUE(ok);
  EXPECT_EQ(TagForInt(3), tag);
  EXPECT_EQ(expected_request_text, request.text());

  return std::make_unique<EchoStream>(
      std::move(context), completion_queue_.get(), std::move(writer));
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

  std::unique_ptr<EchoServerImpl> server_;

 protected:
  std::unique_ptr<GrpcAsyncDispatcher> dispatcher_;

 private:
  base::MessageLoop message_loop_;
  std::unique_ptr<GrpcAsyncDispatcherTestService::Stub> stub_;
};

void GrpcAsyncDispatcherTest::SetUp() {
  dispatcher_ = std::make_unique<GrpcAsyncDispatcher>();
  server_ = std::make_unique<EchoServerImpl>();
  server_->Start();
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
  server_->HandleOneEchoRequest();
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
  server_->HandleOneEchoRequest();
  run_loop_1.Run();

  base::RunLoop run_loop_2;
  AsyncSendText("Hello 2",
                base::BindLambdaForTesting([&](const grpc::Status& status,
                                               const EchoResponse& response) {
                  EXPECT_TRUE(status.ok());
                  EXPECT_EQ("Hello 2", response.text());
                  run_loop_2.QuitWhenIdle();
                }));
  server_->HandleOneEchoRequest();
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
  server_->HandleOneEchoRequest();
  server_->HandleOneEchoRequest();
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
                      CheckStatusThenQuitRunLoopCallback(
                          grpc::StatusCode::CANCELLED, &run_loop));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { dispatcher_.reset(); }));
  run_loop.Run();
}

TEST_F(GrpcAsyncDispatcherTest, ServerStreamImmediatelyClosedByServer) {
  base::RunLoop run_loop;
  auto scoped_stream = StartEchoStream(
      "Hello", NotReachedStreamingCallback(),
      CheckStatusThenQuitRunLoopCallback(grpc::StatusCode::OK, &run_loop));
  std::unique_ptr<EchoStream> stream = server_->AcceptEchoStream("Hello");
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { stream.reset(); }));
  run_loop.Run();
}

TEST_F(GrpcAsyncDispatcherTest,
       ServerStreamImmediatelyClosedByServerWithError) {
  base::RunLoop run_loop;
  auto scoped_stream =
      StartEchoStream("Hello", NotReachedStreamingCallback(),
                      CheckStatusThenQuitRunLoopCallback(
                          grpc::StatusCode::UNAUTHENTICATED, &run_loop));
  std::unique_ptr<EchoStream> stream = server_->AcceptEchoStream("Hello");
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        stream->Close(grpc::Status(grpc::StatusCode::UNAUTHENTICATED, ""));
      }));
  run_loop.Run();
}

TEST_F(GrpcAsyncDispatcherTest, ServerStreamsOneMessageThenClosedByServer) {
  base::RunLoop run_loop;
  std::unique_ptr<EchoStream> stream;
  auto scoped_stream = StartEchoStream(
      "Hello", base::BindLambdaForTesting([&](const EchoResponse& response) {
        ASSERT_EQ("Echo 1", response.text());
        stream->OnClientReceivedEcho();
        stream.reset();
      }),
      CheckStatusThenQuitRunLoopCallback(grpc::StatusCode::OK, &run_loop));
  stream = server_->AcceptEchoStream("Hello");
  stream->SendEcho("Echo 1");
  run_loop.Run();
}

TEST_F(GrpcAsyncDispatcherTest, ServerStreamsTwoMessagesThenClosedByServer) {
  base::RunLoop run_loop;
  std::unique_ptr<EchoStream> stream;
  int received_messages_count = 0;
  auto scoped_stream = StartEchoStream(
      "Hello", base::BindLambdaForTesting([&](const EchoResponse& response) {
        if (received_messages_count == 0) {
          ASSERT_EQ("Echo 1", response.text());
          stream->OnClientReceivedEcho();
          stream->SendEcho("Echo 2");
          received_messages_count++;
          return;
        }
        if (received_messages_count == 1) {
          ASSERT_EQ("Echo 2", response.text());
          stream->OnClientReceivedEcho();
          stream.reset();
          received_messages_count++;
          return;
        }
        NOTREACHED();
      }),
      CheckStatusThenQuitRunLoopCallback(grpc::StatusCode::OK, &run_loop));
  stream = server_->AcceptEchoStream("Hello");
  stream->SendEcho("Echo 1");
  run_loop.Run();
  ASSERT_EQ(2, received_messages_count);
}

TEST_F(GrpcAsyncDispatcherTest,
       ServerStreamOpenThenClosedByClientAtDestruction) {
  base::RunLoop run_loop;
  auto scoped_stream =
      StartEchoStream("Hello", NotReachedStreamingCallback(),
                      CheckStatusThenQuitRunLoopCallback(
                          grpc::StatusCode::CANCELLED, &run_loop));
  std::unique_ptr<EchoStream> stream = server_->AcceptEchoStream("Hello");
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { dispatcher_.reset(); }));
  run_loop.Run();
}

TEST_F(GrpcAsyncDispatcherTest, ServerStreamClosedByStreamHolder) {
  base::RunLoop run_loop;
  std::unique_ptr<EchoStream> stream;
  std::unique_ptr<ScopedGrpcServerStream> scoped_stream =
      StartEchoStream("Hello", NotReachedStreamingCallback(),
                      CheckStatusThenQuitRunLoopCallback(
                          grpc::StatusCode::CANCELLED, &run_loop));
  stream = server_->AcceptEchoStream("Hello");
  scoped_stream.reset();
  run_loop.Run();
}

TEST_F(GrpcAsyncDispatcherTest,
       ServerStreamsOneMessageThenClosedByStreamHolder) {
  base::RunLoop run_loop;
  std::unique_ptr<EchoStream> stream;
  std::unique_ptr<ScopedGrpcServerStream> scoped_stream = StartEchoStream(
      "Hello", base::BindLambdaForTesting([&](const EchoResponse& response) {
        ASSERT_EQ("Echo 1", response.text());
        stream->OnClientReceivedEcho();
        scoped_stream.reset();
      }),
      CheckStatusThenQuitRunLoopCallback(grpc::StatusCode::CANCELLED,
                                         &run_loop));
  stream = server_->AcceptEchoStream("Hello");
  stream->SendEcho("Echo 1");
  run_loop.Run();
}

}  // namespace remoting
