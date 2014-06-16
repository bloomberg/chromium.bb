// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/fake_sync_change_processor.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_data.h"

namespace syncer {

FakeSyncChangeProcessor::FakeSyncChangeProcessor() {}

FakeSyncChangeProcessor::~FakeSyncChangeProcessor() {}

syncer::SyncError FakeSyncChangeProcessor::ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) {
  changes_.insert(
      changes_.end(), change_list.begin(), change_list.end());
  return syncer::SyncError();
}

syncer::SyncDataList FakeSyncChangeProcessor::GetAllSyncData(
    syncer::ModelType type) const {
  return data_;
}

syncer::SyncError FakeSyncChangeProcessor::UpdateDataTypeContext(
    ModelType type,
    ContextRefreshStatus refresh_status,
    const std::string& context) {
  context_ = context;
  return syncer::SyncError();
}

const syncer::SyncChangeList& FakeSyncChangeProcessor::changes() const {
  return changes_;
}

syncer::SyncChangeList& FakeSyncChangeProcessor::changes() {
  return changes_;
}

const syncer::SyncDataList& FakeSyncChangeProcessor::data() const {
  return data_;
}

syncer::SyncDataList& FakeSyncChangeProcessor::data() {
  return data_;
}

const std::string& FakeSyncChangeProcessor::context() const {
  return context_;
}

std::string& FakeSyncChangeProcessor::context() {
  return context_;
}

}  // namespace syncer
