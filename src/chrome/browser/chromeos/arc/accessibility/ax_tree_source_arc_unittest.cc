// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/ax_tree_source_arc.h"

#include "chrome/browser/chromeos/arc/accessibility/accessibility_node_info_data_wrapper.h"
#include "chrome/browser/chromeos/arc/accessibility/accessibility_window_info_data_wrapper.h"
#include "components/arc/common/accessibility_helper.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/ax_android_constants.h"

namespace arc {

using AXBooleanProperty = mojom::AccessibilityBooleanProperty;
using AXCollectionInfoData = mojom::AccessibilityCollectionInfoData;
using AXCollectionItemInfoData = mojom::AccessibilityCollectionItemInfoData;
using AXEventData = mojom::AccessibilityEventData;
using AXEventType = mojom::AccessibilityEventType;
using AXIntListProperty = mojom::AccessibilityIntListProperty;
using AXIntProperty = mojom::AccessibilityIntProperty;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;
using AXRangeInfoData = mojom::AccessibilityRangeInfoData;
using AXStringListProperty = mojom::AccessibilityStringListProperty;
using AXStringProperty = mojom::AccessibilityStringProperty;
using AXWindowInfoData = mojom::AccessibilityWindowInfoData;
using AXWindowIntListProperty = mojom::AccessibilityWindowIntListProperty;
using AXWindowStringProperty = mojom::AccessibilityWindowStringProperty;

void SetProperty(AXNodeInfoData* node, AXBooleanProperty prop, bool value) {
  if (!node->boolean_properties) {
    node->boolean_properties = base::flat_map<AXBooleanProperty, bool>();
  }
  node->boolean_properties.value().insert(std::make_pair(prop, value));
}

void SetProperty(AXNodeInfoData* node,
                 AXStringProperty prop,
                 const std::string& value) {
  if (!node->string_properties) {
    node->string_properties = base::flat_map<AXStringProperty, std::string>();
  }
  node->string_properties.value().insert(std::make_pair(prop, value));
}

void SetProperty(AXNodeInfoData* node, AXIntProperty prop, int32_t value) {
  if (!node->int_properties) {
    node->int_properties = base::flat_map<AXIntProperty, int>();
  }
  node->int_properties.value().insert(std::make_pair(prop, value));
}

void SetProperty(AXWindowInfoData* window,
                 AXWindowStringProperty prop,
                 const std::string& value) {
  if (!window->string_properties) {
    window->string_properties =
        base::flat_map<AXWindowStringProperty, std::string>();
  }
  window->string_properties.value().insert(std::make_pair(prop, value));
}

void SetProperty(AXNodeInfoData* node,
                 AXIntListProperty prop,
                 const std::vector<int>& value) {
  if (!node->int_list_properties) {
    node->int_list_properties =
        base::flat_map<AXIntListProperty, std::vector<int>>();
  }
  node->int_list_properties.value().insert(std::make_pair(prop, value));
}

void SetProperty(AXWindowInfoData* window,
                 AXWindowIntListProperty prop,
                 const std::vector<int>& value) {
  if (!window->int_list_properties) {
    window->int_list_properties =
        base::flat_map<AXWindowIntListProperty, std::vector<int>>();
  }
  window->int_list_properties.value().insert(std::make_pair(prop, value));
}

class AXTreeSourceArcTest : public testing::Test,
                            public AXTreeSourceArc::Delegate {
 public:
  AXTreeSourceArcTest() : tree_(new AXTreeSourceArc(this)) {}

 protected:
  void CallNotifyAccessibilityEvent(AXEventData* event_data) {
    tree_->NotifyAccessibilityEvent(event_data);
  }

  void CallGetChildren(
      AXNodeInfoData* node,
      std::vector<ArcAccessibilityInfoData*>* out_children) const {
    AccessibilityNodeInfoDataWrapper node_data(tree_.get(), node);
    tree_->GetChildren(&node_data, out_children);
  }

  void CallSerializeNode(AXNodeInfoData* node,
                         std::unique_ptr<ui::AXNodeData>* out_data) const {
    ASSERT_TRUE(out_data);
    AccessibilityNodeInfoDataWrapper node_data(tree_.get(), node);
    *out_data = std::make_unique<ui::AXNodeData>();
    tree_->SerializeNode(&node_data, out_data->get());
  }

  void CallSerializeWindow(AXWindowInfoData* window,
                           std::unique_ptr<ui::AXNodeData>* out_data) const {
    ASSERT_TRUE(out_data);
    AccessibilityWindowInfoDataWrapper window_data(tree_.get(), window);
    *out_data = std::make_unique<ui::AXNodeData>();
    tree_->SerializeNode(&window_data, out_data->get());
  }

  ArcAccessibilityInfoData* CallGetFromId(int32_t id) const {
    return tree_->GetFromId(id);
  }

  bool CallGetTreeData(ui::AXTreeData* data) {
    return tree_->GetTreeData(data);
  }

 private:
  void OnAction(const ui::AXActionData& data) const override {}

  std::unique_ptr<AXTreeSourceArc> tree_;

  DISALLOW_COPY_AND_ASSIGN(AXTreeSourceArcTest);
};

TEST_F(AXTreeSourceArcTest, ReorderChildrenByLayout) {
  auto event = AXEventData::New();
  event->source_id = 0;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;

  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  root_window->window_id = 100;
  root_window->root_node_id = 0;

  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 0;
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({1, 2}));

