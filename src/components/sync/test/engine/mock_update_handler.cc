// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/engine/mock_update_handler.h"

#include <string>

namespace syncer {

MockUpdateHandler::MockUpdateHandler(ModelType type) {
  progress_marker_.set_data_type_id(GetSpecificsFieldNumberFromModelType(type));
  const std::string& token_str =
      std::string("Mock token: ") + std::string(ModelTypeToString(type));
  progress_marker_.set_token(token_str);
}

MockUpdateHandler::~MockUpdateHandler() = default;

bool MockUpdateHandler::IsInitialSyncEnded() const {
  return false;
}

const sync_pb::DataTypeProgressMarker& MockUpdateHandler::GetDownloadProgress()
    const {
  return progress_marker_;
}

const sync_pb::DataTypeContext& MockUpdateHandler::GetDataTypeContext() const {
  return kEmptyDataTypeContext;
}

void MockUpdateHandler::ProcessGetUpdatesResponse(
    const sync_pb::DataTypeProgressMarker& progress_marker,
    const sync_pb::DataTypeContext& mutated_context,
    const SyncEntityList& applicable_updates,
    StatusController* status) {
  progress_marker_.CopyFrom(progress_marker);
}

void MockUpdateHandler::ApplyUpdates(StatusController* status) {
  apply_updates_count_++;
}

int MockUpdateHandler::GetApplyUpdatesCount() {
  return apply_updates_count_;
}

}  // namespace syncer
