// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_ITEM_H_
#define TOOLS_GN_ITEM_H_

#include <string>

#include "tools/gn/label.h"

class Config;
class ItemNode;
class Target;
class Toolchain;

// A named item (target, config, etc.) that participates in the dependency
// graph.
class Item {
 public:
  Item(const Label& label);
  virtual ~Item();

  // This is guaranteed to never change after construction so this can be
  // accessed from any thread with no locking once the item is constructed.
  const Label& label() const { return label_; }

  // The Item and the ItemNode make a pair. This will be set when the ItemNode
  // is constructed that owns this Item. The ItemNode should only be
  // dereferenced with the ItemTree lock held.
  ItemNode* item_node() { return item_node_; }
  const ItemNode* item_node() const { return item_node_; }
  void set_item_node(ItemNode* in) { item_node_ = in; }

  // Manual RTTI.
  virtual Config* AsConfig();
  virtual const Config* AsConfig() const;
  virtual Target* AsTarget();
  virtual const Target* AsTarget() const;
  virtual Toolchain* AsToolchain();
  virtual const Toolchain* AsToolchain() const;

  // Returns a name like "target" or "config" for the type of item this is, to
  // be used in logging and error messages.
  std::string GetItemTypeName() const;

  // Called when this item is resolved, meaning it and all of its dependents
  // have no unresolved deps.
  virtual void OnResolved() {}

 private:
  Label label_;

  ItemNode* item_node_;
};

#endif  // TOOLS_GN_ITEM_H_
