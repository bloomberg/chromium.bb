// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_MANAGER_H_
#define UI_ACCESSIBILITY_AX_TREE_MANAGER_H_

#include "base/macros.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree_id.h"

namespace ui {

class AXPlatformNodeDelegate;

// Each AXNode has access to its own tree, but a manager of multiple AXTrees
// is necessary for operations that span across trees.
class AX_EXPORT AXTreeManager {
 public:
  // Exposes the mapping between AXTreeID's and AXNodes based on a node_id.
  // This allows for callers to access nodes outside of their own tree.
  // Returns nullptr if the AXTreeID or node_id is not found.
  virtual AXNode* GetNodeFromTree(AXTreeID tree_id, int32_t node_id) = 0;

  // Exposes the mapping of AXPlatformNodeDelegate*'s from AXTreeID and
  // AXNodeID. This is non-static to allow for test code to override with
  // custom implementations.
  virtual AXPlatformNodeDelegate* GetDelegate(AXTreeID tree_id,
                                              int32_t node_id) = 0;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_MANAGER_H_
