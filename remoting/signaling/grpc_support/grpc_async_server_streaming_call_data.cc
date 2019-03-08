// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/grpc_support/grpc_async_server_streaming_call_data.h"

#include "base/bind.h"
#include "remoting/signaling/grpc_support/scoped_grpc_server_stream.h"

namespace remoting {
namespace internal {

GrpcAsyncServerStreamingCallDataBase::GrpcAsyncServerStreamingCallDataBase(
    std::unique_ptr<grpc::ClientContext> context,
    base::OnceCallback<void(const grpc::Status&)> on_channel_closed)
    : GrpcAsyncCallData(std::move(context)), weak_factory_(this) {
  DCHECK(on_channel_closed);
  on_channel_closed_ = std::move(on_channel_closed);
}

GrpcAsyncServerStreamingCallDataBase::~GrpcAsyncServerStreamingCallDataBase() =
    default;

bool GrpcAsyncServerStreamingCallDataBase::OnDequeuedOnDispatcherThread(
    bool operation_succeeded) {
  base::AutoLock autolock(state_lock_);
  if (state_ == State::CLOSED) {
    return false;
  }
  if (state_ == State::FINISHING) {
    DCHECK(operation_succeeded);
    state_ = State::CLOSED;
    ResolveChannelClosed();
    return false;
  }
  if (!operation_succeeded) {
    VLOG(0) << "Can't read any more data. Figuring out the reason..."
            << " Streaming call: " << this;
    state_ = State::FINISHING;
    FinishStream();
    return true;
  }
  if (state_ == State::STARTING) {
    VLOG(0) << "Streaming call started: " << this;
    state_ = State::STREAMING;
    WaitForIncomingMessage();
    return true;
  }
  DCHECK_EQ(State::STREAMING, state_);
  VLOG(0) << "Streaming call received message: " << this;
  ResolveIncomingMessage();
  WaitForIncomingMessage();
  return true;
}

std::unique_ptr<ScopedGrpcServerStream>
GrpcAsyncServerStreamingCallDataBase::CreateStreamHolder() {
  return std::make_unique<ScopedGrpcServerStream>(weak_factory_.GetWeakPtr());
}

void GrpcAsyncServerStreamingCallDataBase::OnRequestCanceled() {
  base::AutoLock autolock(state_lock_);
  if (state_ == State::CLOSED) {
    return;
  }
  state_ = State::CLOSED;
  status_ = grpc::Status::CANCELLED;
  ResolveChannelClosed();
}

void GrpcAsyncServerStreamingCallDataBase::ResolveChannelClosed() {
  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_channel_closed_), status_));
}

}  // namespace internal
}  // namespace remoting
