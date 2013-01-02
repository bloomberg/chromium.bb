// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_DOWNLOAD_UPDATES_COMMAND_H_
#define SYNC_ENGINE_DOWNLOAD_UPDATES_COMMAND_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "sync/engine/syncer_command.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/protocol/sync.pb.h"

namespace sync_pb {
class EntitySpecifics;
}

namespace syncer {

// Determine the enabled datatypes, download a batch of updates for them
// from the server, place the result in the SyncSession for further processing.
//
// The main inputs to this operation are the download_progress state
// in the syncable::Directory, and the set of enabled types as indicated by
// the SyncSession. DownloadUpdatesCommand will fetch updates for
// all the enabled types, using download_progress to indicate the starting
// point to the server. DownloadUpdatesCommand stores the server response
// in the SyncSession. Only one server request is performed per Execute
// operation. A loop that causes multiple Execute operations within a sync
// session can be found in the Syncer logic. When looping, the
// DownloadUpdatesCommand consumes the information stored by the
// StoreTimestampsCommand.
//
// In practice, DownloadUpdatesCommand should loop until all updates are
// downloaded for all enabled datatypes (i.e., until the server indicates
// changes_remaining == 0 in the GetUpdates response), or until an error
// is encountered.
class SYNC_EXPORT_PRIVATE DownloadUpdatesCommand : public SyncerCommand {
 public:
  // |create_mobile_bookmarks_folder| controls whether or not to
  // create the mobile bookmarks folder if it's not already created.
  // Should be set to true only by mobile clients.
  explicit DownloadUpdatesCommand(bool create_mobile_bookmarks_folder);
  virtual ~DownloadUpdatesCommand();

  // SyncerCommand implementation.
  virtual SyncerError ExecuteImpl(sessions::SyncSession* session) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(DownloadUpdatesCommandTest, VerifyAppendDebugInfo);
  void AppendClientDebugInfoIfNeeded(sessions::SyncSession* session,
      sync_pb::DebugInfo* debug_info);

  const bool create_mobile_bookmarks_folder_;

  DISALLOW_COPY_AND_ASSIGN(DownloadUpdatesCommand);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_DOWNLOAD_UPDATES_COMMAND_H_
