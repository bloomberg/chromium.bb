// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_TREE_VIEW_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_TREE_VIEW_EXAMPLE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/models/tree_node_model.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/tree/tree_view_controller.h"
#include "ui/views/examples/example_base.h"

namespace views {

class TreeView;

namespace examples {

class TreeViewExample : public ExampleBase,
                        public ButtonListener,
                        public TreeViewController {
 public:
  TreeViewExample();
  virtual ~TreeViewExample();

  // ExampleBase:
  virtual void CreateExampleView(View* container) OVERRIDE;

 private:
  // ButtonListener:
  virtual void ButtonPressed(Button* sender, const Event& event) OVERRIDE;

  // TreeViewController:
  virtual void OnTreeViewSelectionChanged(TreeView* tree_view) OVERRIDE;
  virtual bool CanEdit(TreeView* tree_view, ui::TreeModelNode* node) OVERRIDE;

  // The tree view to be tested.
  TreeView* tree_view_;

  // Control buttons to modify the model.
  Button* add_;
  Button* remove_;
  Button* change_title_;

  typedef ui::TreeNodeWithValue<int> NodeType;

  ui::TreeNodeModel<NodeType> model_;

  DISALLOW_COPY_AND_ASSIGN(TreeViewExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_TREE_VIEW_EXAMPLE_H_
