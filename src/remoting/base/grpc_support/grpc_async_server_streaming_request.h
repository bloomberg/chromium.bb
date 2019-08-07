// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_GRPC_SUPPORT_GRPC_ASYNC_SERVER_STREAMING_REQUEST_H_
#define REMOTING_BASE_GRPC_SUPPORT_GRPC_ASYNC_SERVER_STREAMING_REQUEST_H_

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/base/grpc_support/grpc_async_request.h"
#include "remoting/base/grpc_support/scoped_grpc_server_stream.h"
#include "third_party/grpc/src/include/grpcpp/support/async_stream.h"

namespace remoting {

template <typename RequestType, typename ResponseType>
using GrpcAsyncServerStreamingRpcFunction =
    base::OnceCallback<std::unique_ptr<grpc::ClientAsyncReader<ResponseType>>(
        grpc::ClientContext*,
        const RequestType&,
        grpc::CompletionQueue*,
        void*)>;

// GrpcAsyncRequest implementation for server streaming call. The object is
// first enqueued for starting the stream, then kept being re-enqueued to
// receive a new message, until it's canceled by calling CancelRequest().
class GrpcAsyncServerStreamingRequestBase : public GrpcAsyncRequest {
 public:
  GrpcAsyncServerStreamingRequestBase(
      std::unique_ptr<grpc::ClientContext> context,
      base::OnceCallback<void(const grpc::Status&)> on_channel_closed,
      std::unique_ptr<ScopedGrpcServerStream>* scoped_stream);
  ~GrpcAsyncServerStreamingRequestBase() override;

 protected:
  enum class State {
    STARTING,
    STREAMING,

    // Server has closed the stream and we are getting back the reason.
    FINISHING,

    CLOSED,
  };

  virtual void ResolveIncomingMessage() = 0;
  virtual void WaitForIncomingMessage(void* event_tag) = 0;
  virtual void FinishStream(void* event_tag) = 0;

  RunTaskCallback run_task_callback_;

 private:
  // GrpcAsyncRequest implementations.
  bool OnDequeue(bool operation_succeeded) override;
  void Reenqueue(void* event_tag) override;
  void OnRequestCanceled() override;
  bool CanStartRequest() const override;
  void ResolveChannelClosed();

  base::OnceCallback<void(const grpc::Status&)> on_channel_closed_;
  State state_ = State::STARTING;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<GrpcAsyncServerStreamingRequestBase> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(GrpcAsyncServerStreamingRequestBase);
};

template <typename ResponseType>
class GrpcAsyncServerStreamingRequest
    : public GrpcAsyncServerStreamingRequestBase {
 public:
  using OnIncomingMessageCallback =
      base::RepeatingCallback<void(const ResponseType&)>;
  using StartAndCreateReaderCallback =
      base::OnceCallback<std::unique_ptr<grpc::ClientAsyncReader<ResponseType>>(
          grpc::CompletionQueue* cq,
          void* event_tag)>;

  GrpcAsyncServerStreamingRequest(
      std::unique_ptr<grpc::ClientContext> context,
      StartAndCreateReaderCallback create_reader_callback,
      const OnIncomingMessageCallback& on_incoming_msg,
      base::OnceCallback<void(const grpc::Status&)> on_channel_closed,
      std::unique_ptr<ScopedGrpcServerStream>* scoped_stream)
      : GrpcAsyncServerStreamingRequestBase(std::move(context),
                                            std::move(on_channel_closed),
                                            scoped_stream) {
    create_reader_callback_ = std::move(create_reader_callback);
    on_incoming_msg_ = on_incoming_msg;
  }

  ~GrpcAsyncServerStreamingRequest() override = default;

 private:
  // GrpcAsyncRequest implementations
  void Start(const RunTaskCallback& run_task_cb,
             grpc::CompletionQueue* cq,
             void* event_tag) override {
    reader_ = std::move(create_reader_callback_).Run(cq, event_tag);
    run_task_callback_ = run_task_cb;
  }

  // GrpcAsyncServerStreamingRequestBase implementations.
  void ResolveIncomingMessage() override {
    DCHECK(run_task_callback_);
    run_task_callback_.Run(base::BindOnce(on_incoming_msg_, response_));
  }

  void WaitForIncomingMessage(void* event_tag) override {
    DCHECK(reader_);
    reader_->Read(&response_, event_tag);
  }

  void FinishStream(void* event_tag) override {
    DCHECK(reader_);
    reader_->Finish(&status_, event_tag);
  }

  StartAndCreateReaderCallback create_reader_callback_;
  ResponseType response_;
  std::unique_ptr<grpc::ClientAsyncReader<ResponseType>> reader_;
  OnIncomingMessageCallback on_incoming_msg_;

  DISALLOW_COPY_AND_ASSIGN(GrpcAsyncServerStreamingRequest);
};

// Creates a server streaming request.
// |rpc_function| is called once GrpcExecutor is about to send out the request.
// |on_incoming_msg| is called once a message is streamed from the server.
// |on_channel_closed| is called once the channel is closed remotely by the
// server.
// |scoped_stream| is set with an object which upon destruction will cancel the
// stream.
template <typename RequestType, typename ResponseType>
std::unique_ptr<GrpcAsyncServerStreamingRequest<ResponseType>>
CreateGrpcAsyncServerStreamingRequest(
    GrpcAsyncServerStreamingRpcFunction<RequestType, ResponseType> rpc_function,
    std::unique_ptr<grpc::ClientContext> context,
    const RequestType& request,
    const base::RepeatingCallback<void(const ResponseType&)>& on_incoming_msg,
    base::OnceCallback<void(const grpc::Status&)> on_channel_closed,
    std::unique_ptr<ScopedGrpcServerStream>* scoped_stream) {
  auto start_and_create_reader_cb =
      base::BindOnce(std::move(rpc_function), context.get(), request);
  return std::make_unique<GrpcAsyncServerStreamingRequest<ResponseType>>(
      std::move(context), std::move(start_and_create_reader_cb),
      on_incoming_msg, std::move(on_channel_closed), scoped_stream);
}

}  // namespace remoting

#endif  // REMOTING_BASE_GRPC_SUPPORT_GRPC_ASYNC_SERVER_STREAMING_REQUEST_H_
