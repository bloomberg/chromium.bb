// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_PARENT_CHILD_INDEX
#define SYNC_SYNCABLE_PARENT_CHILD_INDEX

#include <map>
#include <set>

#include "base/basictypes.h"
#include "sync/base/sync_export.h"

namespace syncer {
namespace syncable {

struct EntryKernel;
class Id;
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
  const OrderedChildSet* GetChildren(const Id& id);

 private:
  typedef std::map<syncable::Id, OrderedChildSet*> ParentChildrenMap;

  // A map of parent IDs to children.
  // Parents with no children are not included in this map.
  ParentChildrenMap parent_children_map_;

  DISALLOW_COPY_AND_ASSIGN(ParentChildIndex);
};

}  // namespace syncable
}  // namespace syncer

#endif  // SYNC_SYNCABLE_PARENT_CHILD_INDEX
