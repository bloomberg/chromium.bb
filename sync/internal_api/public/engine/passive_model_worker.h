// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_ENGINE_PASSIVE_MODEL_WORKER_H_
#define SYNC_INTERNAL_API_PUBLIC_ENGINE_PASSIVE_MODEL_WORKER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/util/syncer_error.h"

namespace syncer {

// Implementation of ModelSafeWorker for passive types.  All work is
// done on the same thread DoWorkAndWaitUntilDone (i.e., the sync
// thread).
class SYNC_EXPORT PassiveModelWorker : public ModelSafeWorker {
 public:
  explicit PassiveModelWorker(const base::MessageLoop* sync_loop,
                              WorkerLoopDestructionObserver* observer);

  // ModelSafeWorker implementation. Called on the sync thread.
  virtual void RegisterForLoopDestruction() OVERRIDE;
  virtual ModelSafeGroup GetModelSafeGroup() OVERRIDE;

 protected:
  virtual SyncerError DoWorkAndWaitUntilDoneImpl(
      const WorkCallback& work) OVERRIDE;

 private:
  virtual ~PassiveModelWorker();

  const base::MessageLoop* const sync_loop_;

  DISALLOW_COPY_AND_ASSIGN(PassiveModelWorker);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_ENGINE_PASSIVE_MODEL_WORKER_H_
