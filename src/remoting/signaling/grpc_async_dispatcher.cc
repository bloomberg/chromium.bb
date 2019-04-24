// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/grpc_async_dispatcher.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"

namespace remoting {

GrpcAsyncDispatcher::GrpcAsyncDispatcher() {
  dispatcher_thread_.Start();
  dispatcher_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GrpcAsyncDispatcher::RunQueueOnDispatcherThread,
                     base::Unretained(this)));
}

GrpcAsyncDispatcher::~GrpcAsyncDispatcher() {
  completion_queue_.Shutdown();
  {
    base::AutoLock autolock(pending_rpcs_lock_);
    VLOG(0) << "# of pending RPCs at destruction: " << pending_rpcs_.size();
    for (auto* pending_rpc : pending_rpcs_) {
      pending_rpc->CancelRequest();
    }
  }
  dispatcher_thread_.Stop();
  DCHECK_EQ(0u, pending_rpcs_.size());
}

void GrpcAsyncDispatcher::RunQueueOnDispatcherThread() {
  void* event_tag;
  bool operation_succeeded = false;

  // completion_queue_.Next() blocks until a response is received.
  while (completion_queue_.Next(&event_tag, &operation_succeeded)) {
    // |operation_succeeded| is always true for client-side finish event.
    DCHECK(operation_succeeded);
    VLOG(0) << "Dequeuing RPC: " << event_tag;
    GrpcAsyncCallDataBase* rpc_data =
        reinterpret_cast<GrpcAsyncCallDataBase*>(event_tag);
    {
      base::AutoLock autolock(pending_rpcs_lock_);
      DCHECK(pending_rpcs_.find(rpc_data) != pending_rpcs_.end());
      pending_rpcs_.erase(rpc_data);
    }
    rpc_data->RunCallbackAndSelfDestroyOnDone();
  }
}

void GrpcAsyncDispatcher::RegisterRpcData(
    std::unique_ptr<GrpcAsyncCallDataBase> rpc_data) {
  {
    base::AutoLock autolock(pending_rpcs_lock_);
    DCHECK(pending_rpcs_.find(rpc_data.get()) == pending_rpcs_.end());
    pending_rpcs_.insert(rpc_data.get());
  }
  VLOG(0) << "Enqueuing RPC: " << rpc_data.get();
  rpc_data.release()->RegisterAndMoveOwnershipToCompletionQueue();
}

}  // namespace remoting
