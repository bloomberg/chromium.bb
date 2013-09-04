// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SESSIONS_ORDERED_COMMIT_SET_H_
#define SYNC_SESSIONS_ORDERED_COMMIT_SET_H_

#include <map>
#include <set>
#include <vector>

#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"

namespace syncer {
namespace sessions {

// TODO(ncarter): This code is more generic than just Commit and can
// be reused elsewhere (e.g. ChangeReorderBuffer do similar things).  Merge
// all these implementations.
class SYNC_EXPORT_PRIVATE OrderedCommitSet {
 public:
  // A list of indices into the full list of commit ids such that:
  // 1 - each element is an index belonging to a particular ModelSafeGroup.
  // 2 - the vector is in sorted (smallest to largest) order.
  // 3 - each element is a valid index for GetCommitItemAt.
  // See GetCommitIdProjection for usage.
  typedef std::vector<size_t> Projection;

  // TODO(chron): Reserve space according to batch size?
  explicit OrderedCommitSet(const ModelSafeRoutingInfo& routes);
  ~OrderedCommitSet();

  bool HaveCommitItem(const int64 metahandle) const {
    return inserted_metahandles_.count(metahandle) > 0;
  }

  void AddCommitItem(const int64 metahandle, ModelType type);
  void AddCommitItems(const std::vector<int64> metahandles, ModelType type);

  const std::vector<int64>& GetAllCommitHandles() const {
    return metahandle_order_;
  }

  // Return the handle at index |position| in this OrderedCommitSet.  Note that
  // the index uniquely identifies the same logical item in each of:
  // 1) this OrderedCommitSet
  // 2) the CommitRequest sent to the server
  // 3) the list of EntryResponse objects in the CommitResponse.
  // These together allow re-association of the pre-commit Id with the
  // actual committed entry.
  int64 GetCommitHandleAt(const size_t position) const {
    return metahandle_order_[position];
  }

  // Same as above, but for ModelType of the item.
  ModelType GetModelTypeAt(const size_t position) const {
    return types_[position];
  }

  // Get the projection of commit ids onto the space of commit ids
  // belonging to |group|.  This is useful when you need to process a commit
  // response one ModelSafeGroup at a time. See GetCommitIdAt for how the
  // indices contained in the returned Projection can be used.
  const Projection& GetCommitIdProjection(
      ModelSafeGroup group) const;

  size_t Size() const {
    return metahandle_order_.size();
  }

  bool Empty() const {
    return Size() == 0;
  }

  // Returns all the types that are included in this list.
  ModelTypeSet Types() const {
    return types_in_list_;
  }

  // Returns true iff any of the commit ids added to this set have model type
  // BOOKMARKS.
  bool HasBookmarkCommitId() const;

  void Append(const OrderedCommitSet& other);
  void AppendReverse(const OrderedCommitSet& other);
  void Truncate(size_t max_size);

  // Removes all entries from this set.
  void Clear();

  void operator=(const OrderedCommitSet& other);
 private:
  // A set of CommitIdProjections associated with particular ModelSafeGroups.
  typedef std::map<ModelSafeGroup, Projection> Projections;

  // Helper container for return value of GetCommitItemAt.
  struct CommitItem {
    int64 meta;
    ModelType group;
  };

  CommitItem GetCommitItemAt(const size_t position) const;

  // These lists are different views of the same items; e.g they are
  // isomorphic.
  std::set<int64> inserted_metahandles_;
  std::vector<int64> metahandle_order_;
  Projections projections_;

  // We need this because of operations like AppendReverse that take ids from
  // one OrderedCommitSet and insert into another -- we need to know the
  // group for each ID so that the insertion can update the appropriate
  // projection.
  std::vector<ModelType> types_;

  // The set of types which are included in this particular list.
  ModelTypeSet types_in_list_;

  ModelSafeRoutingInfo routes_;
};

}  // namespace sessions
}  // namespace syncer

#endif  // SYNC_SESSIONS_ORDERED_COMMIT_SET_H_

