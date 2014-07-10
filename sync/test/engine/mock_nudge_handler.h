// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_ENGINE_MOCK_NUDGE_HANDLER_H_
#define SYNC_TEST_ENGINE_MOCK_NUDGE_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "sync/engine/nudge_handler.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

class MockNudgeHandler : public NudgeHandler {
 public:
  MockNudgeHandler();
  virtual ~MockNudgeHandler();

  virtual void NudgeForInitialDownload(syncer::ModelType type) OVERRIDE;
  virtual void NudgeForCommit(syncer::ModelType type) OVERRIDE;
  virtual void NudgeForRefresh(syncer::ModelType type) OVERRIDE;

  int GetNumInitialDownloadNudges() const;
  int GetNumCommitNudges() const;
  int GetNumRefreshNudges() const;

  void ClearCounters();

 private:
  int num_initial_nudges_;
  int num_commit_nudges_;
  int num_refresh_nudges_;

  DISALLOW_COPY_AND_ASSIGN(MockNudgeHandler);
};

}  // namespace syncer

#endif  // SYNC_TEST_ENGINE_MOCK_NUDGE_HANDLER_H_
