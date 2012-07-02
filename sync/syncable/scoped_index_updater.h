// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_SCOPED_INDEX_UPDATER_H_
#define SYNC_SYNCABLE_SCOPED_INDEX_UPDATER_H_

#include "sync/syncable/directory.h"

namespace syncer {
namespace syncable {
class ScopedKernelLock;

// A ScopedIndexUpdater temporarily removes an entry from an index,
// and restores it to the index when the scope exits.  This simplifies
// the common pattern where items need to be removed from an index
// before updating the field.
//
// This class is parameterized on the Indexer traits type, which
// must define a Comparator and a static bool ShouldInclude
// function for testing whether the item ought to be included
// in the index.
template<typename Indexer>
class ScopedIndexUpdater {
 public:
  ScopedIndexUpdater(const ScopedKernelLock& proof_of_lock,
                     EntryKernel* entry,
                     typename Index<Indexer>::Set* index)
      : entry_(entry),
        index_(index) {
    // First call to ShouldInclude happens before the field is updated.
    if (Indexer::ShouldInclude(entry_)) {
      // TODO(lipalani): Replace this CHECK with |SyncAssert| by refactorting
      // this class into a function.
      CHECK(index_->erase(entry_));
    }
  }

  ~ScopedIndexUpdater() {
    // Second call to ShouldInclude happens after the field is updated.
    if (Indexer::ShouldInclude(entry_)) {
      // TODO(lipalani): Replace this CHECK with |SyncAssert| by refactorting
      // this class into a function.
      CHECK(index_->insert(entry_).second);
    }
  }
 private:
  // The entry that was temporarily removed from the index.
  EntryKernel* entry_;
  // The index which we are updating.
  typename Index<Indexer>::Set* const index_;
};

}  // namespace syncable
}  // namespace syncer

#endif // SYNC_SYNCABLE_SCOPED_INDEX_UPDATER_H_
