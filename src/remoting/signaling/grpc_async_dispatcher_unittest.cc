// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/grpc_async_dispatcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "remoting/signaling/grpc_async_dispatcher_test_services.grpc.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"

namespace remoting {

namespace {

class EchoServerImpl {
 public:
  EchoServerImpl();
  ~EchoServerImpl();

  void Start();
  std::shared_ptr<grpc::Channel> CreateInProcessChannel();
  void HandleOneRequest();

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

void EchoServerImpl::HandleOneRequest() {
  grpc::ServerContext context;
  EchoRequest request;
  grpc::ServerAsyncResponseWriter<EchoResponse> responder(&context);
  async_service_.RequestEcho(&context, &request, &responder,
                             completion_queue_.get(), completion_queue_.get(),
                             (void*)1);

  void* tag;
  bool ok;

  completion_queue_->Next(&tag, &ok);
  ASSERT_TRUE(ok);
  ASSERT_EQ((void*)1, tag);

  EchoResponse response;
  response.set_text(request.text());
  responder.Finish(response, grpc::Status::OK, (void*)2);

  completion_queue_->Next(&tag, &ok);
  ASSERT_TRUE(ok);
  ASSERT_EQ((void*)2, tag);
}

}  // namespace

class GrpcAsyncDispatcherTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

 protected:
  void AsyncSendText(const std::string& text,
                     GrpcAsyncDispatcher::RpcCallback<EchoResponse> callback);

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

TEST_F(GrpcAsyncDispatcherTest, DoNothing) {}

TEST_F(GrpcAsyncDispatcherTest, SendOneTextAndRespond) {
  base::RunLoop run_loop;
  AsyncSendText("Hello",
                base::BindLambdaForTesting(
                    [&](grpc::Status status, const EchoResponse& response) {
                      EXPECT_TRUE(status.ok());
                      EXPECT_EQ("Hello", response.text());
                      run_loop.Quit();
                    }));
  server_->HandleOneRequest();
  run_loop.Run();
}

TEST_F(GrpcAsyncDispatcherTest, SendTwoTextsAndRespondOneByOne) {
  base::RunLoop run_loop_1;
  AsyncSendText("Hello 1",
                base::BindLambdaForTesting(
                    [&](grpc::Status status, const EchoResponse& response) {
                      EXPECT_TRUE(status.ok());
                      EXPECT_EQ("Hello 1", response.text());
                      run_loop_1.Quit();
                    }));
  server_->HandleOneRequest();
  run_loop_1.Run();

  base::RunLoop run_loop_2;
  AsyncSendText("Hello 2",
                base::BindLambdaForTesting(
                    [&](grpc::Status status, const EchoResponse& response) {
                      EXPECT_TRUE(status.ok());
                      EXPECT_EQ("Hello 2", response.text());
                      run_loop_2.Quit();
                    }));
  server_->HandleOneRequest();
  run_loop_2.Run();
}

TEST_F(GrpcAsyncDispatcherTest, SendTwoTextsAndRespondTogether) {
  base::RunLoop run_loop;
  size_t response_count = 0;
  auto on_received_one_response = [&]() {
    response_count++;
    if (response_count == 2) {
      run_loop.Quit();
    }
  };
  AsyncSendText("Hello 1",
                base::BindLambdaForTesting(
                    [&](grpc::Status status, const EchoResponse& response) {
                      EXPECT_TRUE(status.ok());
                      EXPECT_EQ("Hello 1", response.text());
                      on_received_one_response();
                    }));
  AsyncSendText("Hello 2",
                base::BindLambdaForTesting(
                    [&](grpc::Status status, const EchoResponse& response) {
                      EXPECT_TRUE(status.ok());
                      EXPECT_EQ("Hello 2", response.text());
                      on_received_one_response();
                    }));
  server_->HandleOneRequest();
  server_->HandleOneRequest();
  run_loop.Run();
}

TEST_F(GrpcAsyncDispatcherTest, RpcCanceledOnDestruction) {
  base::RunLoop run_loop;
  AsyncSendText("Hello",
                base::BindLambdaForTesting([&](grpc::Status status,
                                               const EchoResponse& response) {
                  EXPECT_EQ(grpc::StatusCode::CANCELLED, status.error_code());
                  run_loop.Quit();
                }));
  dispatcher_.reset();
  run_loop.Run();
}

}  // namespace remoting
