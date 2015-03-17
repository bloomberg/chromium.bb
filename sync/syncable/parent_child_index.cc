// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/parent_child_index.h"

#include "base/stl_util.h"

#include "sync/syncable/entry_kernel.h"
#include "sync/syncable/syncable_id.h"

namespace syncer {
namespace syncable {

bool ChildComparator::operator()(const EntryKernel* a,
                                 const EntryKernel* b) const {
  const UniquePosition& a_pos = a->ref(UNIQUE_POSITION);
  const UniquePosition& b_pos = b->ref(UNIQUE_POSITION);

  if (a_pos.IsValid() && b_pos.IsValid()) {
    // Position is important to this type.
    return a_pos.LessThan(b_pos);
  } else if (a_pos.IsValid() && !b_pos.IsValid()) {
    // TODO(rlarocque): Remove this case.
    // An item with valid position as sibling of one with invalid position.
    // We should not support this, but the tests rely on it.  For now, just
    // move all invalid position items to the right.
    return true;
  } else if (!a_pos.IsValid() && b_pos.IsValid()) {
    // TODO(rlarocque): Remove this case.
    // Mirror of the above case.
    return false;
  } else {
    // Position doesn't matter.
    DCHECK(!a->ref(UNIQUE_POSITION).IsValid());
    DCHECK(!b->ref(UNIQUE_POSITION).IsValid());
    return a->ref(ID) < b->ref(ID);
  }
}

ParentChildIndex::ParentChildIndex() {
}

ParentChildIndex::~ParentChildIndex() {
  STLDeleteContainerPairSecondPointers(
      parent_children_map_.begin(), parent_children_map_.end());
}

bool ParentChildIndex::ShouldInclude(const EntryKernel* entry) {
  // This index excludes deleted items and the root item.  The root
  // item is excluded so that it doesn't show up as a child of itself.
  return !entry->ref(IS_DEL) && !entry->ref(ID).IsRoot();
}

bool ParentChildIndex::Insert(EntryKernel* entry) {
  DCHECK(ShouldInclude(entry));

  const Id& parent_id = GetParentId(entry);
  // Store type root ID when inserting a type root entry.
  if (parent_id.IsRoot()) {
    ModelType model_type = GetModelType(entry);
    // TODO(stanisc): there are some unit tests that create entries
    // at the root and don't bother initializing specifics which
    // produces TOP_LEVEL_FOLDER type here.
    if (syncer::IsRealDataType(model_type)) {
      model_type_root_ids_[model_type] = entry->ref(ID);
    }
  }

  OrderedChildSet* children = NULL;
  ParentChildrenMap::iterator i = parent_children_map_.find(parent_id);
  if (i != parent_children_map_.end()) {
    children = i->second;
  } else {
    children = new OrderedChildSet();
    parent_children_map_.insert(std::make_pair(parent_id, children));
  }

  return children->insert(entry).second;
}

// Like the other containers used to help support the syncable::Directory, this
// one does not own any EntryKernels.  This function removes references to the
// given EntryKernel but does not delete it.
void ParentChildIndex::Remove(EntryKernel* e) {
  const Id& parent_id = GetParentId(e);
  // Clear type root ID when removing a type root entry.
  if (parent_id.IsRoot()) {
    ModelType model_type = GetModelType(e);
    // TODO(stanisc): the check is needed to work around some tests.
    // See TODO above.
    if (model_type_root_ids_[model_type] == e->ref(ID)) {
      model_type_root_ids_[model_type] = Id();
    }
  }

  ParentChildrenMap::iterator parent = parent_children_map_.find(parent_id);
  DCHECK(parent != parent_children_map_.end());

  OrderedChildSet* children = parent->second;
  OrderedChildSet::iterator j = children->find(e);
  DCHECK(j != children->end());

  children->erase(j);
  if (children->empty()) {
    delete children;
    parent_children_map_.erase(parent);
  }
}

bool ParentChildIndex::Contains(EntryKernel *e) const {
  const Id& parent_id = GetParentId(e);
  ParentChildrenMap::const_iterator parent =
      parent_children_map_.find(parent_id);
  if (parent == parent_children_map_.end()) {
    return false;
  }
  const OrderedChildSet* children = parent->second;
  DCHECK(children && !children->empty());
  return children->count(e) > 0;
}

const OrderedChildSet* ParentChildIndex::GetChildren(const Id& id) const {
  DCHECK(!id.IsNull());

  ParentChildrenMap::const_iterator parent = parent_children_map_.find(id);
  if (parent == parent_children_map_.end()) {
    return NULL;
  }

  // A successful lookup implies at least some children exist.
  DCHECK(!parent->second->empty());
  return parent->second;
}

const OrderedChildSet* ParentChildIndex::GetChildren(EntryKernel* e) const {
  return GetChildren(e->ref(ID));
}

const OrderedChildSet* ParentChildIndex::GetSiblings(EntryKernel* e) const {
  DCHECK(Contains(e));
  const OrderedChildSet* siblings = GetChildren(GetParentId(e));
  DCHECK(siblings && !siblings->empty());
  return siblings;
}

const Id& ParentChildIndex::GetParentId(EntryKernel* e) const {
  const Id& parent_id = e->ref(PARENT_ID);
  if (!parent_id.IsNull()) {
    return parent_id;
  }
  return GetModelTypeRootId(GetModelType(e));
}

ModelType ParentChildIndex::GetModelType(EntryKernel* e) {
  // TODO(stanisc): is there a more effective way to find out model type?
  ModelType model_type = e->GetModelType();
  if (!syncer::IsRealDataType(model_type)) {
    model_type = e->GetServerModelType();
  }
  return model_type;
}

const Id& ParentChildIndex::GetModelTypeRootId(ModelType model_type) const {
  // TODO(stanisc): Review whether this approach is reliable enough.
  // Should this method simply enumerate children of root node ("r") instead?
  return model_type_root_ids_[model_type];
}

}  // namespace syncable
}  // namespace syncer
