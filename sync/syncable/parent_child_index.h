// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_PARENT_CHILD_INDEX
#define SYNC_SYNCABLE_PARENT_CHILD_INDEX

#include <map>
#include <set>

#include "base/basictypes.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/syncable/syncable_id.h"

namespace syncer {
namespace syncable {

struct EntryKernel;
class ParentChildIndex;

// A node ordering function.
struct SYNC_EXPORT_PRIVATE ChildComparator {
  bool operator() (const EntryKernel* a, const EntryKernel* b) const;
};

// An ordered set of nodes.
typedef std::set<EntryKernel*, ChildComparator> OrderedChildSet;

// Container that tracks parent-child relationships.
// Provides fast lookup of all items under a given parent.
class SYNC_EXPORT_PRIVATE ParentChildIndex {
 public:
  ParentChildIndex();
  ~ParentChildIndex();

  // Returns whether or not this entry belongs in the index.
  // True for all non-deleted, non-root entries.
  static bool ShouldInclude(const EntryKernel* e);

  // Inserts a given child into the index.
  bool Insert(EntryKernel* e);

  // Removes a given child from the index.
  void Remove(EntryKernel* e);

  // Returns true if this item is in the index as a child.
  bool Contains(EntryKernel* e) const;

  // Returns all children of the entry with the given Id.  Returns NULL if the
  // node has no children or the Id does not identify a valid directory node.
  const OrderedChildSet* GetChildren(const Id& id) const;

  // Returns all children of the entry.  Returns NULL if the node has no
  // children.
  const OrderedChildSet* GetChildren(EntryKernel* e) const;

  // Returns all siblings of the entry.
  const OrderedChildSet* GetSiblings(EntryKernel* e) const;

 private:
  friend class ParentChildIndexTest;
  typedef std::map<Id, OrderedChildSet*> ParentChildrenMap;

  // Determines entry's model type.
  static ModelType GetModelType(EntryKernel* e);

  // Returns parent ID for the entry which is either its PARENT_ID value
  // or derived from its model type.
  const Id& GetParentId(EntryKernel* e) const;

  // Returns previously cached model type root ID for the given |model_type|.
  const Id& GetModelTypeRootId(ModelType model_type) const;

  // A map of parent IDs to children.
  // Parents with no children are not included in this map.
  ParentChildrenMap parent_children_map_;

  // This array tracks model type roots IDs.
  Id model_type_root_ids_[MODEL_TYPE_COUNT];

  DISALLOW_COPY_AND_ASSIGN(ParentChildIndex);
};

}  // namespace syncable
}  // namespace syncer

#endif  // SYNC_SYNCABLE_PARENT_CHILD_INDEX
