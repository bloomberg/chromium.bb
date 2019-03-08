// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/grpc_async_call_data.h"

#include "base/threading/thread_task_runner_handle.h"
#include "third_party/grpc/src/include/grpcpp/client_context.h"

namespace remoting {
namespace internal {

GrpcAsyncCallData::GrpcAsyncCallData(
    std::unique_ptr<grpc::ClientContext> context) {
  context_ = std::move(context);
  caller_task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

GrpcAsyncCallData::~GrpcAsyncCallData() = default;

void GrpcAsyncCallData::CancelRequest() {
  VLOG(0) << "Canceling request: " << this;
  context_->TryCancel();
  OnRequestCanceled();
}

void GrpcAsyncCallData::DeleteOnCallerThread() {
  if (caller_task_runner_->BelongsToCurrentThread()) {
    delete this;
    return;
  }
  caller_task_runner_->DeleteSoon(FROM_HERE, this);
}

}  // namespace internal
}  // namespace remoting
