// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_MODEL_TYPE_SYNC_PROXY_H_
#define SYNC_ENGINE_MODEL_TYPE_SYNC_PROXY_H_

#include "sync/base/sync_export.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"

namespace syncer {

// Interface used by sync backend to issue requests to a synced data type.
class SYNC_EXPORT_PRIVATE ModelTypeSyncProxy {
 public:
  ModelTypeSyncProxy();
  virtual ~ModelTypeSyncProxy();

  virtual void OnCommitCompleted(
      const DataTypeState& type_state,
      const CommitResponseDataList& response_list) = 0;
  virtual void OnUpdateReceived(
      const DataTypeState& type_state,
      const UpdateResponseDataList& response_list,
      const UpdateResponseDataList& pending_updates) = 0;
};

}  // namespace syncer

#endif  // SYNC_ENGINE_MODEL_TYPE_SYNC_PROXY_H_
