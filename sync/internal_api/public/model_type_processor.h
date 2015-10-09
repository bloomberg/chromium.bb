// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_MODEL_TYPE_PROCESSOR_H_
#define SYNC_INTERNAL_API_PUBLIC_MODEL_TYPE_PROCESSOR_H_

#include "base/memory/scoped_ptr.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"

namespace syncer_v2 {
class CommitQueue;

// Interface used by sync backend to issue requests to a synced data type.
class SYNC_EXPORT_PRIVATE ModelTypeProcessor {
 public:
  ModelTypeProcessor();
  virtual ~ModelTypeProcessor();

  // Callback used to process the handshake response.
  virtual void OnConnect(scoped_ptr<CommitQueue> commit_queue) = 0;

  // Informs this object that some of its commit requests have been
  // successfully serviced.
  virtual void OnCommitCompleted(
      const DataTypeState& type_state,
      const CommitResponseDataList& response_list) = 0;

  // Informs this object that there are some incoming updates is should
  // handle.
  virtual void OnUpdateReceived(
      const DataTypeState& type_state,
      const UpdateResponseDataList& response_list,
      const UpdateResponseDataList& pending_updates) = 0;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_MODEL_TYPE_PROCESSOR_H_
