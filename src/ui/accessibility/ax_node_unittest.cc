// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node.h"

#include <memory>
#include <utility>
#include <vector>

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/test_ax_tree_manager.h"

namespace ui {

namespace {

// The third argument is an optional description string which we don't need
// because, after verifying manually, test errors are descriptive enough.
MATCHER_P(HasAXNodeID, ax_node_data, "") {
  return arg->id() == ax_node_data.id;
}

}  // namespace

using testing::ElementsAre;

TEST(AXNodeTest, TreeWalking) {
  // ++kRootWebArea
  // ++++kParagraph
  // ++++++kStaticText IGNORED
  // ++++kParagraph IGNORED
  // ++++++kStaticText
  // ++++kParagraph
  // ++++++kStaticText
  // ++++kParagraph IGNORED
  // ++++++kLink IGNORED
  // ++++++++kStaticText
  // ++++++kButton

  // Numbers at the end of variable names indicate their position under the
  // root.
  AXNodeData root;
  AXNodeData paragraph_0;
  AXNodeData static_text_0_0_ignored;
  AXNodeData paragraph_1_ignored;
  AXNodeData static_text_1_0;
  AXNodeData paragraph_2;
  AXNodeData static_text_2_0;
  AXNodeData paragraph_3_ignored;
  AXNodeData link_3_0_ignored;
  AXNodeData static_text_3_0_0;
  AXNodeData button_3_1;

  root.id = 1;
  paragraph_0.id = 2;
  static_text_0_0_ignored.id = 3;
  paragraph_1_ignored.id = 4;
  static_text_1_0.id = 5;
  paragraph_2.id = 6;
  static_text_2_0.id = 7;
  paragraph_3_ignored.id = 8;
  link_3_0_ignored.id = 9;
  static_text_3_0_0.id = 10;
  button_3_1.id = 11;

  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {paragraph_0.id, paragraph_1_ignored.id, paragraph_2.id,
                    paragraph_3_ignored.id};

  paragraph_0.role = ax::mojom::Role::kParagraph;
  paragraph_0.child_ids = {static_text_0_0_ignored.id};

  static_text_0_0_ignored.role = ax::mojom::Role::kStaticText;
  static_text_0_0_ignored.AddState(ax::mojom::State::kIgnored);
  static_text_0_0_ignored.SetName("static_text_0_0_ignored");

  paragraph_1_ignored.role = ax::mojom::Role::kParagraph;
  paragraph_1_ignored.AddState(ax::mojom::State::kIgnored);
  paragraph_1_ignored.child_ids = {static_text_1_0.id};

  static_text_1_0.role = ax::mojom::Role::kStaticText;
  static_text_1_0.SetName("static_text_1_0");

  paragraph_2.role = ax::mojom::Role::kParagraph;
  paragraph_2.child_ids = {static_text_2_0.id};

  static_text_2_0.role = ax::mojom::Role::kStaticText;
  static_text_2_0.SetName("static_text_2_0");

  paragraph_3_ignored.role = ax::mojom::Role::kParagraph;
  paragraph_3_ignored.AddState(ax::mojom::State::kIgnored);
  paragraph_3_ignored.child_ids = {link_3_0_ignored.id, button_3_1.id};

  link_3_0_ignored.role = ax::mojom::Role::kLink;
  link_3_0_ignored.AddState(ax::mojom::State::kLinked);
  link_3_0_ignored.AddState(ax::mojom::State::kIgnored);
  link_3_0_ignored.child_ids = {static_text_3_0_0.id};

  static_text_3_0_0.role = ax::mojom::Role::kStaticText;
  static_text_3_0_0.SetName("static_text_3_0_0");

  button_3_1.role = ax::mojom::Role::kButton;
  button_3_1.SetName("button_3_1");

  AXTreeUpdate initial_state;
  initial_state.root_id = root.id;
  initial_state.nodes = {root,
                         paragraph_0,
                         static_text_0_0_ignored,
                         paragraph_1_ignored,
                         static_text_1_0,
                         paragraph_2,
                         static_text_2_0,
                         paragraph_3_ignored,
                         link_3_0_ignored,
                         static_text_3_0_0,
                         button_3_1};
  initial_state.has_tree_data = true;

  AXTreeData tree_data;
  tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
  tree_data.title = "Application";
  initial_state.tree_data = tree_data;

  AXTree tree;
  ASSERT_TRUE(tree.Unserialize(initial_state)) << tree.error();

  const AXNode* root_node = tree.root();
  ASSERT_EQ(root.id, root_node->id());

  EXPECT_THAT(
      root_node->GetAllChildren(),
      ElementsAre(HasAXNodeID(paragraph_0), HasAXNodeID(paragraph_1_ignored),
                  HasAXNodeID(paragraph_2), HasAXNodeID(paragraph_3_ignored)));
  EXPECT_EQ(4u, root_node->GetChildCount());
  EXPECT_EQ(4u, root_node->GetChildCountCrossingTreeBoundary());
  EXPECT_EQ(5u, root_node->GetUnignoredChildCount());
  EXPECT_EQ(5u, root_node->GetUnignoredChildCountCrossingTreeBoundary());

  EXPECT_EQ(paragraph_0.id, root_node->GetChildAtIndex(0)->id());
  EXPECT_EQ(paragraph_1_ignored.id, root_node->GetChildAtIndex(1)->id());
  EXPECT_EQ(paragraph_2.id, root_node->GetChildAtIndex(2)->id());
  EXPECT_EQ(paragraph_3_ignored.id, root_node->GetChildAtIndex(3)->id());
  EXPECT_EQ(nullptr, root_node->GetChildAtIndex(4));

  EXPECT_EQ(paragraph_0.id,
            root_node->GetChildAtIndexCrossingTreeBoundary(0)->id());
  EXPECT_EQ(paragraph_1_ignored.id,
            root_node->GetChildAtIndexCrossingTreeBoundary(1)->id());
  EXPECT_EQ(paragraph_2.id,
            root_node->GetChildAtIndexCrossingTreeBoundary(2)->id());
  EXPECT_EQ(paragraph_3_ignored.id,
            root_node->GetChildAtIndexCrossingTreeBoundary(3)->id());
  EXPECT_EQ(nullptr, root_node->GetChildAtIndexCrossingTreeBoundary(4));

  EXPECT_EQ(paragraph_0.id, root_node->GetUnignoredChildAtIndex(0)->id());
  EXPECT_EQ(static_text_1_0.id, root_node->GetUnignoredChildAtIndex(1)->id());
  EXPECT_EQ(paragraph_2.id, root_node->GetUnignoredChildAtIndex(2)->id());
  EXPECT_EQ(static_text_3_0_0.id, root_node->GetUnignoredChildAtIndex(3)->id());
  EXPECT_EQ(button_3_1.id, root_node->GetUnignoredChildAtIndex(4)->id());
  EXPECT_EQ(nullptr, root_node->GetUnignoredChildAtIndex(5));

  EXPECT_EQ(paragraph_0.id,
            root_node->GetUnignoredChildAtIndexCrossingTreeBoundary(0)->id());
  EXPECT_EQ(static_text_1_0.id,
            root_node->GetUnignoredChildAtIndexCrossingTreeBoundary(1)->id());
  EXPECT_EQ(paragraph_2.id,
            root_node->GetUnignoredChildAtIndexCrossingTreeBoundary(2)->id());
  EXPECT_EQ(static_text_3_0_0.id,
            root_node->GetUnignoredChildAtIndexCrossingTreeBoundary(3)->id());
  EXPECT_EQ(button_3_1.id,
            root_node->GetUnignoredChildAtIndexCrossingTreeBoundary(4)->id());
  EXPECT_EQ(nullptr,
            root_node->GetUnignoredChildAtIndexCrossingTreeBoundary(5));

  EXPECT_EQ(nullptr, root_node->GetParent());
  EXPECT_EQ(nullptr, root_node->GetParentCrossingTreeBoundary());
  EXPECT_EQ(nullptr, root_node->GetUnignoredParent());
  EXPECT_EQ(nullptr, root_node->GetUnignoredParentCrossingTreeBoundary());

  EXPECT_EQ(root_node, tree.GetFromId(paragraph_0.id)->GetParent());
  EXPECT_EQ(root_node,
            tree.GetFromId(paragraph_0.id)->GetParentCrossingTreeBoundary());
  EXPECT_EQ(root_node, tree.GetFromId(paragraph_0.id)->GetUnignoredParent());
  EXPECT_EQ(
      root_node,
      tree.GetFromId(paragraph_0.id)->GetUnignoredParentCrossingTreeBoundary());

  EXPECT_EQ(tree.GetFromId(paragraph_1_ignored.id),
            tree.GetFromId(static_text_1_0.id)->GetParent());
  EXPECT_EQ(
      tree.GetFromId(paragraph_1_ignored.id),
      tree.GetFromId(static_text_1_0.id)->GetParentCrossingTreeBoundary());
  EXPECT_EQ(root_node,
            tree.GetFromId(static_text_1_0.id)->GetUnignoredParent());
  EXPECT_EQ(root_node, tree.GetFromId(static_text_1_0.id)
                           ->GetUnignoredParentCrossingTreeBoundary());

  EXPECT_EQ(0u, root_node->GetIndexInParent());
  EXPECT_EQ(0u, root_node->GetUnignoredIndexInParent());

  EXPECT_EQ(2u, tree.GetFromId(paragraph_2.id)->GetIndexInParent());
  EXPECT_EQ(1u,
            tree.GetFromId(static_text_1_0.id)->GetUnignoredIndexInParent());
  EXPECT_EQ(2u, tree.GetFromId(paragraph_2.id)->GetUnignoredIndexInParent());

  EXPECT_EQ(paragraph_0.id, root_node->GetFirstChild()->id());
  EXPECT_EQ(paragraph_0.id,
            root_node->GetFirstChildCrossingTreeBoundary()->id());
  EXPECT_EQ(paragraph_0.id, root_node->GetFirstUnignoredChild()->id());
  EXPECT_EQ(paragraph_0.id,
            root_node->GetFirstUnignoredChildCrossingTreeBoundary()->id());

  EXPECT_EQ(paragraph_3_ignored.id, root_node->GetLastChild()->id());
  EXPECT_EQ(paragraph_3_ignored.id,
            root_node->GetLastChildCrossingTreeBoundary()->id());
  EXPECT_EQ(button_3_1.id, root_node->GetLastUnignoredChild()->id());
  EXPECT_EQ(button_3_1.id,
            root_node->GetLastUnignoredChildCrossingTreeBoundary()->id());

  EXPECT_EQ(static_text_0_0_ignored.id,
            root_node->GetDeepestFirstChild()->id());
  EXPECT_EQ(paragraph_0.id, root_node->GetDeepestFirstUnignoredChild()->id());

  EXPECT_EQ(button_3_1.id, root_node->GetDeepestLastChild()->id());
  EXPECT_EQ(button_3_1.id, root_node->GetDeepestLastUnignoredChild()->id());

  {
    std::vector<const AXNode*> siblings;
    for (const AXNode* sibling = tree.GetFromId(paragraph_0.id); sibling;
         sibling = sibling->GetNextSibling()) {
      siblings.push_back(sibling);
    }
    EXPECT_THAT(siblings, ElementsAre(HasAXNodeID(paragraph_0),
                                      HasAXNodeID(paragraph_1_ignored),
                                      HasAXNodeID(paragraph_2),
                                      HasAXNodeID(paragraph_3_ignored)));
  }

  {
    std::vector<const AXNode*> siblings;
    for (const AXNode* sibling = tree.GetFromId(paragraph_0.id); sibling;
         sibling = sibling->GetNextUnignoredSibling()) {
      siblings.push_back(sibling);
    }
    EXPECT_THAT(
        siblings,
        ElementsAre(HasAXNodeID(paragraph_0), HasAXNodeID(static_text_1_0),
                    HasAXNodeID(paragraph_2), HasAXNodeID(static_text_3_0_0),
                    HasAXNodeID(button_3_1)));
  }

  {
    std::vector<const AXNode*> siblings;
    for (const AXNode* sibling = tree.GetFromId(paragraph_3_ignored.id);
         sibling; sibling = sibling->GetPreviousSibling()) {
      siblings.push_back(sibling);
    }
    EXPECT_THAT(siblings, ElementsAre(HasAXNodeID(paragraph_3_ignored),
                                      HasAXNodeID(paragraph_2),
                                      HasAXNodeID(paragraph_1_ignored),
                                      HasAXNodeID(paragraph_0)));
  }

  {
    std::vector<const AXNode*> siblings;
    for (const AXNode* sibling = tree.GetFromId(button_3_1.id); sibling;
         sibling = sibling->GetPreviousUnignoredSibling()) {
      siblings.push_back(sibling);
    }
    EXPECT_THAT(
        siblings,
        ElementsAre(HasAXNodeID(button_3_1), HasAXNodeID(static_text_3_0_0),
                    HasAXNodeID(paragraph_2), HasAXNodeID(static_text_1_0),
                    HasAXNodeID(paragraph_0)));
  }

  {
    std::vector<const AXNode::AllChildIterator> siblings;
    for (auto iter = root_node->AllChildrenBegin();
         iter != root_node->AllChildrenEnd(); ++iter) {
      siblings.push_back(iter);
    }
    EXPECT_THAT(siblings, ElementsAre(HasAXNodeID(paragraph_0),
                                      HasAXNodeID(paragraph_1_ignored),
                                      HasAXNodeID(paragraph_2),
                                      HasAXNodeID(paragraph_3_ignored)));
  }

  {
    std::vector<const AXNode::AllChildCrossingTreeBoundaryIterator> siblings;
    for (auto iter = root_node->AllChildrenCrossingTreeBoundaryBegin();
         iter != root_node->AllChildrenCrossingTreeBoundaryEnd(); ++iter) {
      siblings.push_back(iter);
    }
    EXPECT_THAT(siblings, ElementsAre(HasAXNodeID(paragraph_0),
                                      HasAXNodeID(paragraph_1_ignored),
                                      HasAXNodeID(paragraph_2),
                                      HasAXNodeID(paragraph_3_ignored)));
  }

  {
    std::vector<const AXNode::UnignoredChildIterator> siblings;
    for (auto iter = root_node->UnignoredChildrenBegin();
         iter != root_node->UnignoredChildrenEnd(); ++iter) {
      siblings.push_back(iter);
    }
    EXPECT_THAT(
        siblings,
        ElementsAre(HasAXNodeID(paragraph_0), HasAXNodeID(static_text_1_0),
                    HasAXNodeID(paragraph_2), HasAXNodeID(static_text_3_0_0),
                    HasAXNodeID(button_3_1)));
  }

  {
    std::vector<const AXNode::UnignoredChildCrossingTreeBoundaryIterator>
        siblings;
    for (auto iter = root_node->UnignoredChildrenCrossingTreeBoundaryBegin();
         iter != root_node->UnignoredChildrenCrossingTreeBoundaryEnd();
         ++iter) {
      siblings.push_back(iter);
    }
    EXPECT_THAT(
        siblings,
        ElementsAre(HasAXNodeID(paragraph_0), HasAXNodeID(static_text_1_0),
                    HasAXNodeID(paragraph_2), HasAXNodeID(static_text_3_0_0),
                    HasAXNodeID(button_3_1)));
  }
}

TEST(AXNodeTest, TreeWalkingCrossingTreeBoundary) {
  AXTreeData tree_data_1;
  tree_data_1.tree_id = AXTreeID::CreateNewAXTreeID();
  tree_data_1.title = "Application";

  AXTreeData tree_data_2;
  tree_data_2.tree_id = AXTreeID::CreateNewAXTreeID();
  tree_data_2.parent_tree_id = tree_data_1.tree_id;
  tree_data_2.title = "Iframe";

  AXNodeData root_1;
  AXNodeData root_2;
  root_1.id = 1;
  root_2.id = 1;
  root_1.role = ax::mojom::Role::kRootWebArea;
  root_1.AddChildTreeId(tree_data_2.tree_id);
  root_2.role = ax::mojom::Role::kRootWebArea;

  AXTreeUpdate initial_state_1;
  initial_state_1.root_id = root_1.id;
  initial_state_1.nodes = {root_1};
  initial_state_1.has_tree_data = true;
  initial_state_1.tree_data = tree_data_1;

  AXTreeUpdate initial_state_2;
  initial_state_2.root_id = root_2.id;
  initial_state_2.nodes = {root_2};
  initial_state_2.has_tree_data = true;
  initial_state_2.tree_data = tree_data_2;

  auto tree_1 = std::make_unique<AXTree>(initial_state_1);
  TestAXTreeManager tree_manager_1(std::move(tree_1));
  auto tree_2 = std::make_unique<AXTree>(initial_state_2);
  TestAXTreeManager tree_manager_2(std::move(tree_2));

  const AXNode* root_node_1 = tree_manager_1.GetRootAsAXNode();
  ASSERT_EQ(root_1.id, root_node_1->id());

  const AXNode* root_node_2 = tree_manager_2.GetRootAsAXNode();
  ASSERT_EQ(root_2.id, root_node_2->id());

  EXPECT_EQ(0u, root_node_1->GetChildCount());
  EXPECT_EQ(1u, root_node_1->GetChildCountCrossingTreeBoundary());
  EXPECT_EQ(0u, root_node_1->GetUnignoredChildCount());
  EXPECT_EQ(1u, root_node_1->GetUnignoredChildCountCrossingTreeBoundary());

  EXPECT_EQ(nullptr, root_node_1->GetChildAtIndex(0));
  EXPECT_EQ(root_node_2, root_node_1->GetChildAtIndexCrossingTreeBoundary(0));
  EXPECT_EQ(nullptr, root_node_1->GetUnignoredChildAtIndex(0));
  EXPECT_EQ(root_node_2,
            root_node_1->GetUnignoredChildAtIndexCrossingTreeBoundary(0));

  EXPECT_EQ(nullptr, root_node_2->GetParent());
  EXPECT_EQ(root_node_1, root_node_2->GetParentCrossingTreeBoundary());
  EXPECT_EQ(nullptr, root_node_2->GetUnignoredParent());
  EXPECT_EQ(root_node_1, root_node_2->GetUnignoredParentCrossingTreeBoundary());
}

TEST(AXNodeTest, GetValueForControlTextField) {
  // kRootWebArea
  // ++kTextField (contenteditable)
  // ++++kGenericContainer
  // ++++++kStaticText "Line 1"
  // ++++++kLineBreak '\n'
  // ++++++kStaticText "Line 2"

  AXNodeData root;
  root.id = 1;
  AXNodeData rich_text_field;
  rich_text_field.id = 2;
  AXNodeData rich_text_field_text_container;
  rich_text_field_text_container.id = 3;
  AXNodeData rich_text_field_line_1;
  rich_text_field_line_1.id = 4;
  AXNodeData rich_text_field_line_break;
  rich_text_field_line_break.id = 5;
  AXNodeData rich_text_field_line_2;
  rich_text_field_line_2.id = 6;

  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {rich_text_field.id};

  rich_text_field.role = ax::mojom::Role::kTextField;
  rich_text_field.AddState(ax::mojom::State::kEditable);
  rich_text_field.AddState(ax::mojom::State::kRichlyEditable);
  rich_text_field.AddBoolAttribute(
      ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot, true);
  rich_text_field.SetName("Rich text field");
  rich_text_field.child_ids = {rich_text_field_text_container.id};

  rich_text_field_text_container.role = ax::mojom::Role::kGenericContainer;
  rich_text_field_text_container.AddState(ax::mojom::State::kIgnored);
  rich_text_field_text_container.AddState(ax::mojom::State::kEditable);
  rich_text_field_text_container.AddState(ax::mojom::State::kRichlyEditable);
  rich_text_field_text_container.child_ids = {rich_text_field_line_1.id,
                                              rich_text_field_line_break.id,
                                              rich_text_field_line_2.id};

  rich_text_field_line_1.role = ax::mojom::Role::kStaticText;
  rich_text_field_line_1.AddState(ax::mojom::State::kEditable);
  rich_text_field_line_1.AddState(ax::mojom::State::kRichlyEditable);
  rich_text_field_line_1.SetName("Line 1");

  rich_text_field_line_break.role = ax::mojom::Role::kLineBreak;
  rich_text_field_line_break.AddState(ax::mojom::State::kEditable);
  rich_text_field_line_break.AddState(ax::mojom::State::kRichlyEditable);
  rich_text_field_line_break.SetName("\n");

  rich_text_field_line_2.role = ax::mojom::Role::kStaticText;
  rich_text_field_line_2.AddState(ax::mojom::State::kEditable);
  rich_text_field_line_2.AddState(ax::mojom::State::kRichlyEditable);
  rich_text_field_line_2.SetName("Line 2");

  AXTreeUpdate update;
  update.has_tree_data = true;
  update.tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
  update.root_id = root.id;
  update.nodes = {root,
                  rich_text_field,
                  rich_text_field_text_container,
                  rich_text_field_line_1,
                  rich_text_field_line_break,
                  rich_text_field_line_2};

  auto tree = std::make_unique<AXTree>(update);
  TestAXTreeManager manager(std::move(tree));

  {
    const AXNode* text_field_node =
        manager.GetTree()->GetFromId(rich_text_field.id);
    ASSERT_NE(nullptr, text_field_node);
    EXPECT_EQ("Line 1\nLine 2", text_field_node->GetValueForControl());
  }

  // Only rich text fields should have their value attribute automatically
  // computed from their inner text. Atomic text fields, such as <input> or
  // <textarea> should not.
  rich_text_field.RemoveState(ax::mojom::State::kRichlyEditable);
  rich_text_field.RemoveBoolAttribute(
      ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot);
  AXTreeUpdate update_2;
  update_2.nodes = {rich_text_field};

  ASSERT_TRUE(manager.GetTree()->Unserialize(update_2))
      << manager.GetTree()->error();
  {
    const AXNode* text_field_node =
        manager.GetTree()->GetFromId(rich_text_field.id);
    ASSERT_NE(nullptr, text_field_node);
    EXPECT_EQ("", text_field_node->GetValueForControl());
  }

  rich_text_field.AddState(ax::mojom::State::kRichlyEditable);
  rich_text_field.AddBoolAttribute(
      ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot, true);

  // A node's data should override any computed node data.
  rich_text_field.SetValue("Other value");
  AXTreeUpdate update_3;
  update_3.nodes = {rich_text_field};

  ASSERT_TRUE(manager.GetTree()->Unserialize(update_3))
      << manager.GetTree()->error();
  {
    const AXNode* text_field_node =
        manager.GetTree()->GetFromId(rich_text_field.id);
    ASSERT_NE(nullptr, text_field_node);
    EXPECT_EQ("Other value", text_field_node->GetValueForControl());
  }
}

TEST(AXNodeTest, GetLowestPlatformAncestor) {
  // ++kRootWebArea
  // ++++kButton (IsLeaf=false)
  // ++++++kGenericContainer ignored
  // ++++++++kStaticText "Hello"
  // ++++++++++kInlineTextBox "Hello" (IsLeaf=true)
  // ++++kTextField "World" (IsLeaf=true)
  // ++++++kStaticText "World"
  // ++++++++kInlineTextBox "World" (IsLeaf=true)
  AXNodeData root;
  AXNodeData button;
  AXNodeData generic_container;
  AXNodeData static_text_1;
  AXNodeData inline_box_1;
  AXNodeData text_field;
  AXNodeData static_text_2;
  AXNodeData inline_box_2;

  root.id = 1;
  button.id = 2;
  generic_container.id = 3;
  static_text_1.id = 4;
  inline_box_1.id = 5;
  text_field.id = 6;
  static_text_2.id = 7;
  inline_box_2.id = 8;

  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {button.id, text_field.id};

  button.role = ax::mojom::Role::kButton;
  button.SetValue("Hello");
  button.child_ids = {generic_container.id};

  generic_container.role = ax::mojom::Role::kGenericContainer;
  generic_container.AddState(ax::mojom::State::kIgnored);
  generic_container.child_ids = {static_text_1.id};

  static_text_1.role = ax::mojom::Role::kStaticText;
  static_text_1.SetName("Hello");
  static_text_1.child_ids = {inline_box_1.id};

  inline_box_1.role = ax::mojom::Role::kInlineTextBox;
  inline_box_1.SetName("Hello");

  text_field.role = ax::mojom::Role::kTextField;
  text_field.AddState(ax::mojom::State::kEditable);
  text_field.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "input");
  text_field.SetValue("World");
  text_field.child_ids = {static_text_2.id};

