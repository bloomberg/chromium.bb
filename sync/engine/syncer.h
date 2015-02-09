// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_SYNCER_H_
#define SYNC_ENGINE_SYNCER_H_

#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/synchronization/lock.h"
#include "sync/base/sync_export.h"
#include "sync/engine/conflict_resolver.h"
#include "sync/engine/syncer_types.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/sessions/sync_session.h"
#include "sync/util/extensions_activity.h"

namespace syncer {

class CancelationSignal;
class CommitProcessor;
class GetUpdatesProcessor;

// A Syncer provides a control interface for driving the sync cycle.  These
// cycles consist of downloading updates, parsing the response (aka. process
// updates), applying updates while resolving conflicts, and committing local
// changes.  Some of these steps may be skipped if they're deemed to be
// unnecessary.
//
// A Syncer instance expects to run on a dedicated thread.  Calls to SyncShare()
// may take an unbounded amount of time because it may block on network I/O, on
// lock contention, or on tasks posted to other threads.
class SYNC_EXPORT_PRIVATE Syncer {
 public:
  typedef std::vector<int64> UnsyncedMetaHandles;

  Syncer(CancelationSignal* cancelation_signal);
  virtual ~Syncer();

  bool ExitRequested();

  // Fetches and applies updates, resolves conflicts and commits local changes
  // for |request_types| as necessary until client and server states are in
  // sync.  The |nudge_tracker| contains state that describes why the client is
  // out of sync and what must be done to bring it back into sync.
  virtual bool NormalSyncShare(ModelTypeSet request_types,
                               const sessions::NudgeTracker& nudge_tracker,
                               sessions::SyncSession* session);

  // Performs an initial download for the |request_types|.  It is assumed that
  // the specified types have no local state, and that their associated change
  // processors are in "passive" mode, so none of the downloaded updates will be
  // applied to the model.  The |source| is sent up to the server for debug
  // purposes.  It describes the reson for performing this initial download.
  virtual bool ConfigureSyncShare(
      ModelTypeSet request_types,
      sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
      sessions::SyncSession* session);

  // Requests to download updates for the |request_types|.  For a well-behaved
  // client with a working connection to the invalidations server, this should
  // be unnecessary.  It may be invoked periodically to try to keep the client
  // in sync despite bugs or transient failures.
  virtual bool PollSyncShare(ModelTypeSet request_types,
                             sessions::SyncSession* session);

 private:
  bool DownloadAndApplyUpdates(
      ModelTypeSet request_types,
      sessions::SyncSession* session,
      GetUpdatesProcessor* get_updates_processor,
      bool create_mobile_bookmarks_folder);

  // This function will commit batches of unsynced items to the server until the
  // number of unsynced and ready to commit items reaches zero or an error is
  // encountered.  A request to exit early will be treated as an error and will
  // abort any blocking operations.
  SyncerError BuildAndPostCommits(
      ModelTypeSet request_types,
      sessions::SyncSession* session,
      CommitProcessor* commit_processor);

  void HandleCycleBegin(sessions::SyncSession* session);
  bool HandleCycleEnd(
      sessions::SyncSession* session,
      sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source);

  syncer::CancelationSignal* const cancelation_signal_;

  friend class SyncerTest;
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, NameClashWithResolver);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, IllegalAndLegalUpdates);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, TestCommitListOrderingAndNewParent);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest,
                           TestCommitListOrderingAndNewParentAndChild);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, TestCommitListOrderingCounterexample);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, TestCommitListOrderingWithNesting);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, TestCommitListOrderingWithNewItems);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, TestGetUnsyncedAndSimpleCommit);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, TestPurgeWhileUnsynced);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, TestPurgeWhileUnapplied);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, UnappliedUpdateDuringCommit);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, DeletingEntryInFolder);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest,
                           LongChangelistCreatesFakeOrphanedEntries);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, QuicklyMergeDualCreatedHierarchy);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, LongChangelistWithApplicationConflict);
  FRIEND_TEST_ALL_PREFIXES(SyncerTest, DeletingEntryWithLocalEdits);
  FRIEND_TEST_ALL_PREFIXES(EntryCreatedInNewFolderTest,
                           EntryCreatedInNewFolderMidSync);

  DISALLOW_COPY_AND_ASSIGN(Syncer);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_SYNCER_H_
