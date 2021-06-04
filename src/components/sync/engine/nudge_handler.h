// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_NUDGE_HANDLER_H_
#define COMPONENTS_SYNC_ENGINE_NUDGE_HANDLER_H_

#include "base/compiler_specific.h"
#include "components/sync/base/model_type.h"

namespace syncer {

class NudgeHandler {
 public:
  NudgeHandler() = default;
  virtual ~NudgeHandler() = default;

  // Schedules initial sync for |type| and returns.
  virtual void NudgeForInitialDownload(ModelType type) = 0;
  // Schedules a commit for |type| and returns.
  virtual void NudgeForCommit(ModelType type) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_NUDGE_HANDLER_H_
