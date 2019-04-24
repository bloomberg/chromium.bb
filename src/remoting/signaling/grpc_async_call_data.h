// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_GRPC_ASYNC_CALL_DATA_H_
#define REMOTING_SIGNALING_GRPC_ASYNC_CALL_DATA_H_

#include <memory>
#include <utility>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "third_party/grpc/src/include/grpcpp/support/async_unary_call.h"
#include "third_party/grpc/src/include/grpcpp/support/status.h"

namespace grpc {
class ClientContext;
}  // namespace grpc

namespace remoting {

// The GrpcAsyncCallData base class that holds logic invariant to the response
// type.
class GrpcAsyncCallDataBase {
 public:
  explicit GrpcAsyncCallDataBase(std::unique_ptr<grpc::ClientContext> context);
  virtual ~GrpcAsyncCallDataBase();

  void RunCallbackAndSelfDestroyOnDone();
  void CancelRequest();

  virtual void RegisterAndMoveOwnershipToCompletionQueue() = 0;
  virtual void RunCallbackOnCallerThread() = 0;

 protected:
  grpc::Status status_{grpc::StatusCode::UNKNOWN, "Uninitialized"};

 private:
  std::unique_ptr<grpc::ClientContext> context_;
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(GrpcAsyncCallDataBase);
};

template <typename ResponseType>
class GrpcAsyncCallData : public GrpcAsyncCallDataBase {
 public:
  using RpcCallback =
      base::OnceCallback<void(grpc::Status, const ResponseType&)>;

  GrpcAsyncCallData(
      std::unique_ptr<grpc::ClientContext> context,
      std::unique_ptr<grpc::ClientAsyncResponseReader<ResponseType>>
          response_reader,
      RpcCallback callback)
      : GrpcAsyncCallDataBase(std::move(context)) {
    response_reader_ = std::move(response_reader);
    callback_ = std::move(callback);
  }
  ~GrpcAsyncCallData() override = default;

  void RegisterAndMoveOwnershipToCompletionQueue() override {
    response_reader_->Finish(&response_, &status_, /* event_tag */ this);
  }

  void RunCallbackOnCallerThread() override {
    std::move(callback_).Run(status_, response_);
  }

 private:
  std::unique_ptr<grpc::ClientAsyncResponseReader<ResponseType>>
      response_reader_;
  ResponseType response_;
  RpcCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GrpcAsyncCallData);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_GRPC_ASYNC_CALL_DATA_H_
