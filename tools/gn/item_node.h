// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_ITEM_NODE_H_
#define TOOLS_GN_ITEM_NODE_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "tools/gn/location.h"

class Item;

// Represents a node in the depdency tree. It references an Item which is
// the actual thing.
//
// The items and nodes are split apart so that the ItemTree can manipulate
// the dependencies one one thread while the Item itself is been modified on
// another.
class ItemNode {
 public:
  enum State {
    // Another item has referenced this one by name, but we have not yet
    // encountered this item to know what it is.
    REFERENCED,

    // This item has been defined but some of the dependencies it references
    // have not been.
    GENERATED,

    // All of this item's transitive dependencies have been found and
    // resolved.
    RESOLVED,
  };

  typedef std::set<ItemNode*> ItemNodeSet;

  // Takes ownership of the pointer.
  // Initial state will be REFERENCED.
  ItemNode(Item* i);
  ~ItemNode();

  State state() const { return state_; }

  // This closure will be executed when the item is resolved.
  void set_resolved_closure(const base::Closure& closure) {
    resolved_closure_ = closure;
  }

  const Item* item() const { return item_.get(); }
  Item* item() { return item_.get(); }

  // Where this was created from, which might be when it was generated or
  // when it was first referenced from another target.
  const LocationRange& originally_referenced_from_here() const {
    return originally_referenced_from_here_;
  }
  void set_originally_referenced_from_here(const LocationRange& r) {
    originally_referenced_from_here_ = r;
  }

  // Where this was generated from. This will be empty for items that have
  // been referenced but not generated. Note that this has to be one the
  // ItemNode because it can be changing from multiple threads and we need
  // to be sure that access is serialized.
  const LocationRange& generated_from_here() const {
    return generated_from_here_;
  }
  void set_generated_from_here(const LocationRange& r) {
    generated_from_here_ = r;
  }

  const ItemNodeSet& direct_dependencies() const {
    return direct_dependencies_;
  }
  const ItemNodeSet& unresolved_dependencies() const {
    return unresolved_dependencies_;
  }

  void AddDependency(ItemNode* node);

  // Removes the given dependency from the unresolved list. Does not do
  // anything else to update waiters.
  void MarkDirectDependencyResolved(ItemNode* node);

  // Destructively retrieve the set of waiting nodes.
  void SwapOutWaitingDependencySet(ItemNodeSet* out_set);

  void SetGenerated();
  void SetResolved();

 private:
  State state_;
  scoped_ptr<Item> item_;

  LocationRange originally_referenced_from_here_;
  LocationRange generated_from_here_;

  // What to run when this item is resolved.
  base::Closure resolved_closure_;

  // Everything this item directly depends on.
  ItemNodeSet direct_dependencies_;

  // Unresolved things this item directly depends on.
  ItemNodeSet unresolved_dependencies_;

  // These items are waiting on us to be resolved before they can be
  // resolved. This is the backpointer for unresolved_dependencies_.
  ItemNodeSet waiting_on_resolution_;

  DISALLOW_COPY_AND_ASSIGN(ItemNode);
};

#endif  // TOOLS_GN_ITEM_NODE_H_
