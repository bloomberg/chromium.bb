// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"

namespace ui {

TEST(AXTreeTest, TestSerialize) {
  AXNodeData root;
  root.id = 1;
  root.role = AX_ROLE_ROOT_WEB_AREA;
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  AXNodeData button;
  button.id = 2;
  button.role = AX_ROLE_BUTTON;
  button.state = 0;

  AXNodeData checkbox;
  checkbox.id = 3;
  checkbox.role = AX_ROLE_CHECK_BOX;

  AXTreeUpdate initial_state;
  initial_state.nodes.push_back(root);
  initial_state.nodes.push_back(button);
  initial_state.nodes.push_back(checkbox);
  AXSerializableTree src_tree(initial_state);

  scoped_ptr<AXTreeSource<AXNode> > tree_source(
      src_tree.CreateTreeSource());
  AXTreeSerializer<AXNode> serializer(tree_source.get());
  AXTreeUpdate update;
  serializer.SerializeChanges(src_tree.GetRoot(), &update);

  AXTree dst_tree;
  ASSERT_TRUE(dst_tree.Unserialize(update));

  AXNode* root_node = dst_tree.GetRoot();
  ASSERT_TRUE(root_node != NULL);
  EXPECT_EQ(root.id, root_node->id());
  EXPECT_EQ(root.role, root_node->data().role);

  ASSERT_EQ(2, root_node->child_count());

  AXNode* button_node = root_node->ChildAtIndex(0);
  EXPECT_EQ(button.id, button_node->id());
  EXPECT_EQ(button.role, button_node->data().role);

  AXNode* checkbox_node = root_node->ChildAtIndex(1);
  EXPECT_EQ(checkbox.id, checkbox_node->id());
  EXPECT_EQ(checkbox.role, checkbox_node->data().role);
}

}  // namespace ui
