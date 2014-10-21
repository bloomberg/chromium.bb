// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_ENGINE_MOCK_UPDATE_HANDLER_H_
#define SYNC_TEST_ENGINE_MOCK_UPDATE_HANDLER_H_

#include "base/compiler_specific.h"
#include "sync/engine/update_handler.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {

class MockUpdateHandler : public UpdateHandler {
 public:
  explicit MockUpdateHandler(ModelType type);
  ~MockUpdateHandler() override;

  // UpdateHandler implementation.
  void GetDownloadProgress(
      sync_pb::DataTypeProgressMarker* progress_marker) const override;
  void GetDataTypeContext(sync_pb::DataTypeContext* context) const override;
  SyncerError ProcessGetUpdatesResponse(
      const sync_pb::DataTypeProgressMarker& progress_marker,
      const sync_pb::DataTypeContext& mutated_context,
      const SyncEntityList& applicable_updates,
      sessions::StatusController* status) override;
  void ApplyUpdates(sessions::StatusController* status) override;
  void PassiveApplyUpdates(sessions::StatusController* status) override;

  // Returns the number of times ApplyUpdates() was invoked.
  int GetApplyUpdatesCount();

  // Returns the number of times PassiveApplyUpdates() was invoked.
  int GetPassiveApplyUpdatesCount();

 private:
  sync_pb::DataTypeProgressMarker progress_marker_;

  int apply_updates_count_;
  int passive_apply_updates_count_;
};

}  // namespace syncer

#endif  // SYNC_TEST_ENGINE_MOCK_UPDATE_HANDLER_H_
