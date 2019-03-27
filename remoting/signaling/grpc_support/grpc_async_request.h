// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_REQUEST_H_
#define REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_REQUEST_H_

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

// The GrpcAsyncRequest base class that holds logic invariant to the response
// type.
//
// The lifetime of GrpcAsyncRequest is bound to the completion queue. A
// subclass may enqueue itself multiple times into the completion queue, and
// GrpcAsyncExecutor will dequeue it and call OnDequeuedOnDispatcherThread()
// on a background thread when the event is handled. If the subclass won't
// re-enqueue itself, OnDequeuedOnDispatcherThread() should return false, which
// will delete the call data by calling DeleteOnCallerThread().
//
// Subclass is not allowed to enqueue itself outside the call stack of
// StartInternal() and OnDequeuedOnDispatcherThread().
//
// Ctor, dtor, and methods except OnDequeuedOnDispatcherThread() will be called
// from the same thread (caller_task_runner_).
class GrpcAsyncRequest {
 public:
  explicit GrpcAsyncRequest(std::unique_ptr<grpc::ClientContext> context);
  virtual ~GrpcAsyncRequest();

  // Force dequeues any pending request.
  void CancelRequest();

  // Posts a task to delete |this| on the caller thread.
  void DeleteOnCallerThread();

  void Start();

  // Returns true iff the task is not finished and the subclass will
  // *immediately* enqueue itself back to the completion queue. If this method
  // returns false, the object will be deleted by calling
  // DeleteOnCallerThread(). Tasks posted within this method will be scheduled
  // before DeleteOnCallerThread()'s deletion task, so subclass still has a
  // chance to run on the caller thread before it gets deleted.
  // Note that this will be called from an anonymous thread.
  bool OnDequeuedOnDispatcherThread(bool operation_succeeded);

 protected:
  // Gets an event tag for enqueueing |this| to the completion queue. This has
  // to be called within StartInternal() and
  // OnDequeuedOnDispatcherThreadInternal() without task posting, otherwise it
  // will fail a DCHECK.
  void* GetEventTag();

  // The first enqueue must be done within this method.
  virtual void StartInternal() = 0;

  // See OnDequeuedOnDispatcherThread().
  virtual bool OnDequeuedOnDispatcherThreadInternal(
      bool operation_succeeded) = 0;

  // Called after CancelRequest() is called.
  virtual void OnRequestCanceled() = 0;

  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  grpc::Status status_{grpc::StatusCode::UNKNOWN, "Uninitialized"};

 private:
  bool is_get_event_tag_allowed_ = false;
  std::unique_ptr<grpc::ClientContext> context_;

  DISALLOW_COPY_AND_ASSIGN(GrpcAsyncRequest);
};

}  // namespace internal
}  // namespace remoting

#endif  // REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_REQUEST_H_
