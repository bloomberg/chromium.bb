// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TREE_TREE_VIEW_CONTROLLER_H_
#define UI_VIEWS_CONTROLS_TREE_TREE_VIEW_CONTROLLER_H_
#pragma once

#include "ui/base/keycodes/keyboard_codes.h"

namespace ui {
class TreeModelNode;
}

namespace views {

class TreeView;

// TreeViewController ---------------------------------------------------------

// Controller for the treeview.
class TreeViewController {
 public:
  // Notification that the selection of the tree view has changed. Use
  // GetSelectedNode to find the current selection.
  virtual void OnTreeViewSelectionChanged(TreeView* tree_view) = 0;

  // Returns true if the node can be edited. This is only used if the
  // TreeView is editable.
  virtual bool CanEdit(TreeView* tree_view, ui::TreeModelNode* node);

  // Invoked when a key is pressed on the tree view.
  virtual void OnTreeViewKeyDown(ui::KeyboardCode keycode);

 protected:
  virtual ~TreeViewController();
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TREE_TREE_VIEW_CONTROLLER_H_
