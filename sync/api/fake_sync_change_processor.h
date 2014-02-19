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

  // SyncChangeProcessor implementation.
  //
  // ProcessSyncChanges will accumulate changes in changes() until they are
  // cleared.
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

  // SyncChangeProcessor implementation.
  //
  // Returns an empty list.
  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type)
      const OVERRIDE;

  virtual const syncer::SyncChangeList& changes() const;
  virtual syncer::SyncChangeList& changes();

 private:
  syncer::SyncChangeList change_list_;

  DISALLOW_COPY_AND_ASSIGN(FakeSyncChangeProcessor);
};

}  // namespace syncer

#endif  // SYNC_API_FAKE_SYNC_CHANGE_PROCESSOR_H_
