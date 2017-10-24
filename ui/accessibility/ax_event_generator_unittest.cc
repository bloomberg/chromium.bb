// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_event_generator.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"

namespace ui {

namespace {

std::string DumpEvents(AXEventGenerator* generator) {
  std::vector<std::string> event_strs;
  for (auto targeted_event : *generator) {
    const char* event_name;
    switch (targeted_event.event) {
      case AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED:
        event_name = "ACTIVE_DESCENDANT_CHANGED";
        break;
      case AXEventGenerator::Event::ALERT:
        event_name = "ALERT";
        break;
      case AXEventGenerator::Event::CHECKED_STATE_CHANGED:
        event_name = "CHECKED_STATE_CHANGED";
        break;
      case AXEventGenerator::Event::CHILDREN_CHANGED:
        event_name = "CHILDREN_CHANGED";
        break;
      case AXEventGenerator::Event::COLLAPSED:
        event_name = "COLLAPSED";
        break;
      case AXEventGenerator::Event::DESCRIPTION_CHANGED:
        event_name = "DESCRIPTION_CHANGED";
        break;
      case AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED:
        event_name = "DOCUMENT_SELECTION_CHANGED";
        break;
      case AXEventGenerator::Event::DOCUMENT_TITLE_CHANGED:
        event_name = "DOCUMENT_TITLE_CHANGED";
        break;
      case AXEventGenerator::Event::EXPANDED:
        event_name = "EXPANDED";
        break;
      case AXEventGenerator::Event::INVALID_STATUS_CHANGED:
        event_name = "INVALID_STATUS_CHANGED";
        break;
      case AXEventGenerator::Event::LIVE_REGION_CHANGED:
        event_name = "LIVE_REGION_CHANGED";
        break;
      case AXEventGenerator::Event::LIVE_REGION_CREATED:
        event_name = "LIVE_REGION_CREATED";
        break;
      case AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED:
        event_name = "LIVE_REGION_NODE_CHANGED";
        break;
      case AXEventGenerator::Event::LOAD_COMPLETE:
        event_name = "LOAD_COMPLETE";
        break;
      case AXEventGenerator::Event::MENU_ITEM_SELECTED:
        event_name = "MENU_ITEM_SELECTED";
        break;
      case AXEventGenerator::Event::NAME_CHANGED:
        event_name = "NAME_CHANGED";
        break;
      case AXEventGenerator::Event::OTHER_ATTRIBUTE_CHANGED:
        event_name = "OTHER_ATTRIBUTE_CHANGED";
        break;
      case AXEventGenerator::Event::ROLE_CHANGED:
        event_name = "ROLE_CHANGED";
        break;
      case AXEventGenerator::Event::ROW_COUNT_CHANGED:
        event_name = "ROW_COUNT_CHANGED";
        break;
      case AXEventGenerator::Event::SCROLL_POSITION_CHANGED:
        event_name = "SCROLL_POSITION_CHANGED";
        break;
      case AXEventGenerator::Event::SELECTED_CHANGED:
        event_name = "SELECTED_CHANGED";
        break;
      case AXEventGenerator::Event::SELECTED_CHILDREN_CHANGED:
        event_name = "SELECTED_CHILDREN_CHANGED";
        break;
      case AXEventGenerator::Event::STATE_CHANGED:
        event_name = "STATE_CHANGED";
        break;
      case AXEventGenerator::Event::VALUE_CHANGED:
        event_name = "VALUE_CHANGED";
        break;
      default:
        NOTREACHED();
        event_name = "UNKNOWN";
        break;
    }
    event_strs.push_back(
        base::StringPrintf("%s on %d", event_name, targeted_event.node->id()));
  }

  // The order of events is arbitrary, so just sort the strings
  // alphabetically to make the test output predictable.
  std::sort(event_strs.begin(), event_strs.end());

  return base::JoinString(event_strs, ", ");
}

}  // namespace

TEST(AXEventGeneratorTest, LoadCompleteSameTree) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.has_tree_data = true;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate load_complete_update = initial_state;
  load_complete_update.tree_data.loaded = true;
  EXPECT_TRUE(tree.Unserialize(load_complete_update));
  EXPECT_EQ("LOAD_COMPLETE on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, LoadCompleteNewTree) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.has_tree_data = true;
  initial_state.tree_data.loaded = true;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate load_complete_update;
  load_complete_update.root_id = 2;
  load_complete_update.nodes.resize(1);
  load_complete_update.nodes[0].id = 2;
  load_complete_update.has_tree_data = true;
  load_complete_update.tree_data.loaded = true;
  EXPECT_TRUE(tree.Unserialize(load_complete_update));
  EXPECT_EQ("LOAD_COMPLETE on 2", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, DocumentSelectionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.has_tree_data = true;
  initial_state.tree_data.sel_focus_object_id = 1;
  initial_state.tree_data.sel_focus_offset = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.tree_data.sel_focus_offset = 2;
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("DOCUMENT_SELECTION_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, DocumentTitleChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.has_tree_data = true;
  initial_state.tree_data.title = "Before";
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.tree_data.title = "After";
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("DOCUMENT_TITLE_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, ExpandedAndRowCount) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(4);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ui::AX_ROLE_ROOT_WEB_AREA;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(4);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ui::AX_ROLE_TABLE;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ui::AX_ROLE_ROW;
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ui::AX_ROLE_POP_UP_BUTTON;
  initial_state.nodes[3].AddState(ui::AX_STATE_EXPANDED);
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[2].AddState(ui::AX_STATE_EXPANDED);
  update.nodes[3].state = 0;
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "COLLAPSED on 4, "
      "EXPANDED on 3, "
      "ROW_COUNT_CHANGED on 2, "
      "STATE_CHANGED on 3, "
      "STATE_CHANGED on 4",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, SelectedAndSelectedChildren) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(4);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ui::AX_ROLE_ROOT_WEB_AREA;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(4);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ui::AX_ROLE_MENU;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ui::AX_ROLE_MENU_ITEM;
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ui::AX_ROLE_LIST_BOX_OPTION;
  initial_state.nodes[3].AddState(ui::AX_STATE_SELECTED);
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[2].AddState(ui::AX_STATE_SELECTED);
  update.nodes[3].state = 0;
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "SELECTED_CHANGED on 3, "
      "SELECTED_CHANGED on 4, "
      "SELECTED_CHILDREN_CHANGED on 2, "
      "STATE_CHANGED on 3, "
      "STATE_CHANGED on 4",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, StringValueChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ui::AX_ROLE_TEXT_FIELD;
  initial_state.nodes[0].AddStringAttribute(ui::AX_ATTR_VALUE, "Before");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].string_attributes.clear();
  update.nodes[0].AddStringAttribute(ui::AX_ATTR_VALUE, "After");
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("VALUE_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, FloatValueChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ui::AX_ROLE_SLIDER;
  initial_state.nodes[0].AddFloatAttribute(ui::AX_ATTR_VALUE_FOR_RANGE, 1.0);
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].float_attributes.clear();
  update.nodes[0].AddFloatAttribute(ui::AX_ATTR_VALUE_FOR_RANGE, 2.0);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("VALUE_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, InvalidStatusChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ui::AX_ROLE_TEXT_FIELD;
  initial_state.nodes[0].AddStringAttribute(ui::AX_ATTR_VALUE, "Text");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddIntAttribute(ui::AX_ATTR_INVALID_STATE,
                                  ui::AX_INVALID_STATE_SPELLING);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("INVALID_STATUS_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, AddLiveRegionAttribute) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddStringAttribute(ui::AX_ATTR_LIVE_STATUS, "polite");
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("LIVE_REGION_CREATED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, CheckedStateChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ui::AX_ROLE_CHECK_BOX;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddIntAttribute(ui::AX_ATTR_CHECKED_STATE,
                                  ui::AX_CHECKED_STATE_TRUE);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("CHECKED_STATE_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, ActiveDescendantChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ui::AX_ROLE_LIST_BOX;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[0].AddIntAttribute(ui::AX_ATTR_ACTIVEDESCENDANT_ID, 2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ui::AX_ROLE_LIST_BOX_OPTION;
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ui::AX_ROLE_LIST_BOX_OPTION;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].int_attributes.clear();
  update.nodes[0].AddIntAttribute(ui::AX_ATTR_ACTIVEDESCENDANT_ID, 3);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("ACTIVE_DESCENDANT_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, CreateAlertAndLiveRegion) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes.resize(3);
  update.nodes[0].child_ids.push_back(2);
  update.nodes[0].child_ids.push_back(3);
  update.nodes[1].id = 2;
  update.nodes[1].AddStringAttribute(ui::AX_ATTR_LIVE_STATUS, "polite");
  update.nodes[2].id = 3;
  update.nodes[2].role = ui::AX_ROLE_ALERT;
  update.nodes[2].AddStringAttribute(ui::AX_ATTR_LIVE_STATUS, "polite");

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "ALERT on 3, "
      "CHILDREN_CHANGED on 1, "
      "LIVE_REGION_CREATED on 2",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, LiveRegionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddStringAttribute(ui::AX_ATTR_LIVE_STATUS, "polite");
  initial_state.nodes[0].AddStringAttribute(ui::AX_ATTR_CONTAINER_LIVE_STATUS,
                                            "polite");
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ui::AX_ROLE_STATIC_TEXT;
  initial_state.nodes[1].AddStringAttribute(ui::AX_ATTR_CONTAINER_LIVE_STATUS,
                                            "polite");
  initial_state.nodes[1].AddStringAttribute(ui::AX_ATTR_NAME, "Before 1");
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ui::AX_ROLE_STATIC_TEXT;
  initial_state.nodes[2].AddStringAttribute(ui::AX_ATTR_CONTAINER_LIVE_STATUS,
                                            "polite");
  initial_state.nodes[2].AddStringAttribute(ui::AX_ATTR_NAME, "Before 2");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[1].string_attributes.clear();
  update.nodes[1].AddStringAttribute(ui::AX_ATTR_CONTAINER_LIVE_STATUS,
                                     "polite");
  update.nodes[1].AddStringAttribute(ui::AX_ATTR_NAME, "After 1");
  update.nodes[2].string_attributes.clear();
  update.nodes[2].AddStringAttribute(ui::AX_ATTR_CONTAINER_LIVE_STATUS,
                                     "polite");
  update.nodes[2].AddStringAttribute(ui::AX_ATTR_NAME, "After 2");

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "LIVE_REGION_CHANGED on 1, "
      "LIVE_REGION_NODE_CHANGED on 2, "
      "LIVE_REGION_NODE_CHANGED on 3, "
      "NAME_CHANGED on 2, "
      "NAME_CHANGED on 3",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, BusyLiveRegionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddStringAttribute(ui::AX_ATTR_LIVE_STATUS, "polite");
  initial_state.nodes[0].AddStringAttribute(ui::AX_ATTR_CONTAINER_LIVE_STATUS,
                                            "polite");
  initial_state.nodes[0].AddBoolAttribute(ui::AX_ATTR_BUSY, true);
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ui::AX_ROLE_STATIC_TEXT;
  initial_state.nodes[1].AddStringAttribute(ui::AX_ATTR_CONTAINER_LIVE_STATUS,
                                            "polite");
  initial_state.nodes[1].AddStringAttribute(ui::AX_ATTR_NAME, "Before 1");
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ui::AX_ROLE_STATIC_TEXT;
  initial_state.nodes[2].AddStringAttribute(ui::AX_ATTR_CONTAINER_LIVE_STATUS,
                                            "polite");
  initial_state.nodes[2].AddStringAttribute(ui::AX_ATTR_NAME, "Before 2");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[1].string_attributes.clear();
  update.nodes[1].AddStringAttribute(ui::AX_ATTR_CONTAINER_LIVE_STATUS,
                                     "polite");
  update.nodes[1].AddStringAttribute(ui::AX_ATTR_NAME, "After 1");
  update.nodes[2].string_attributes.clear();
  update.nodes[2].AddStringAttribute(ui::AX_ATTR_CONTAINER_LIVE_STATUS,
                                     "polite");
  update.nodes[2].AddStringAttribute(ui::AX_ATTR_NAME, "After 2");

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "NAME_CHANGED on 2, "
      "NAME_CHANGED on 3",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, AddChild) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes.resize(3);
  update.nodes[0].child_ids.push_back(3);
  update.nodes[2].id = 3;

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("CHILDREN_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, RemoveChild) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[2].id = 3;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes.resize(2);
  update.nodes[0].child_ids.clear();
  update.nodes[0].child_ids.push_back(2);

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("CHILDREN_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, ReorderChildren) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[2].id = 3;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].child_ids.clear();
  update.nodes[0].child_ids.push_back(3);
  update.nodes[0].child_ids.push_back(2);

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("CHILDREN_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, ScrollPositionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddIntAttribute(ui::AX_ATTR_SCROLL_Y, 10);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("SCROLL_POSITION_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, OtherAttributeChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(6);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[0].child_ids.push_back(4);
  initial_state.nodes[0].child_ids.push_back(5);
  initial_state.nodes[0].child_ids.push_back(6);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[2].id = 3;
  initial_state.nodes[3].id = 4;
  initial_state.nodes[4].id = 5;
  initial_state.nodes[5].id = 6;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[1].AddStringAttribute(ui::AX_ATTR_LANGUAGE, "de");
  update.nodes[2].AddIntAttribute(ui::AX_ATTR_ARIA_CELL_COLUMN_INDEX, 7);
  update.nodes[3].AddFloatAttribute(ui::AX_ATTR_FONT_SIZE, 12.0f);
  update.nodes[4].AddBoolAttribute(ui::AX_ATTR_MODAL, true);
  std::vector<int> ids = {2};
  update.nodes[5].AddIntListAttribute(ui::AX_ATTR_CONTROLS_IDS, ids);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "OTHER_ATTRIBUTE_CHANGED on 2, "
      "OTHER_ATTRIBUTE_CHANGED on 3, "
      "OTHER_ATTRIBUTE_CHANGED on 4, "
      "OTHER_ATTRIBUTE_CHANGED on 5, "
      "OTHER_ATTRIBUTE_CHANGED on 6",
      DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, NameChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddStringAttribute(ui::AX_ATTR_NAME, "Hello");
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("NAME_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, DescriptionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddStringAttribute(ui::AX_ATTR_DESCRIPTION, "Hello");
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("DESCRIPTION_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, RoleChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].role = ui::AX_ROLE_CHECK_BOX;
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ("ROLE_CHANGED on 1", DumpEvents(&event_generator));
}

TEST(AXEventGeneratorTest, MenuItemSelected) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ui::AX_ROLE_MENU;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[0].AddIntAttribute(ui::AX_ATTR_ACTIVEDESCENDANT_ID, 2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ui::AX_ROLE_MENU_LIST_OPTION;
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ui::AX_ROLE_MENU_LIST_OPTION;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].int_attributes.clear();
  update.nodes[0].AddIntAttribute(ui::AX_ATTR_ACTIVEDESCENDANT_ID, 3);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_EQ(
      "ACTIVE_DESCENDANT_CHANGED on 1, "
      "MENU_ITEM_SELECTED on 3",
      DumpEvents(&event_generator));
}

}  // namespace ui