  static_text_2.role = ax::mojom::Role::kStaticText;
  static_text_2.AddState(ax::mojom::State::kEditable);
  static_text_2.SetName("World");
  static_text_2.child_ids = {inline_box_2.id};

  inline_box_2.role = ax::mojom::Role::kInlineTextBox;
  inline_box_2.AddState(ax::mojom::State::kEditable);
  inline_box_2.SetName("World");

  AXTreeUpdate initial_state;
  initial_state.root_id = root.id;
  initial_state.nodes = {root,          button,       generic_container,
                         static_text_1, inline_box_1, text_field,
                         static_text_2, inline_box_2};
  initial_state.has_tree_data = true;

  AXTreeData tree_data;
  tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
  tree_data.title = "Application";
  initial_state.tree_data = tree_data;

  AXTree tree;
  ASSERT_TRUE(tree.Unserialize(initial_state)) << tree.error();

  const AXNode* root_node = tree.root();
  ASSERT_EQ(root.id, root_node->id());
  EXPECT_EQ(root_node, root_node->GetLowestPlatformAncestor());

  const AXNode* button_node = root_node->children()[0];
  ASSERT_EQ(button.id, button_node->id());
  EXPECT_EQ(button_node, button_node->GetLowestPlatformAncestor());

  const AXNode* generic_container_node = button_node->children()[0];
  ASSERT_EQ(generic_container.id, generic_container_node->id());
  EXPECT_EQ(button_node, generic_container_node->GetLowestPlatformAncestor());

  const AXNode* static_text_1_node = generic_container_node->children()[0];
  ASSERT_EQ(static_text_1.id, static_text_1_node->id());
  EXPECT_EQ(static_text_1_node,
            static_text_1_node->GetLowestPlatformAncestor());

  const AXNode* inline_box_1_node = static_text_1_node->children()[0];
  ASSERT_EQ(inline_box_1.id, inline_box_1_node->id());
  EXPECT_EQ(static_text_1_node, inline_box_1_node->GetLowestPlatformAncestor());

  const AXNode* text_field_node = root_node->children()[1];
  ASSERT_EQ(text_field.id, text_field_node->id());
  EXPECT_EQ(text_field_node, text_field_node->GetLowestPlatformAncestor());

  const AXNode* static_text_2_node = text_field_node->children()[0];
  ASSERT_EQ(static_text_2.id, static_text_2_node->id());
  EXPECT_EQ(text_field_node, static_text_2_node->GetLowestPlatformAncestor());

  const AXNode* inline_box_2_node = static_text_2_node->children()[0];
  ASSERT_EQ(inline_box_2.id, inline_box_2_node->id());
  EXPECT_EQ(text_field_node, inline_box_2_node->GetLowestPlatformAncestor());
}

}  // namespace ui
