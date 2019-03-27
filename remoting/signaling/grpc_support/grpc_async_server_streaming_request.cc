// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/grpc_support/grpc_async_server_streaming_request.h"

#include "base/bind.h"
#include "remoting/signaling/grpc_support/scoped_grpc_server_stream.h"

namespace remoting {
namespace internal {

GrpcAsyncServerStreamingRequestBase::GrpcAsyncServerStreamingRequestBase(
    std::unique_ptr<grpc::ClientContext> context,
    base::OnceCallback<void(const grpc::Status&)> on_channel_closed)
    : GrpcAsyncRequest(std::move(context)), weak_factory_(this) {
  DCHECK(on_channel_closed);
  on_channel_closed_ = std::move(on_channel_closed);
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

GrpcAsyncServerStreamingRequestBase::~GrpcAsyncServerStreamingRequestBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool GrpcAsyncServerStreamingRequestBase::OnDequeuedOnDispatcherThreadInternal(
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
GrpcAsyncServerStreamingRequestBase::CreateStreamHolder() {
  return std::make_unique<ScopedGrpcServerStream>(weak_ptr_);
}

void GrpcAsyncServerStreamingRequestBase::OnRequestCanceled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock autolock(state_lock_);
  if (state_ == State::CLOSED) {
    return;
  }
  state_ = State::CLOSED;
  status_ = grpc::Status::CANCELLED;
  weak_factory_.InvalidateWeakPtrs();
}

void GrpcAsyncServerStreamingRequestBase::RunClosure(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(closure).Run();
}

void GrpcAsyncServerStreamingRequestBase::ResolveChannelClosed() {
  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_channel_closed_), status_));
}

}  // namespace internal
}  // namespace remoting
