// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/item_node.h"

#include <algorithm>

#include "base/callback.h"
#include "base/logging.h"
#include "tools/gn/item.h"

ItemNode::ItemNode(Item* i)
    : state_(REFERENCED),
      item_(i) {
}

ItemNode::~ItemNode() {
}

void ItemNode::AddDependency(ItemNode* node) {
  if (direct_dependencies_.find(node) != direct_dependencies_.end())
    return;  // Already have this dep.
  direct_dependencies_.insert(node);

  if (node->state() != RESOLVED) {
    // Wire up the pending resolution info.
    unresolved_dependencies_.insert(node);
    node->waiting_on_resolution_.insert(this);
  }
}

void ItemNode::MarkDirectDependencyResolved(ItemNode* node) {
  DCHECK(unresolved_dependencies_.find(node) != unresolved_dependencies_.end());
  unresolved_dependencies_.erase(node);
}

void ItemNode::SwapOutWaitingDependencySet(ItemNodeSet* out_set) {
  waiting_on_resolution_.swap(*out_set);
}

void ItemNode::SetGenerated() {
  state_ = GENERATED;
}

void ItemNode::SetResolved() {
  state_ = RESOLVED;

  if (!resolved_closure_.is_null())
    resolved_closure_.Run();
}
