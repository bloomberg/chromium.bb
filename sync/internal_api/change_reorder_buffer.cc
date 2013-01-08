// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/change_reorder_buffer.h"

#include <limits>
#include <queue>
#include <set>
#include <utility>  // for pair<>

#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/syncable_base_transaction.h"

using std::numeric_limits;
using std::pair;
using std::queue;
using std::set;

namespace syncer {

// Traversal provides a way to collect a set of nodes from the syncable
// directory structure and then traverse them, along with any intermediate
// nodes, in a top-down fashion, starting from a single common ancestor.  A
// Traversal starts out empty and is grown by means of the ExpandToInclude
// method.  Once constructed, the top(), begin_children(), and end_children()
// methods can be used to explore the nodes in root-to-leaf order.
class ChangeReorderBuffer::Traversal {
 public:
  typedef pair<int64, int64> ParentChildLink;
  typedef set<ParentChildLink> LinkSet;

  Traversal() : top_(kInvalidId) { }

  // Expand the traversal so that it includes the node indicated by
  // |child_handle|.
  void ExpandToInclude(syncable::BaseTransaction* trans,
                       int64 child_handle) {
    // If |top_| is invalid, this is the first insertion -- easy.
    if (top_ == kInvalidId) {
      top_ = child_handle;
      return;
    }

    int64 node_to_include = child_handle;
    while (node_to_include != kInvalidId && node_to_include != top_) {
      int64 node_parent = 0;

      syncable::Entry node(trans, syncable::GET_BY_HANDLE, node_to_include);
      CHECK(node.good());
      if (node.Get(syncable::ID).IsRoot()) {
        // If we've hit the root, and the root isn't already in the tree
        // (it would have to be |top_| if it were), start a new expansion
        // upwards from |top_| to unite the original traversal with the
        // path we just added that goes from |child_handle| to the root.
        node_to_include = top_;
        top_ = node.Get(syncable::META_HANDLE);
      } else {
        // Otherwise, get the parent ID so that we can add a ParentChildLink.
        syncable::Entry parent(trans, syncable::GET_BY_ID,
                             node.Get(syncable::PARENT_ID));
        CHECK(parent.good());
        node_parent = parent.Get(syncable::META_HANDLE);

        ParentChildLink link(node_parent, node_to_include);

        // If the link exists in the LinkSet |links_|, we don't need to search
        // any higher; we are done.
        if (links_.find(link) != links_.end())
          return;

        // Otherwise, extend |links_|, and repeat on the parent.
        links_.insert(link);
        node_to_include = node_parent;
      }
    }
  }

  // Return the top node of the traversal.  Use this as a starting point
  // for walking the tree.
  int64 top() const { return top_; }

  // Return an iterator corresponding to the first child (in the traversal)
  // of the node specified by |parent_id|.  Iterate this return value until
  // it is equal to the value returned by end_children(parent_id).  The
  // enumeration thus provided is unordered.
  LinkSet::const_iterator begin_children(int64 parent_id) const {
    return links_.upper_bound(
        ParentChildLink(parent_id, numeric_limits<int64>::min()));
  }

  // Return an iterator corresponding to the last child in the traversal
  // of the node specified by |parent_id|.
  LinkSet::const_iterator end_children(int64 parent_id) const {
    return begin_children(parent_id + 1);
  }

 private:
  // The topmost point in the directory hierarchy that is in the traversal,
  // and thus the first node to be traversed.  If the traversal is empty,
  // this is kInvalidId.  If the traversal contains exactly one member, |top_|
  // will be the solitary member, and |links_| will be empty.
  int64 top_;
  // A set of single-level links that compose the traversal below |top_|.  The
  // (parent, child) ordering of values enables efficient lookup of children
  // given the parent handle, which is used for top-down traversal.  |links_|
  // is expected to be connected -- every node that appears as a parent in a
  // link must either appear as a child of another link, or else be the
  // topmost node, |top_|.
  LinkSet links_;