  // Child button.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* button1 = event->node_data.back().get();
  button1->id = 1;
  SetProperty(button1, AXStringProperty::CLASS_NAME, ui::kAXButtonClassname);
  SetProperty(button1, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(button1, AXBooleanProperty::FOCUSABLE, true);

  // Another child button.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* button2 = event->node_data.back().get();
  button2->id = 2;
  SetProperty(button2, AXStringProperty::CLASS_NAME, ui::kAXButtonClassname);
  SetProperty(button2, AXBooleanProperty::VISIBLE_TO_USER, true);
  SetProperty(button2, AXBooleanProperty::FOCUSABLE, true);

  // Non-overlapping, bottom to top.
  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  button2->bounds_in_screen = gfx::Rect(0, 0, 50, 50);

  // Trigger an update which refreshes the computed bounds used for reordering.
  CallNotifyAccessibilityEvent(event.get());
  std::vector<ArcAccessibilityInfoData*> top_to_bottom;
  CallGetChildren(root, &top_to_bottom);
  ASSERT_EQ(2U, top_to_bottom.size());
  EXPECT_EQ(2, top_to_bottom[0]->GetId());
  EXPECT_EQ(1, top_to_bottom[1]->GetId());

  // Non-overlapping, top to bottom.
  button1->bounds_in_screen = gfx::Rect(0, 0, 50, 50);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  top_to_bottom.clear();
  CallGetChildren(event->node_data[0].get(), &top_to_bottom);
  ASSERT_EQ(2U, top_to_bottom.size());
  EXPECT_EQ(1, top_to_bottom[0]->GetId());
  EXPECT_EQ(2, top_to_bottom[1]->GetId());

  // Overlapping; right to left.
  button1->bounds_in_screen = gfx::Rect(101, 100, 99, 100);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  std::vector<ArcAccessibilityInfoData*> left_to_right;
  CallGetChildren(root, &left_to_right);
  ASSERT_EQ(2U, left_to_right.size());
  EXPECT_EQ(2, left_to_right[0]->GetId());
  EXPECT_EQ(1, left_to_right[1]->GetId());

  // Overlapping; left to right.
  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  button2->bounds_in_screen = gfx::Rect(101, 100, 99, 100);
  CallNotifyAccessibilityEvent(event.get());
  left_to_right.clear();
  CallGetChildren(event->node_data[0].get(), &left_to_right);
  ASSERT_EQ(2U, left_to_right.size());
  EXPECT_EQ(1, left_to_right[0]->GetId());
  EXPECT_EQ(2, left_to_right[1]->GetId());

  // Overlapping, bottom to top.
  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  button2->bounds_in_screen = gfx::Rect(100, 99, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  top_to_bottom.clear();
  CallGetChildren(event->node_data[0].get(), &top_to_bottom);
  ASSERT_EQ(2U, top_to_bottom.size());
  EXPECT_EQ(2, top_to_bottom[0]->GetId());
  EXPECT_EQ(1, top_to_bottom[1]->GetId());

  // Overlapping, top to bottom.
  button1->bounds_in_screen = gfx::Rect(100, 99, 100, 100);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  top_to_bottom.clear();
  CallGetChildren(event->node_data[0].get(), &top_to_bottom);
  ASSERT_EQ(2U, top_to_bottom.size());
  EXPECT_EQ(1, top_to_bottom[0]->GetId());
  EXPECT_EQ(2, top_to_bottom[1]->GetId());

  // Identical. smaller to larger.
  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 10);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  std::vector<ArcAccessibilityInfoData*> dimension;
  CallGetChildren(event->node_data[0].get(), &dimension);
  ASSERT_EQ(2U, dimension.size());
  EXPECT_EQ(2, dimension[0]->GetId());
  EXPECT_EQ(1, dimension[1]->GetId());

  button1->bounds_in_screen = gfx::Rect(100, 100, 10, 100);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  CallNotifyAccessibilityEvent(event.get());
  dimension.clear();
  CallGetChildren(event->node_data[0].get(), &dimension);
  ASSERT_EQ(2U, dimension.size());
  EXPECT_EQ(2, dimension[0]->GetId());
  EXPECT_EQ(1, dimension[1]->GetId());

  // Identical. Larger to smaller.
  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  button2->bounds_in_screen = gfx::Rect(100, 100, 100, 10);
  CallNotifyAccessibilityEvent(event.get());
  dimension.clear();
  CallGetChildren(event->node_data[0].get(), &dimension);
  ASSERT_EQ(2U, dimension.size());
  EXPECT_EQ(1, dimension[0]->GetId());
  EXPECT_EQ(2, dimension[1]->GetId());

  button1->bounds_in_screen = gfx::Rect(100, 100, 100, 100);
  button2->bounds_in_screen = gfx::Rect(100, 100, 10, 100);
  CallNotifyAccessibilityEvent(event.get());
  dimension.clear();
  CallGetChildren(event->node_data[0].get(), &dimension);
  ASSERT_EQ(2U, dimension.size());
  EXPECT_EQ(1, dimension[0]->GetId());
  EXPECT_EQ(2, dimension[1]->GetId());
}

TEST_F(AXTreeSourceArcTest, AccessibleNameComputation) {
  auto event = AXEventData::New();
  event->source_id = 0;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* root = event->node_data.back().get();
  root->id = 0;
  SetProperty(root, AXStringProperty::CLASS_NAME, "");
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({1, 2}));

