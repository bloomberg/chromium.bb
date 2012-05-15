// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_FAKE_SYNCABLE_SERVICE_H_
#define SYNC_API_FAKE_SYNCABLE_SERVICE_H_
#pragma once

#include "sync/api/syncable_service.h"

class SyncErrorFactory;

// A fake SyncableService that can return arbitrary values and maintains the
// syncing status.
class FakeSyncableService : public SyncableService {
 public:
  FakeSyncableService();
  virtual ~FakeSyncableService();

  // Setters for SyncableService implementation results.
  void set_merge_data_and_start_syncing_error(const SyncError& error);
  void set_process_sync_changes_error(const SyncError& error);

  // Whether we're syncing or not. Set on a successful MergeDataAndStartSyncing,
  // unset on StopSyncing. False by default.
  bool syncing() const;

  // SyncableService implementation.
  virtual SyncError MergeDataAndStartSyncing(
      syncable::ModelType type,
      const SyncDataList& initial_sync_data,
      scoped_ptr<SyncChangeProcessor> sync_processor,
      scoped_ptr<SyncErrorFactory> sync_error_factory) OVERRIDE;
  virtual void StopSyncing(syncable::ModelType type) OVERRIDE;
  virtual SyncDataList GetAllSyncData(syncable::ModelType type) const OVERRIDE;
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE;

 private:
  scoped_ptr<SyncChangeProcessor> sync_processor_;
  SyncError merge_data_and_start_syncing_error_;
  SyncError process_sync_changes_error_;
  bool syncing_;
  syncable::ModelType type_;
};

#endif  // SYNC_API_FAKE_SYNCABLE_SERVICE_H_
