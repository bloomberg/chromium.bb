// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/sync_change_processor.h"

namespace syncer {

SyncChangeProcessor::SyncChangeProcessor() {}

SyncChangeProcessor::~SyncChangeProcessor() {}

syncer::SyncError SyncChangeProcessor::UpdateDataTypeContext(
    ModelType type,
    syncer::SyncChangeProcessor::ContextRefreshStatus refresh_status,
    const std::string& context) {
  // Do nothing.
  return syncer::SyncError();
}

}  // namespace syncer