  // Child.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* child1 = event->node_data.back().get();
  child1->id = 1;

  // Another child.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* child2 = event->node_data.back().get();
  child2->id = 2;

  // Populate the tree source with the data.
  CallNotifyAccessibilityEvent(event.get());

  // No attributes.
  std::unique_ptr<ui::AXNodeData> data;
  CallSerializeNode(root, &data);
  std::string name;
  ASSERT_FALSE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));

  // Text (empty).
  SetProperty(root, AXStringProperty::TEXT, "");

  CallSerializeNode(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("", name);

  // Text (non-empty).
  root->string_properties->clear();
  SetProperty(root, AXStringProperty::TEXT, "label text");

  CallSerializeNode(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("label text", name);

  // Content description (empty), text (non-empty).
  SetProperty(root, AXStringProperty::CONTENT_DESCRIPTION, "");

  CallSerializeNode(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("label text", name);

  // Content description (non-empty), text (non-empty).
  root->string_properties.value()[AXStringProperty::CONTENT_DESCRIPTION] =
      "label content description";

  CallSerializeNode(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("label content description", name);

  // Name from contents.

  // Root node has no name, but has descendants with name.
  root->string_properties->clear();
  // Name from contents only happens if a node is clickable.
  SetProperty(root, AXBooleanProperty::CLICKABLE, true);
  SetProperty(child1, AXStringProperty::TEXT, "child1 label text");
  SetProperty(child2, AXStringProperty::TEXT, "child2 label text");

  CallSerializeNode(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("child1 label text child2 label text", name);

  // If the node has a name, it should override the contents.
  SetProperty(root, AXStringProperty::TEXT, "root label text");

  CallSerializeNode(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("root label text", name);

  // Clearing both clickable and name from root, the name should not be
  // populated.
  root->boolean_properties->clear();
  root->string_properties->clear();
  CallSerializeNode(root, &data);
  ASSERT_FALSE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
}

TEST_F(AXTreeSourceArcTest, AccessibleNameComputationWindow) {
  auto event = AXEventData::New();
  event->source_id = 0;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;
  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root = event->window_data->back().get();
  root->window_id = 0;

  CallNotifyAccessibilityEvent(event.get());

  // Live edit name related attributes.

  // No attributes.
  std::unique_ptr<ui::AXNodeData> data;
  CallSerializeWindow(root, &data);
  std::string name;
  ASSERT_FALSE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));

  // Title attribute
  SetProperty(root, AXWindowStringProperty::TITLE, "window title");
  CallSerializeWindow(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("window title", name);
}

TEST_F(AXTreeSourceArcTest, AccessibleNameComputationWindowWithChildren) {
  auto event = AXEventData::New();
  event->source_id = 3;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;
  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root = event->window_data->back().get();
  root->window_id = 0;
  root->root_node_id = 3;
  SetProperty(root, AXWindowIntListProperty::CHILD_WINDOW_IDS, {2, 5});
  SetProperty(root, AXWindowStringProperty::TITLE, "window title");

  // Add a child window.
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* child = event->window_data->back().get();
  child->window_id = 2;
  child->root_node_id = 4;
  SetProperty(child, AXWindowStringProperty::TITLE, "child window title");

  // Add a child node.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node = event->node_data.back().get();
  node->id = 3;
  SetProperty(node, AXStringProperty::TEXT, "node text");

  // Add a child node to the child window as well.
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* child_node = event->node_data.back().get();
  child_node->id = 4;
  SetProperty(child_node, AXStringProperty::TEXT, "child node text");

  // Add a child window with no children as well.
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* child2 = event->window_data->back().get();
  child2->window_id = 5;
  SetProperty(child2, AXWindowStringProperty::TITLE, "child2 window title");

  CallNotifyAccessibilityEvent(event.get());
  std::unique_ptr<ui::AXNodeData> data;
  std::string name;

  CallSerializeWindow(root, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("window title", name);
  EXPECT_NE(ax::mojom::Role::kRootWebArea, data->role);

  CallSerializeWindow(child, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("child window title", name);
  EXPECT_NE(ax::mojom::Role::kRootWebArea, data->role);

  CallSerializeNode(node, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("node text", name);
  EXPECT_EQ(ax::mojom::Role::kRootWebArea, data->role);

  CallSerializeNode(child_node, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("child node text", name);
  EXPECT_NE(ax::mojom::Role::kRootWebArea, data->role);

  CallSerializeWindow(child2, &data);
  ASSERT_TRUE(
      data->GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("child2 window title", name);
  EXPECT_NE(ax::mojom::Role::kRootWebArea, data->role);
}

// TODO(katie): Maybe remove this test when adding AccessibilityWindowInfoData
// support per go/a11y-arc++-window-mapping if it is no longer needed. This
// depends on if we can assume that each tree has exactly one root.
TEST_F(AXTreeSourceArcTest, MultipleNodeSubtrees) {
  // Run several times to try source_id from root, middle, or leaf of the tree.
  int tree_size = 4;
  for (int i = 0; i < 4; i++) {
    auto event = AXEventData::New();
    event->source_id = i + tree_size;
    event->task_id = 1;
    event->event_type = AXEventType::VIEW_FOCUSED;

    // Make three non-overlapping trees. The middle tree in the list has the
    // source_id of interest. Each tree has a root with one child, and that
    // child has two leaf children.
    int num_trees = 3;
    for (int j = 0; j < num_trees; j++) {
      event->node_data.push_back(AXNodeInfoData::New());
      AXNodeInfoData* root = event->node_data.back().get();
      root->id = j * tree_size;
      SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
                  std::vector<int>({j * tree_size + 1}));

      event->node_data.push_back(AXNodeInfoData::New());
      AXNodeInfoData* child1 = event->node_data.back().get();
      child1->id = j * tree_size + 1;
      SetProperty(child1, AXIntListProperty::CHILD_NODE_IDS,
                  std::vector<int>({j * tree_size + 2, j * tree_size + 3}));

      event->node_data.push_back(AXNodeInfoData::New());
      AXNodeInfoData* child2 = event->node_data.back().get();
      child2->id = j * tree_size + 2;

      event->node_data.push_back(AXNodeInfoData::New());
      AXNodeInfoData* child3 = event->node_data.back().get();
      child3->id = j * tree_size + 3;
    }

    CallNotifyAccessibilityEvent(event.get());

    // Check that only the middle tree was added, and that it is correct.
    std::vector<ArcAccessibilityInfoData*> children;
    CallGetChildren(event->node_data.at(tree_size).get(), &children);
    ASSERT_EQ(1U, children.size());
    EXPECT_EQ(5, children[0]->GetId());
    children.clear();
    CallGetChildren(event->node_data.at(tree_size + 1).get(), &children);
    ASSERT_EQ(2U, children.size());
    EXPECT_EQ(6, children[0]->GetId());
    EXPECT_EQ(7, children[1]->GetId());

    // The first and third roots are not part of the tree.
    EXPECT_EQ(nullptr, CallGetFromId(0));
    EXPECT_EQ(nullptr, CallGetFromId(1));
    EXPECT_EQ(nullptr, CallGetFromId(2));
    EXPECT_EQ(nullptr, CallGetFromId(3));
    EXPECT_EQ(nullptr, CallGetFromId(8));
    EXPECT_EQ(nullptr, CallGetFromId(9));
    EXPECT_EQ(nullptr, CallGetFromId(10));
    EXPECT_EQ(nullptr, CallGetFromId(11));
  }
}

TEST_F(AXTreeSourceArcTest, ComplexTreeStructure) {
  int tree_size = 4;
  int num_trees = 3;

  auto event = AXEventData::New();
  event->source_id = 4;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;
  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root_window = event->window_data->back().get();
  // Pick large numbers for the IDs so as not to overlap.
  root_window->window_id = 1000;
  SetProperty(root_window, AXWindowIntListProperty::CHILD_WINDOW_IDS,
              {100, 200, 300});

  // Make three non-overlapping trees rooted at the same window. One tree has
  // the source_id of interest. Each subtree has a root window, which has a
  // root node with one child, and that child has two leaf children.
  for (int i = 0; i < num_trees; i++) {
    event->window_data->push_back(AXWindowInfoData::New());
    AXWindowInfoData* child_window = event->window_data->back().get();
    child_window->window_id = (i + 1) * 100;
    child_window->root_node_id = i * tree_size + 1;

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* root = event->node_data.back().get();
    root->id = i * tree_size + 1;
    root->window_id = (i + 1) * 100;
    SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
                std::vector<int>({i * tree_size + 2}));

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* child1 = event->node_data.back().get();
    child1->id = i * tree_size + 2;
    SetProperty(child1, AXIntListProperty::CHILD_NODE_IDS,
                std::vector<int>({i * tree_size + 3, i * tree_size + 4}));

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* child2 = event->node_data.back().get();
    child2->id = i * tree_size + 3;

    event->node_data.push_back(AXNodeInfoData::New());
    AXNodeInfoData* child3 = event->node_data.back().get();
    child3->id = i * tree_size + 4;
  }

  CallNotifyAccessibilityEvent(event.get());

  // Check that each node subtree tree was added, and that it is correct.
  std::vector<ArcAccessibilityInfoData*> children;
  for (int i = 0; i < num_trees; i++) {
    CallGetChildren(event->node_data.at(i * tree_size).get(), &children);
    ASSERT_EQ(1U, children.size());
    EXPECT_EQ(i * tree_size + 2, children[0]->GetId());
    children.clear();
    CallGetChildren(event->node_data.at(i * tree_size + 1).get(), &children);
    ASSERT_EQ(2U, children.size());
    EXPECT_EQ(i * tree_size + 3, children[0]->GetId());
    EXPECT_EQ(i * tree_size + 4, children[1]->GetId());
    children.clear();
  }
}

TEST_F(AXTreeSourceArcTest, GetTreeDataAppliesFocus) {
  auto event = AXEventData::New();
  event->source_id = 5;
  event->task_id = 1;
  event->event_type = AXEventType::VIEW_FOCUSED;
  event->window_data = std::vector<mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* root = event->window_data->back().get();
  root->window_id = 5;
  SetProperty(root, AXWindowIntListProperty::CHILD_WINDOW_IDS, {1});

  // Add a child window.
  event->window_data->push_back(AXWindowInfoData::New());
  AXWindowInfoData* child = event->window_data->back().get();
  child->window_id = 1;

  CallNotifyAccessibilityEvent(event.get());
  ui::AXTreeData data;

  // Nothing should be focused when there are no nodes.
  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(-1, data.focus_id);

  // Add a child node.
  root->root_node_id = 2;
  event->node_data.push_back(AXNodeInfoData::New());
  AXNodeInfoData* node = event->node_data.back().get();
  node->id = 2;

  CallNotifyAccessibilityEvent(event.get());

  EXPECT_TRUE(CallGetTreeData(&data));
  EXPECT_EQ(2, data.focus_id);
}

}  // namespace arc
