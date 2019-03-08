// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_UNARY_CALL_DATA_H_
#define REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_UNARY_CALL_DATA_H_

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "remoting/signaling/grpc_support/grpc_async_call_data.h"
#include "third_party/grpc/src/include/grpcpp/support/async_unary_call.h"

namespace remoting {
namespace internal {

// GrpcAsyncCallData implementation for unary call. The object is enqueued
// when waiting for response and dequeued once the response is received.
template <typename ResponseType>
class GrpcAsyncUnaryCallData : public GrpcAsyncCallData {
 public:
  using RpcCallback =
      base::OnceCallback<void(const grpc::Status&, const ResponseType&)>;

  GrpcAsyncUnaryCallData(
      std::unique_ptr<grpc::ClientContext> context,
      std::unique_ptr<grpc::ClientAsyncResponseReader<ResponseType>>
          response_reader,
      RpcCallback callback)
      : GrpcAsyncCallData(std::move(context)) {
    response_reader_ = std::move(response_reader);
    callback_ = std::move(callback);
    response_reader_->Finish(&response_, &status_, /* event_tag */ this);
  }
  ~GrpcAsyncUnaryCallData() override = default;

  bool OnDequeuedOnDispatcherThread(bool operation_succeeded) override {
    DCHECK(operation_succeeded);
    caller_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), status_, response_));
    return false;
  }

 private:
  std::unique_ptr<grpc::ClientAsyncResponseReader<ResponseType>>
      response_reader_;
  ResponseType response_;
  RpcCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GrpcAsyncUnaryCallData);
};

}  // namespace internal
}  // namespace remoting

#endif  // REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_UNARY_CALL_DATA_H_
