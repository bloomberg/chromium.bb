// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_language_info.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

// Tests that AXNode::GetLanguage() terminates when there is no lang attribute.
TEST(AXLanguageInfoTest, TestGetLanguageNoLangAttr) {
  /* build tree including parenting, this is to exercise the code paths within
   * AXNode::GetLanguage() which scan up the tree to handle lang inheritance.
   *      1
   *    2   3
   *  4
   */
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(4);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.resize(2);
  initial_state.nodes[0].child_ids[0] = 2;
  initial_state.nodes[0].child_ids[1] = 3;
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].child_ids.resize(1);
  initial_state.nodes[1].child_ids[0] = 4;
  initial_state.nodes[2].id = 3;
  initial_state.nodes[3].id = 4;
  AXTree tree(initial_state);

  // Check that tree parenting conforms to expected shape.
  AXNode* item1 = tree.GetFromId(1);
  EXPECT_EQ(item1->parent(), nullptr);

  AXNode* item2 = tree.GetFromId(2);
  EXPECT_EQ(item2->parent(), item1);
  EXPECT_EQ(item2->parent()->parent(), nullptr);

  AXNode* item3 = tree.GetFromId(3);
  EXPECT_EQ(item3->parent(), item1);
  EXPECT_EQ(item3->parent()->parent(), nullptr);

  AXNode* item4 = tree.GetFromId(4);
  EXPECT_EQ(item4->parent(), item2);
  EXPECT_EQ(item4->parent()->parent(), item1);
  EXPECT_EQ(item4->parent()->parent()->parent(), nullptr);

  std::string item1_lang = item1->GetLanguage();
  EXPECT_EQ(item1_lang, "");

  std::string item2_lang = item2->GetLanguage();
  EXPECT_EQ(item2_lang, "");

  std::string item3_lang = item3->GetLanguage();
  EXPECT_EQ(item3_lang, "");

  std::string item4_lang = item4->GetLanguage();
  EXPECT_EQ(item4_lang, "");
}

}  // namespace ui
