// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_FAKE_SYNC_CHANGE_PROCESSOR_H_
#define SYNC_API_FAKE_SYNC_CHANGE_PROCESSOR_H_

#include "sync/api/sync_change_processor.h"

namespace syncer {

class FakeSyncChangeProcessor : public SyncChangeProcessor {
 public:
  FakeSyncChangeProcessor();
  virtual ~FakeSyncChangeProcessor();

  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type)
      const OVERRIDE;
};

}  // namespace syncer

#endif  // SYNC_API_FAKE_SYNC_CHANGE_PROCESSOR_H_
