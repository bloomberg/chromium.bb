// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_SERVER_STREAMING_REQUEST_H_
#define REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_SERVER_STREAMING_REQUEST_H_

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "remoting/signaling/grpc_support/grpc_async_request.h"
#include "third_party/grpc/src/include/grpcpp/support/async_stream.h"

namespace remoting {

class ScopedGrpcServerStream;

namespace internal {

// GrpcAsyncRequest implementation for server streaming call. The object is
// first enqueued for starting the stream, then kept being re-enqueued to
// receive a new message, until it's canceled by calling CancelRequest().
class GrpcAsyncServerStreamingRequestBase : public GrpcAsyncRequest {
 public:
  GrpcAsyncServerStreamingRequestBase(
      std::unique_ptr<grpc::ClientContext> context,
      base::OnceCallback<void(const grpc::Status&)> on_channel_closed);
  ~GrpcAsyncServerStreamingRequestBase() override;

  // GrpcAsyncRequest implementations.
  bool OnDequeuedOnDispatcherThreadInternal(bool operation_succeeded) override;

  std::unique_ptr<ScopedGrpcServerStream> CreateStreamHolder();

  // Helper method for subclass to run callback only when the weak pointer is
  // valid.
  void RunClosure(base::OnceClosure closure);

 protected:
  enum class State {
    STARTING,
    STREAMING,

    // Server has closed the stream and we are getting back the reason.
    FINISHING,

    CLOSED,
  };

  virtual void ResolveIncomingMessage() = 0;
  virtual void WaitForIncomingMessage() = 0;
  virtual void FinishStream() = 0;

  // GrpcAsyncRequest implementations.
  void OnRequestCanceled() override;

  base::Lock state_lock_;
  State state_ GUARDED_BY(state_lock_) = State::STARTING;
  base::WeakPtr<GrpcAsyncServerStreamingRequestBase> weak_ptr_;

 private:
  void ResolveChannelClosed();

  base::OnceCallback<void(const grpc::Status&)> on_channel_closed_;

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
          void* event_tag)>;

  GrpcAsyncServerStreamingRequest(
      std::unique_ptr<grpc::ClientContext> context,
      StartAndCreateReaderCallback create_reader_callback,
      const OnIncomingMessageCallback& on_incoming_msg,
      base::OnceCallback<void(const grpc::Status&)> on_channel_closed)
      : GrpcAsyncServerStreamingRequestBase(std::move(context),
                                            std::move(on_channel_closed)) {
    create_reader_callback_ = std::move(create_reader_callback);
    on_incoming_msg_ = on_incoming_msg;
  }
  ~GrpcAsyncServerStreamingRequest() override = default;

  // GrpcAsyncRequest implementations
  void StartInternal() override {
    reader_ = std::move(create_reader_callback_).Run(GetEventTag());
  }

 protected:
  // GrpcAsyncServerStreamingRequestBase implementations.
  void ResolveIncomingMessage() override {
    caller_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GrpcAsyncServerStreamingRequestBase::RunClosure,
                       weak_ptr_, base::BindOnce(on_incoming_msg_, response_)));
  }

  void WaitForIncomingMessage() override {
    DCHECK(reader_);
    reader_->Read(&response_, GetEventTag());
  }

  void FinishStream() override {
    DCHECK(reader_);
    reader_->Finish(&status_, GetEventTag());
  }

 private:
  StartAndCreateReaderCallback create_reader_callback_;
  ResponseType response_;
  std::unique_ptr<grpc::ClientAsyncReader<ResponseType>> reader_;
  OnIncomingMessageCallback on_incoming_msg_;

  DISALLOW_COPY_AND_ASSIGN(GrpcAsyncServerStreamingRequest);
};

}  // namespace internal
}  // namespace remoting

#endif  // REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_SERVER_STREAMING_REQUEST_H_
