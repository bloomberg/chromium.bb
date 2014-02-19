// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/sync_change_processor_wrapper_for_test.h"

namespace syncer {

SyncChangeProcessorWrapperForTest::SyncChangeProcessorWrapperForTest(
    syncer::SyncChangeProcessor* wrapped)
    : wrapped_(wrapped) {
  DCHECK(wrapped_);
}

SyncChangeProcessorWrapperForTest::~SyncChangeProcessorWrapperForTest() {}

syncer::SyncError SyncChangeProcessorWrapperForTest::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  return wrapped_->ProcessSyncChanges(from_here, change_list);
}

syncer::SyncDataList SyncChangeProcessorWrapperForTest::GetAllSyncData(
    syncer::ModelType type) const {
  return wrapped_->GetAllSyncData(type);
}

}  // namespace syncer
