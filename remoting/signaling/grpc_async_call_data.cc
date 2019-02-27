// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/grpc_async_call_data.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/grpc/src/include/grpcpp/client_context.h"

namespace remoting {

GrpcAsyncCallDataBase::GrpcAsyncCallDataBase(
    std::unique_ptr<grpc::ClientContext> context) {
  context_ = std::move(context);
  caller_task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

GrpcAsyncCallDataBase::~GrpcAsyncCallDataBase() = default;

void GrpcAsyncCallDataBase::RunCallbackAndSelfDestroyOnDone() {
  caller_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GrpcAsyncCallDataBase::RunCallbackOnCallerThread,
                     base::Owned(this)));
}

void GrpcAsyncCallDataBase::CancelRequest() {
  context_->TryCancel();
}

}  // namespace remoting
