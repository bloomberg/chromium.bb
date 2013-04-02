// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_PARENT_CHILD_INDEX_UPDATER_H_
#define SYNC_SYNCABLE_PARENT_CHILD_INDEX_UPDATER_H_

#include "base/basictypes.h"
#include "sync/base/sync_export.h"

namespace syncer {
namespace syncable {

class ParentChildIndex;
class ScopedKernelLock;
struct EntryKernel;

// Temporarily removes an item from the ParentChildIndex and re-adds it this
// object goes out of scope.
class ScopedParentChildIndexUpdater {
 public:
  ScopedParentChildIndexUpdater(ScopedKernelLock& proof_of_lock,
                                EntryKernel* entry,
                                ParentChildIndex* index);
  ~ScopedParentChildIndexUpdater();

 private:
  EntryKernel* entry_;
  ParentChildIndex* index_;

  DISALLOW_COPY_AND_ASSIGN(ScopedParentChildIndexUpdater);
};

}  // namespace syncer
}  // namespace syncable

#endif  // SYNC_SYNCABLE_PARENT_CHILD_INDEX_UPDATER_H_
