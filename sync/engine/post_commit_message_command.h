// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_POST_COMMIT_MESSAGE_COMMAND_H_
#define SYNC_ENGINE_POST_COMMIT_MESSAGE_COMMAND_H_
#pragma once

#include "base/compiler_specific.h"
#include "sync/engine/syncer_command.h"

namespace browser_sync {

class PostCommitMessageCommand : public SyncerCommand {
 public:
  PostCommitMessageCommand();
  virtual ~PostCommitMessageCommand();

  // SyncerCommand implementation.
  virtual SyncerError ExecuteImpl(sessions::SyncSession* session) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PostCommitMessageCommand);
};

}  // namespace browser_sync

#endif  // SYNC_ENGINE_POST_COMMIT_MESSAGE_COMMAND_H_
