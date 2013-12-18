// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/fake_sync_change_processor.h"

namespace syncer {

FakeSyncChangeProcessor::FakeSyncChangeProcessor() {}

FakeSyncChangeProcessor::~FakeSyncChangeProcessor() {}

syncer::SyncError FakeSyncChangeProcessor::ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) {
  return syncer::SyncError();
}

syncer::SyncDataList FakeSyncChangeProcessor::GetAllSyncData(
    syncer::ModelType type) const {
  return syncer::SyncDataList();
}

}  // namespace syncer