  DISALLOW_COPY_AND_ASSIGN(Traversal);
};

ChangeReorderBuffer::ChangeReorderBuffer() {
}

ChangeReorderBuffer::~ChangeReorderBuffer() {
}

bool ChangeReorderBuffer::GetAllChangesInTreeOrder(
    const BaseTransaction* sync_trans,
    ImmutableChangeRecordList* changes) {
  syncable::BaseTransaction* trans = sync_trans->GetWrappedTrans();

  // Step 1: Iterate through the operations, doing three things:
  // (a) Push deleted items straight into the |changelist|.
  // (b) Construct a traversal spanning all non-deleted items.
  // (c) Construct a set of all parent nodes of any position changes.
  set<int64> parents_of_position_changes;
  Traversal traversal;

  ChangeRecordList changelist;

  OperationMap::const_iterator i;
  for (i = operations_.begin(); i != operations_.end(); ++i) {
    if (i->second == OP_DELETE) {
      ChangeRecord record;
      record.id = i->first;
      record.action = ChangeRecord::ACTION_DELETE;
      if (specifics_.find(record.id) != specifics_.end())
        record.specifics = specifics_[record.id];
      if (extra_data_.find(record.id) != extra_data_.end())
        record.extra = extra_data_[record.id];
      changelist.push_back(record);
    } else {
      traversal.ExpandToInclude(trans, i->first);
      if (i->second == OP_ADD ||
          i->second == OP_UPDATE_POSITION_AND_PROPERTIES) {
        ReadNode node(sync_trans);
        CHECK_EQ(BaseNode::INIT_OK, node.InitByIdLookup(i->first));

        // We only care about parents of entry's with position-sensitive models.
        if (ShouldMaintainPosition(node.GetEntry()->GetModelType())) {
          parents_of_position_changes.insert(node.GetParentId());
        }
      }
    }
  }

  // Step 2: Breadth-first expansion of the traversal, enumerating children in
  // the syncable sibling order if there were any position updates.
  queue<int64> to_visit;
  to_visit.push(traversal.top());
  while (!to_visit.empty()) {
    int64 next = to_visit.front();
    to_visit.pop();

    // If the node has an associated action, output a change record.
    i = operations_.find(next);
    if (i != operations_.end()) {
      ChangeRecord record;
      record.id = next;
      if (i->second == OP_ADD)
        record.action = ChangeRecord::ACTION_ADD;
      else
        record.action = ChangeRecord::ACTION_UPDATE;
      if (specifics_.find(record.id) != specifics_.end())
        record.specifics = specifics_[record.id];
      if (extra_data_.find(record.id) != extra_data_.end())
        record.extra = extra_data_[record.id];
      changelist.push_back(record);
    }

    // Now add the children of |next| to |to_visit|.
    if (parents_of_position_changes.find(next) ==
        parents_of_position_changes.end()) {
        // No order changes on this parent -- traverse only the nodes listed
        // in the traversal (and not in sibling order).
        Traversal::LinkSet::const_iterator j = traversal.begin_children(next);
        Traversal::LinkSet::const_iterator end = traversal.end_children(next);
        for (; j != end; ++j) {
          CHECK(j->first == next);
          to_visit.push(j->second);
        }
    } else {
      // There were ordering changes on the children of this parent, so
      // enumerate all the children in the sibling order.
      syncable::Entry parent(trans, syncable::GET_BY_HANDLE, next);
      syncable::Id id;
      if (!trans->directory()->GetFirstChildId(
              trans, parent.Get(syncable::ID), &id)) {
        *changes = ImmutableChangeRecordList();
        return false;
      }
      while (!id.IsRoot()) {
        syncable::Entry child(trans, syncable::GET_BY_ID, id);
        CHECK(child.good());
        int64 handle = child.Get(syncable::META_HANDLE);
        to_visit.push(handle);
        // If there is no operation on this child node, record it as as an
        // update, so that the listener gets notified of all nodes in the new
        // ordering.
        if (operations_.find(handle) == operations_.end())
          operations_[handle] = OP_UPDATE_POSITION_AND_PROPERTIES;
        id = child.GetSuccessorId();
      }
    }
  }

  *changes = ImmutableChangeRecordList(&changelist);
  return true;
}

}  // namespace syncer
