// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_NON_BLOCKING_TYPE_CORE_INTERFACE_H_
#define SYNC_ENGINE_NON_BLOCKING_TYPE_CORE_INTERFACE_H_

#include "sync/engine/non_blocking_sync_common.h"

namespace syncer {

// An interface representing a NonBlockingTypeProcessorCore and its thread.
// This abstraction is useful in tests.
class SYNC_EXPORT_PRIVATE NonBlockingTypeProcessorCoreInterface {
 public:
  NonBlockingTypeProcessorCoreInterface();
  virtual ~NonBlockingTypeProcessorCoreInterface();

  virtual void RequestCommits(const CommitRequestDataList& list) = 0;
};

}  // namespace syncer

#endif  // SYNC_ENGINE_NON_BLOCKING_TYPE_CORE_INTERFACE_H_
