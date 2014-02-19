// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/engine/mock_update_handler.h"

#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

MockUpdateHandler::MockUpdateHandler(ModelType type) {
  progress_marker_.set_data_type_id(GetSpecificsFieldNumberFromModelType(type));
  const std::string& token_str =
      std::string("Mock token: ") + std::string(ModelTypeToString(type));
  progress_marker_.set_token(token_str);
}

MockUpdateHandler::~MockUpdateHandler() {}

void MockUpdateHandler::GetDownloadProgress(
      sync_pb::DataTypeProgressMarker* progress_marker) const {
  progress_marker->CopyFrom(progress_marker_);
}

void MockUpdateHandler::ProcessGetUpdatesResponse(
      const sync_pb::DataTypeProgressMarker& progress_marker,
      const SyncEntityList& applicable_updates,
      sessions::StatusController* status) {
  progress_marker_.CopyFrom(progress_marker);
}

void MockUpdateHandler::ApplyUpdates(sessions::StatusController* status) {}

void MockUpdateHandler::PassiveApplyUpdates(
    sessions::StatusController* status) {}

}  // namespace syncer
