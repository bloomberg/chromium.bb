// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_ITEM_TREE_H_
#define TOOLS_GN_ITEM_TREE_H_

#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "tools/gn/label.h"

class Err;
class Item;
class ItemNode;

// Represents the full dependency tree if labeled items in the system.
// Generally you will interact with this through the target manager, etc.
class ItemTree {
 public:
  ItemTree();
  ~ItemTree();

  // This lock must be held when calling the "Locked" functions below.
  base::Lock& lock() { return lock_; }

  // Returns NULL if the item is not found.
  //
  // The lock must be held.
  ItemNode* GetExistingNodeLocked(const Label& label);

  // There must not be an item with this label in the tree already. Takes
  // ownership of the pointer.
  //
  // The lock must be held.
  void AddNodeLocked(ItemNode* node);

  // Mark the given item as being generated. If it has no unresolved
  // dependencies, it will be marked resolved, and the resolved state will be
  // recursively pushed into the dependency tree. Returns an error if there was
  // an error.
  Err MarkItemGeneratedLocked(const Label& label);

  // Fills the given vector with all known items.
  void GetAllItemsLocked(std::vector<const Item*>* dest) const;

  // Returns an error if there are unresolved dependencies, or no error if
  // there aren't.
  //
  // The lock should not be held.
  Err CheckForBadItems() const;

 private:
  Err MarkItemResolvedLocked(ItemNode* node);

  // Given a set of unresolved nodes, looks for cycles and returns the error
  // message describing any cycles it found.
  std::string CheckForCircularDependenciesLocked(
      const std::vector<const ItemNode*>& bad_nodes) const;

  mutable base::Lock lock_;

  typedef base::hash_map<Label, ItemNode*> StringToNodeHash;
  StringToNodeHash items_;  // Owning pointer.

  DISALLOW_COPY_AND_ASSIGN(ItemTree);
};

#endif  // TOOLS_GN_ITEM_TREE_H_
