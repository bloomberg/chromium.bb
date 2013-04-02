// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_SYNCER_H_
#define SYNC_ENGINE_SYNCER_H_

#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/synchronization/lock.h"
#include "sync/base/sync_export.h"
#include "sync/engine/conflict_resolver.h"
#include "sync/engine/syncer_types.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/sessions/sync_session.h"
#include "sync/util/extensions_activity_monitor.h"

namespace syncer {

namespace syncable {
class Entry;
class MutableEntry;
}  // namespace syncable

enum SyncerStep {
  SYNCER_BEGIN,
  DOWNLOAD_UPDATES,
  PROCESS_UPDATES,
  STORE_TIMESTAMPS,
  APPLY_UPDATES,
  COMMIT,
  SYNCER_END
};

// A Syncer provides a control interface for driving the individual steps
// of the sync cycle.  Each cycle (hopefully) moves the client into closer
// synchronization with the server.  The individual steps are modeled
// as SyncerCommands, and the ordering of the steps is expressed using
// the SyncerStep enum.
//
// A Syncer instance expects to run on a dedicated thread.  Calls
// to SyncShare() may take an unbounded amount of time, as SyncerCommands
// may block on network i/o, on lock contention, or on tasks posted to
// other threads.
class SYNC_EXPORT_PRIVATE Syncer {
 public:
  typedef std::vector<int64> UnsyncedMetaHandles;

  Syncer();
  virtual ~Syncer();

  // Called by other threads to tell the syncer to stop what it's doing
  // and return early from SyncShare, if possible.
  bool ExitRequested();
  void RequestEarlyExit();

  // Runs a sync cycle from |first_step| to |last_step|.
  // Returns true if the cycle completed with |last_step|, and false
  // if it terminated early due to error / exit requested.
  virtual bool SyncShare(sessions::SyncSession* session,
                         SyncerStep first_step,
                         SyncerStep last_step);

 private:
  bool early_exit_requested_;
  base::Lock early_exit_requested_lock_;

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

// Utility function declarations.
void CopyServerFields(syncable::Entry* src, syncable::MutableEntry* dest);
const char* SyncerStepToString(const SyncerStep);

}  // namespace syncer

#endif  // SYNC_ENGINE_SYNCER_H_
