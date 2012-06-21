// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class that watches the syncer and attempts to resolve any conflicts that
// occur.

#ifndef SYNC_ENGINE_CONFLICT_RESOLVER_H_
#define SYNC_ENGINE_CONFLICT_RESOLVER_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "sync/engine/syncer_types.h"

namespace syncable {
class BaseTransaction;
class Id;
class MutableEntry;
class WriteTransaction;
}  // namespace syncable

namespace csync {

class Cryptographer;

namespace sessions {
class ConflictProgress;
class StatusController;
}  // namespace sessions

class ConflictResolver {
  friend class SyncerTest;
  FRIEND_TEST_ALL_PREFIXES(SyncerTest,
                           ConflictResolverMergeOverwritesLocalEntry);
 public:
  // Enumeration of different conflict resolutions. Used for histogramming.
  enum SimpleConflictResolutions {
    OVERWRITE_LOCAL,           // Resolved by overwriting local changes.
    OVERWRITE_SERVER,          // Resolved by overwriting server changes.
    UNDELETE,                  // Resolved by undeleting local item.
    IGNORE_ENCRYPTION,         // Resolved by ignoring an encryption-only server
                               // change.
    NIGORI_MERGE,              // Resolved by merging nigori nodes.
    CHANGES_MATCH,             // Resolved by ignoring both local and server
                               // changes because they matched.
    CONFLICT_RESOLUTION_SIZE,
  };

  ConflictResolver();
  ~ConflictResolver();
  // Called by the syncer at the end of a update/commit cycle.
  // Returns true if the syncer should try to apply its updates again.
  bool ResolveConflicts(syncable::WriteTransaction* trans,
                        const Cryptographer* cryptographer,
                        const sessions::ConflictProgress& progress,
                        sessions::StatusController* status);

 private:
  enum ProcessSimpleConflictResult {
    NO_SYNC_PROGRESS,  // No changes to advance syncing made.
    SYNC_PROGRESS,     // Progress made.
  };

  void IgnoreLocalChanges(syncable::MutableEntry* entry);
  void OverwriteServerChanges(syncable::WriteTransaction* trans,
                              syncable::MutableEntry* entry);

  ProcessSimpleConflictResult ProcessSimpleConflict(
      syncable::WriteTransaction* trans,
      const syncable::Id& id,
      const Cryptographer* cryptographer,
      sessions::StatusController* status);

  bool ResolveSimpleConflicts(syncable::WriteTransaction* trans,
                              const Cryptographer* cryptographer,
                              const sessions::ConflictProgress& progress,
                              sessions::StatusController* status);

  DISALLOW_COPY_AND_ASSIGN(ConflictResolver);
};

}  // namespace csync

#endif  // SYNC_ENGINE_CONFLICT_RESOLVER_H_
