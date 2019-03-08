// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_CALL_DATA_H_
#define REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_CALL_DATA_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "third_party/grpc/src/include/grpcpp/support/status.h"

namespace grpc {
class ClientContext;
}  // namespace grpc

namespace remoting {
namespace internal {

// The GrpcAsyncCallData base class that holds logic invariant to the response
// type.
//
// The lifetime of GrpcAsyncCallData is bound to the completion queue. A
// subclass may enqueue itself multiple times into the completion queue, and
// GrpcAsyncDispatcher will dequeue it and call OnDequeuedOnDispatcherThread()
// on a background thread when the event is handled. If the subclass won't
// re-enqueue itself, OnDequeuedOnDispatcherThread() should return false, which
// will delete the call data by calling DeleteOnCallerThread().
//
// Ctor, dtor, and methods except OnDequeuedOnDispatcherThread() will be called
// from the same thread (caller_task_runner_).
class GrpcAsyncCallData {
 public:
  explicit GrpcAsyncCallData(std::unique_ptr<grpc::ClientContext> context);
  virtual ~GrpcAsyncCallData();

  // Force dequeues any pending request.
  void CancelRequest();

  // Can be called from any thread.
  void DeleteOnCallerThread();

  // Returns true iff the task is not finished and the subclass will
  // *immediately* enqueue itself back to the completion queue. If this method
  // returns false, the object will be deleted by calling
  // DeleteOnCallerThread().
  // Note that this will be called from an anonymous thread.
  virtual bool OnDequeuedOnDispatcherThread(bool operation_succeeded) = 0;

 protected:
  // Called after CancelRequest() is called.
  virtual void OnRequestCanceled() {}

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  grpc::Status status_{grpc::StatusCode::UNKNOWN, "Uninitialized"};

 private:
  std::unique_ptr<grpc::ClientContext> context_;

  DISALLOW_COPY_AND_ASSIGN(GrpcAsyncCallData);
};

}  // namespace internal
}  // namespace remoting

#endif  // REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_CALL_DATA_H_
