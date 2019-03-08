// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_SERVER_STREAMING_CALL_DATA_H_
#define REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_SERVER_STREAMING_CALL_DATA_H_

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "remoting/signaling/grpc_support/grpc_async_call_data.h"
#include "third_party/grpc/src/include/grpcpp/support/async_stream.h"

namespace remoting {

class ScopedGrpcServerStream;

namespace internal {

// GrpcAsyncCallData implementation for server streaming call. The object is
// first enqueued for starting the stream, then kept being re-enqueued to
// receive a new message, until it's canceled by calling CancelRequest().
class GrpcAsyncServerStreamingCallDataBase : public GrpcAsyncCallData {
 public:
  GrpcAsyncServerStreamingCallDataBase(
      std::unique_ptr<grpc::ClientContext> context,
      base::OnceCallback<void(const grpc::Status&)> on_channel_closed);
  ~GrpcAsyncServerStreamingCallDataBase() override;

  // GrpcAsyncCallData implementations.
  bool OnDequeuedOnDispatcherThread(bool operation_succeeded) override;

  std::unique_ptr<ScopedGrpcServerStream> CreateStreamHolder();

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

  // GrpcAsyncCallData implementations.
  void OnRequestCanceled() override;

  base::Lock state_lock_;
  State state_ GUARDED_BY(state_lock_) = State::STARTING;

 private:
  void ResolveChannelClosed();

  base::OnceCallback<void(const grpc::Status&)> on_channel_closed_;

  base::WeakPtrFactory<GrpcAsyncServerStreamingCallDataBase> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(GrpcAsyncServerStreamingCallDataBase);
};

template <typename ResponseType>
class GrpcAsyncServerStreamingCallData
    : public GrpcAsyncServerStreamingCallDataBase {
 public:
  using OnIncomingMessageCallback =
      base::RepeatingCallback<void(const ResponseType&)>;

  GrpcAsyncServerStreamingCallData(
      std::unique_ptr<grpc::ClientContext> context,
      const OnIncomingMessageCallback& on_incoming_msg,
      base::OnceCallback<void(const grpc::Status&)> on_channel_closed)
      : GrpcAsyncServerStreamingCallDataBase(std::move(context),
                                             std::move(on_channel_closed)) {
    on_incoming_msg_ = on_incoming_msg;
  }
  ~GrpcAsyncServerStreamingCallData() override = default;

  void Initialize(
      std::unique_ptr<grpc::ClientAsyncReader<ResponseType>> reader) {
    reader_ = std::move(reader);
  }

 protected:
  // GrpcAsyncServerStreamingCallDataBase implementations.
  void ResolveIncomingMessage() override {
    caller_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(on_incoming_msg_, response_));
  }

  void WaitForIncomingMessage() override {
    DCHECK(reader_);
    reader_->Read(&response_, /* event_tag */ this);
  }

  void FinishStream() override {
    DCHECK(reader_);
    reader_->Finish(&status_, /* event_tag */ this);
  }

 private:
  ResponseType response_;
  std::unique_ptr<grpc::ClientAsyncReader<ResponseType>> reader_;
  OnIncomingMessageCallback on_incoming_msg_;

  DISALLOW_COPY_AND_ASSIGN(GrpcAsyncServerStreamingCallData);
};

}  // namespace internal
}  // namespace remoting

#endif  // REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_SERVER_STREAMING_CALL_DATA_H_
