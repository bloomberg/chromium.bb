// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_NUDGE_HANDLER_H_
#define SYNC_ENGINE_NUDGE_HANDLER_H_

#include "base/compiler_specific.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

class SYNC_EXPORT_PRIVATE NudgeHandler {
 public:
  NudgeHandler();
  virtual ~NudgeHandler();

  virtual void NudgeForInitialDownload(syncer::ModelType type) = 0;
  virtual void NudgeForCommit(syncer::ModelType type) = 0;
  virtual void NudgeForRefresh(syncer::ModelType type) = 0;
};

}  // namespace syncer

#endif  // SYNC_ENGINE_NUDGE_HANDLER_H_
