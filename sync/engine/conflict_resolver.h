// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class that watches the syncer and attempts to resolve any conflicts that
// occur.

#ifndef SYNC_ENGINE_CONFLICT_RESOLVER_H_
#define SYNC_ENGINE_CONFLICT_RESOLVER_H_

#include <set>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "sync/engine/syncer_types.h"

namespace syncer {

namespace syncable {
class Id;
class WriteTransaction;
}  // namespace syncable

class Cryptographer;
struct UpdateCounters;

namespace sessions {
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
  void ResolveConflicts(syncable::WriteTransaction* trans,
                        const Cryptographer* cryptographer,
                        const std::set<syncable::Id>& simple_conflict_ids,
                        sessions::StatusController* status,
                        UpdateCounters* counters);

 private:
  void ProcessSimpleConflict(
      syncable::WriteTransaction* trans,
      const syncable::Id& id,
      const Cryptographer* cryptographer,
      sessions::StatusController* status,
      UpdateCounters* counters);

  DISALLOW_COPY_AND_ASSIGN(ConflictResolver);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_CONFLICT_RESOLVER_H_
