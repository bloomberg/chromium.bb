// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_GET_COMMIT_IDS_H_
#define SYNC_ENGINE_GET_COMMIT_IDS_H_

#include <vector>

#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/syncable/directory.h"

using std::vector;

namespace syncer {

namespace sessions {
class OrderedCommitSet;
}

namespace syncable {
class BaseTransaction;
}

// These functions return handles in "commit order".  A valid commit ordering is
// one where parents are placed before children, predecessors are placed before
// successors, and deletes appear after creates and moves.
//
// The predecessor to successor rule was implemented when we tracked positions
// within a folder that was sensitive to such things.  The current positioning
// system can handle receiving the elements within a folder out of order, so we
// may be able to remove that functionality in the future.
// See crbug.com/287938.

// Returns up to |max_entries| metahandles of entries that belong to the
// specified |type| and are ready for commit.  The returned handles will be
// in a valid commit ordering.
SYNC_EXPORT_PRIVATE void GetCommitIdsForType(
    syncable::BaseTransaction* trans,
    ModelType type,
    size_t max_entries,
    std::vector<int64>* out);

// Fills the specified |ordered_commit_set| with up to |commit_batch_size|
// metahandles that belong to the set of types |requested_types| and are ready
// for commit.  The list will be in a valid commit ordering.
SYNC_EXPORT_PRIVATE void GetCommitIds(
    syncable::BaseTransaction* trans,
    ModelTypeSet requested_types,
    size_t commit_batch_size,
    sessions::OrderedCommitSet* ordered_commit_set);

}  // namespace syncer

#endif  // SYNC_ENGINE_GET_COMMIT_IDS_H_
