// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/grpc_support/grpc_async_executor.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/no_destructor.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "remoting/signaling/grpc_support/grpc_async_request.h"
#include "third_party/grpc/src/include/grpcpp/completion_queue.h"

namespace remoting {

namespace {

using DequeueCallback = base::OnceCallback<void(bool operation_succeeded)>;

struct DispatchTask {
  scoped_refptr<base::SequencedTaskRunner> caller_sequence_task_runner;
  DequeueCallback callback;
};

// Helper class that is shared by all GrpcAsyncExecutors to run the completion
// queue and dispatch tasks back to the right executor.
// When enqueueing, caller should create a DispatchTask and enqueue it as the
// event_tag. The ownership of the object will be taken by the
// CompletionQueueDispatcher.
class CompletionQueueDispatcher {
 public:
  CompletionQueueDispatcher();
  ~CompletionQueueDispatcher();

  static CompletionQueueDispatcher* GetInstance();

  grpc::CompletionQueue* completion_queue() { return &completion_queue_; }

 private:
  void RunQueueOnDispatcherThread();

  // TODO(yuweih): Consider using task scheduler instead.
  // We need a dedicated thread because getting response from the completion
  // queue will block until any response is received. Note that the RPC call
  // itself is still async, meaning any new RPC call when the queue is blocked
  // can still be made, and can unblock the queue once the response is ready.
  base::Thread dispatcher_thread_{"grpc_completion_queue_dispatcher"};

  // Note that the gRPC library is thread-safe.
  grpc::CompletionQueue completion_queue_;

  DISALLOW_COPY_AND_ASSIGN(CompletionQueueDispatcher);
};

CompletionQueueDispatcher::CompletionQueueDispatcher() {
  dispatcher_thread_.Start();
  dispatcher_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CompletionQueueDispatcher::RunQueueOnDispatcherThread,
                     base::Unretained(this)));
}

CompletionQueueDispatcher::~CompletionQueueDispatcher() = default;

// static
CompletionQueueDispatcher* CompletionQueueDispatcher::GetInstance() {
  static base::NoDestructor<CompletionQueueDispatcher> dispatcher;
  return dispatcher.get();
}

void CompletionQueueDispatcher::RunQueueOnDispatcherThread() {
  void* event_tag;
  bool operation_succeeded = false;

  // completion_queue_.Next() blocks until a response is received.
  while (completion_queue_.Next(&event_tag, &operation_succeeded)) {
    DispatchTask* task = reinterpret_cast<DispatchTask*>(event_tag);
    task->caller_sequence_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(task->callback), operation_succeeded));
    delete task;
  }
}

}  // namespace

GrpcAsyncExecutor::GrpcAsyncExecutor() : weak_factory_(this) {}

GrpcAsyncExecutor::~GrpcAsyncExecutor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(0) << "# of pending RPCs at destruction: " << pending_requests_.size();
  for (auto* pending_request : pending_requests_) {
    pending_request->CancelRequest();
  }
}

void GrpcAsyncExecutor::ExecuteRpc(std::unique_ptr<GrpcAsyncRequest> request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_requests_.find(request.get()) == pending_requests_.end());
  auto* unowned_request = request.get();
  auto task = std::make_unique<DispatchTask>();
  task->caller_sequence_task_runner = base::SequencedTaskRunnerHandle::Get();
  task->callback =
      base::BindOnce(&GrpcAsyncExecutor::OnDequeue, weak_factory_.GetWeakPtr(),
                     std::move(request));
  if (!unowned_request->CanStartRequest()) {
    VLOG(0) << "RPC is canceled before execution: " << unowned_request;
    return;
  }
  VLOG(0) << "Enqueuing RPC: " << unowned_request;

  // User can potentially delete the executor in the callback, so we should
  // delay it to prevent race condition. We also bind the closure with the
  // WeakPtr of this object to make sure it won't run after the executor is
  // deleted.
  auto run_task_cb = base::BindRepeating(
      &GrpcAsyncExecutor::PostTaskToRunClosure, weak_factory_.GetWeakPtr());

  unowned_request->Start(
      run_task_cb, CompletionQueueDispatcher::GetInstance()->completion_queue(),
      task.release());
  pending_requests_.insert(unowned_request);
}

void GrpcAsyncExecutor::OnDequeue(std::unique_ptr<GrpcAsyncRequest> request,
                                  bool operation_succeeded) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!request->OnDequeue(operation_succeeded)) {
    VLOG(0) << "Dequeuing RPC: " << request.get();
    DCHECK(pending_requests_.find(request.get()) != pending_requests_.end());
    pending_requests_.erase(request.get());
    return;
  }

  VLOG(0) << "Re-enqueuing RPC: " << request.get();
  auto* unowned_request = request.get();
  auto task = std::make_unique<DispatchTask>();
  task->caller_sequence_task_runner = base::SequencedTaskRunnerHandle::Get();
  task->callback =
      base::BindOnce(&GrpcAsyncExecutor::OnDequeue, weak_factory_.GetWeakPtr(),
                     std::move(request));
  unowned_request->Reenqueue(task.release());
}

void GrpcAsyncExecutor::PostTaskToRunClosure(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&GrpcAsyncExecutor::RunClosure, weak_factory_.GetWeakPtr(),
                     std::move(closure)));
}

void GrpcAsyncExecutor::RunClosure(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(closure).Run();
}

}  // namespace remoting
