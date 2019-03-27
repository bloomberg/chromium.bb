// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/grpc_support/grpc_async_request.h"

#include "base/threading/thread_task_runner_handle.h"
#include "third_party/grpc/src/include/grpcpp/client_context.h"

namespace remoting {
namespace internal {

GrpcAsyncRequest::GrpcAsyncRequest(
    std::unique_ptr<grpc::ClientContext> context) {
  context_ = std::move(context);
  caller_task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

GrpcAsyncRequest::~GrpcAsyncRequest() = default;

void GrpcAsyncRequest::CancelRequest() {
  VLOG(0) << "Canceling request: " << this;
  context_->TryCancel();
  OnRequestCanceled();
}

void GrpcAsyncRequest::DeleteOnCallerThread() {
  caller_task_runner_->DeleteSoon(FROM_HERE, this);
}

void GrpcAsyncRequest::Start() {
  DCHECK(!is_get_event_tag_allowed_);
  is_get_event_tag_allowed_ = true;
  StartInternal();
  is_get_event_tag_allowed_ = false;
}

bool GrpcAsyncRequest::OnDequeuedOnDispatcherThread(bool operation_succeeded) {
  DCHECK(!is_get_event_tag_allowed_);
  is_get_event_tag_allowed_ = true;
  bool result = OnDequeuedOnDispatcherThreadInternal(operation_succeeded);
  is_get_event_tag_allowed_ = false;
  return result;
}

void* GrpcAsyncRequest::GetEventTag() {
  DCHECK(is_get_event_tag_allowed_);
  return this;
}

}  // namespace internal
}  // namespace remoting
