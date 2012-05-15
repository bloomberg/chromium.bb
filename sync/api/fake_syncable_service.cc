// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/fake_syncable_service.h"

#include "base/location.h"
#include "sync/api/sync_error_factory.h"

FakeSyncableService::FakeSyncableService()
    : syncing_(false),
      type_(syncable::UNSPECIFIED) {}

FakeSyncableService::~FakeSyncableService() {}

void FakeSyncableService::set_merge_data_and_start_syncing_error(
    const SyncError& error) {
  merge_data_and_start_syncing_error_ = error;
}

void FakeSyncableService::set_process_sync_changes_error(
    const SyncError& error) {
  process_sync_changes_error_ = error;
}

bool FakeSyncableService::syncing() const {
  return syncing_;
}

// SyncableService implementation.
SyncError FakeSyncableService::MergeDataAndStartSyncing(
    syncable::ModelType type,
    const SyncDataList& initial_sync_data,
    scoped_ptr<SyncChangeProcessor> sync_processor,
    scoped_ptr<SyncErrorFactory> sync_error_factory) {
  sync_processor_ = sync_processor.Pass();
  type_ = type;
  if (!merge_data_and_start_syncing_error_.IsSet()) {
    syncing_ = true;
  }
  return merge_data_and_start_syncing_error_;
}

void FakeSyncableService::StopSyncing(syncable::ModelType type) {
  syncing_ = false;
  sync_processor_.reset();
}

SyncDataList FakeSyncableService::GetAllSyncData(
    syncable::ModelType type) const {
  return SyncDataList();
}

SyncError FakeSyncableService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& change_list) {
  return process_sync_changes_error_;
}
