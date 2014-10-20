// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/engine/passive_model_worker.h"

#include "base/message_loop/message_loop.h"

namespace syncer {

PassiveModelWorker::PassiveModelWorker(const base::MessageLoop* sync_loop,
                                       WorkerLoopDestructionObserver* observer)
    : ModelSafeWorker(observer),
      sync_loop_(sync_loop) {
}

PassiveModelWorker::~PassiveModelWorker() {
}

void PassiveModelWorker::RegisterForLoopDestruction() {
  SetWorkingLoopToCurrent();
}

SyncerError PassiveModelWorker::DoWorkAndWaitUntilDoneImpl(
    const WorkCallback& work) {
  DCHECK_EQ(base::MessageLoop::current(), sync_loop_);
  // Simply do the work on the current thread.
  return work.Run();
}

ModelSafeGroup PassiveModelWorker::GetModelSafeGroup() {
  return GROUP_PASSIVE;
}

}  // namespace syncer
