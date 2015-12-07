// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_COMMIT_QUEUE_H_
#define SYNC_ENGINE_COMMIT_QUEUE_H_

#include "sync/internal_api/public/non_blocking_sync_common.h"

namespace syncer_v2 {

// Interface used by a synced data type to issue requests to the sync backend.
class SYNC_EXPORT_PRIVATE CommitQueue {
 public:
  CommitQueue();
  virtual ~CommitQueue();

  // Entry point for the ModelTypeProcessor to send commit requests.
  virtual void EnqueueForCommit(const CommitRequestDataList& list) = 0;
};

}  // namespace syncer

#endif  // SYNC_ENGINE_COMMIT_QUEUE_H_
