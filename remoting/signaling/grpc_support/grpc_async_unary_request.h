// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_UNARY_REQUEST_H_
#define REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_UNARY_REQUEST_H_

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/signaling/grpc_support/grpc_async_request.h"
#include "third_party/grpc/src/include/grpcpp/support/async_unary_call.h"

namespace remoting {
namespace internal {

// GrpcAsyncRequest implementation for unary call. The object is enqueued
// when waiting for response and dequeued once the response is received.
template <typename ResponseType>
class GrpcAsyncUnaryRequest : public GrpcAsyncRequest {
 public:
  using RpcCallback =
      base::OnceCallback<void(const grpc::Status&, const ResponseType&)>;

  GrpcAsyncUnaryRequest(
      std::unique_ptr<grpc::ClientContext> context,
      std::unique_ptr<grpc::ClientAsyncResponseReader<ResponseType>>
          response_reader,
      RpcCallback callback)
      : GrpcAsyncRequest(std::move(context)), weak_factory_(this) {
    response_reader_ = std::move(response_reader);
    callback_ = std::move(callback);
    weak_ptr_ = weak_factory_.GetWeakPtr();
  }
  ~GrpcAsyncUnaryRequest() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // GrpcAsyncRequest implementations

  void StartInternal() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    response_reader_->Finish(&response_, &status_, GetEventTag());
  }

  bool OnDequeuedOnDispatcherThreadInternal(bool operation_succeeded) override {
    DCHECK(operation_succeeded);
    caller_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GrpcAsyncUnaryRequest::RunCallback, weak_ptr_));
    return false;
  }

 private:
  void OnRequestCanceled() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    weak_factory_.InvalidateWeakPtrs();
  }

  void RunCallback() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::move(callback_).Run(status_, response_);
  }

  std::unique_ptr<grpc::ClientAsyncResponseReader<ResponseType>>
      response_reader_;
  ResponseType response_;
  RpcCallback callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<GrpcAsyncUnaryRequest<ResponseType>> weak_ptr_;
  base::WeakPtrFactory<GrpcAsyncUnaryRequest<ResponseType>> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(GrpcAsyncUnaryRequest);
};

}  // namespace internal
}  // namespace remoting

#endif  // REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_UNARY_REQUEST_H_
