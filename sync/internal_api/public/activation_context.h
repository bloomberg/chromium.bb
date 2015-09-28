// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_ACTIVATION_CONTEXT_H_
#define SYNC_INTERNAL_API_PUBLIC_ACTIVATION_CONTEXT_H_

#include "base/memory/weak_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/non_blocking_sync_common.h"

namespace syncer_v2 {
class ModelTypeProcessor;

// The state passed from ModelTypeProcessor to Sync thread during DataType
// activation.
struct SYNC_EXPORT_PRIVATE ActivationContext {
  ActivationContext();
  ~ActivationContext();

  // Initial DataTypeState at the moment of activation.
  DataTypeState data_type_state;
  // Pending updates from the previous session.
  // TODO(stanisc): crbug.com/529498: should remove pending updates.
  UpdateResponseDataList saved_pending_updates;
  // Task runner for the data type.
  scoped_refptr<base::SequencedTaskRunner> type_task_runner;
  // ModelTypeProcessor for the data type.
  base::WeakPtr<ModelTypeProcessor> type_processor;
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_ACTIVATION_CONTEXT_H_
