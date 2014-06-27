// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_MODEL_TYPE_SYNC_WORKER_H_
#define SYNC_ENGINE_MODEL_TYPE_SYNC_WORKER_H_

#include "sync/engine/non_blocking_sync_common.h"

namespace syncer {

// Interface used by a synced data type to issue requests to the sync backend.
class SYNC_EXPORT_PRIVATE ModelTypeSyncWorker {
 public:
  ModelTypeSyncWorker();
  virtual ~ModelTypeSyncWorker();

  // Entry point for the ModelTypeSyncProxy to send commit requests.
  virtual void EnqueueForCommit(const CommitRequestDataList& list) = 0;
};

}  // namespace syncer

#endif  // SYNC_ENGINE_MODEL_TYPE_SYNC_WORKER_H_
