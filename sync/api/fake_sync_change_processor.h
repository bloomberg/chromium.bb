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
  // Returns data().
  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type)
      const OVERRIDE;

  // SyncChangeProcessor implementation.
  //
  // Updates context().
  virtual syncer::SyncError UpdateDataTypeContext(
      ModelType type,
      ContextRefreshStatus refresh_status,
      const std::string& context) OVERRIDE;

  virtual const syncer::SyncChangeList& changes() const;
  virtual syncer::SyncChangeList& changes();

  virtual const syncer::SyncDataList& data() const;
  virtual syncer::SyncDataList& data();

  virtual const std::string& context() const;
  virtual std::string& context();

 private:
  syncer::SyncChangeList changes_;
  syncer::SyncDataList data_;
  std::string context_;

  DISALLOW_COPY_AND_ASSIGN(FakeSyncChangeProcessor);
};

}  // namespace syncer

#endif  // SYNC_API_FAKE_SYNC_CHANGE_PROCESSOR_H_
