// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_event_generator.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"

namespace ui {

// Required by gmock to print TargetedEvent in a human-readable way.
void PrintTo(const AXEventGenerator::TargetedEvent& event, std::ostream* os) {
  *os << event.event_params.event << " on " << event.node->id();
}

namespace {

using testing::Matches;
using testing::PrintToString;
using testing::UnorderedElementsAre;

// TODO(gilmanmh): Improve printing of test failure messages when the expected
// values are themselves matchers (e.g. Not(3)).
MATCHER_P2(HasEventAtNode,
           expected_event_type,
           expected_node_id,
           std::string(negation ? "does not have" : "has") + " " +
               PrintToString(expected_event_type) + " on " +
               PrintToString(expected_node_id)) {
  const auto& event = arg;
  return Matches(expected_event_type)(event.event_params.event) &&
         Matches(expected_node_id)(event.node->id());
}

}  // namespace

TEST(AXEventGeneratorTest, LoadCompleteSameTree) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  initial_state.has_tree_data = true;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate load_complete_update = initial_state;
  load_complete_update.tree_data.loaded = true;

  ASSERT_TRUE(tree.Unserialize(load_complete_update));
  EXPECT_THAT(event_generator, UnorderedElementsAre(HasEventAtNode(
                                   AXEventGenerator::Event::LOAD_COMPLETE, 1)));
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
  load_complete_update.nodes[0].relative_bounds.bounds =
      gfx::RectF(0, 0, 800, 600);
  load_complete_update.has_tree_data = true;
  load_complete_update.tree_data.loaded = true;

  ASSERT_TRUE(tree.Unserialize(load_complete_update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::LOAD_COMPLETE, 2),
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 2)));

  // Load complete should not be emitted for sizeless roots.
  load_complete_update.root_id = 3;
  load_complete_update.nodes.resize(1);
  load_complete_update.nodes[0].id = 3;
  load_complete_update.nodes[0].relative_bounds.bounds = gfx::RectF(0, 0, 0, 0);
  load_complete_update.has_tree_data = true;
  load_complete_update.tree_data.loaded = true;

  ASSERT_TRUE(tree.Unserialize(load_complete_update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 3)));

  // TODO(accessibility): http://crbug.com/888758
  // Load complete should not be emitted for chrome-search URLs.
  load_complete_update.root_id = 4;
  load_complete_update.nodes.resize(1);
  load_complete_update.nodes[0].id = 4;
  load_complete_update.nodes[0].relative_bounds.bounds =
      gfx::RectF(0, 0, 800, 600);
  load_complete_update.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kUrl, "chrome-search://foo");
  load_complete_update.has_tree_data = true;
  load_complete_update.tree_data.loaded = true;

  ASSERT_TRUE(tree.Unserialize(load_complete_update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::LOAD_COMPLETE, 4),
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 4)));
}

TEST(AXEventGeneratorTest, LoadStart) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  initial_state.has_tree_data = true;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate load_start_update;
  load_start_update.root_id = 2;
  load_start_update.nodes.resize(1);
  load_start_update.nodes[0].id = 2;
  load_start_update.nodes[0].relative_bounds.bounds =
      gfx::RectF(0, 0, 800, 600);
  load_start_update.has_tree_data = true;
  load_start_update.tree_data.loaded = false;

  ASSERT_TRUE(tree.Unserialize(load_start_update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::LOAD_START, 2),
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 2)));
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

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED, 1)));
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

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::DOCUMENT_TITLE_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, ExpandedAndRowCount) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(4);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(4);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kTable;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kRow;
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kPopUpButton;
  initial_state.nodes[3].AddState(ax::mojom::State::kExpanded);
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[2].AddState(ax::mojom::State::kExpanded);
  update.nodes[3].state = 0;

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::COLLAPSED, 4),
                  HasEventAtNode(AXEventGenerator::Event::EXPANDED, 3),
                  HasEventAtNode(AXEventGenerator::Event::ROW_COUNT_CHANGED, 2),
                  HasEventAtNode(AXEventGenerator::Event::STATE_CHANGED, 3),
                  HasEventAtNode(AXEventGenerator::Event::STATE_CHANGED, 4)));
}

TEST(AXEventGeneratorTest, SelectedAndSelectedChildren) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(4);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(4);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kMenu;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kMenuItem;
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kListBoxOption;
  initial_state.nodes[3].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                          true);
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[2].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes.pop_back();
  update.nodes.emplace_back();
  update.nodes[3].id = 4;
  update.nodes[3].role = ax::mojom::Role::kListBoxOption;
  update.nodes[3].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, false);

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::SELECTED_CHILDREN_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::SELECTED_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::SELECTED_CHANGED, 4)));
}

TEST(AXEventGeneratorTest, StringValueChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kTextField;
  initial_state.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kValue,
                                            "Before");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].string_attributes.clear();
  update.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kValue,
                                     "After");

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator, UnorderedElementsAre(HasEventAtNode(
                                   AXEventGenerator::Event::VALUE_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, FloatValueChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kSlider;
  initial_state.nodes[0].AddFloatAttribute(
      ax::mojom::FloatAttribute::kValueForRange, 1.0);
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].float_attributes.clear();
  update.nodes[0].AddFloatAttribute(ax::mojom::FloatAttribute::kValueForRange,
                                    2.0);

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator, UnorderedElementsAre(HasEventAtNode(
                                   AXEventGenerator::Event::VALUE_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, InvalidStatusChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kTextField;
  initial_state.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kValue,
                                            "Text");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].SetInvalidState(ax::mojom::InvalidState::kTrue);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::INVALID_STATUS_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, AddLiveRegionAttribute) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                                     "polite");
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::LIVE_STATUS_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::LIVE_REGION_CREATED, 1)));

  event_generator.ClearEvents();
  update.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                                     "assertive");
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::LIVE_STATUS_CHANGED, 1)));

  event_generator.ClearEvents();
  update.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                                     "off");

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::LIVE_STATUS_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, CheckedStateChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kCheckBox;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].SetCheckedState(ax::mojom::CheckedState::kTrue);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::CHECKED_STATE_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, ActiveDescendantChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kListBox;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[0].AddIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId, 2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kListBoxOption;
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kListBoxOption;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].int_attributes.clear();
  update.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                                  3);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::RELATED_NODE_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, CreateAlertAndLiveRegion) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes.resize(4);
  update.nodes[0].child_ids.push_back(2);
  update.nodes[0].child_ids.push_back(3);
  update.nodes[0].child_ids.push_back(4);

  update.nodes[1].id = 2;
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                                     "polite");

  // Blink should automatically add aria-live="assertive" to elements with role
  // kAlert, but we should fire an alert event regardless.
  update.nodes[2].id = 3;
  update.nodes[2].role = ax::mojom::Role::kAlert;

  // Elements with role kAlertDialog will *not* usually have a live region
  // status, but again, we should always fire an alert event.
  update.nodes[3].id = 4;
  update.nodes[3].role = ax::mojom::Role::kAlertDialog;

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::ALERT, 3),
          HasEventAtNode(AXEventGenerator::Event::ALERT, 4),
          HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::LIVE_REGION_CREATED, 2),
          HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 2),
          HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 3),
          HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 4)));
}

TEST(AXEventGeneratorTest, LiveRegionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 1");
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 2");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[1].string_attributes.clear();
  update.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     "After 1");
  update.nodes[2].string_attributes.clear();
  update.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  update.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     "After 2");

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::LIVE_REGION_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::NAME_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::NAME_CHANGED, 3)));
}

TEST(AXEventGeneratorTest, LiveRegionOnlyTextChanges) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 1");
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 2");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                                     "Description 1");
  update.nodes[2].SetCheckedState(ax::mojom::CheckedState::kTrue);

  // Note that we do NOT expect a LIVE_REGION_CHANGED event here, because
  // the name did not change.
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::CHECKED_STATE_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::DESCRIPTION_CHANGED, 2)));
}

TEST(AXEventGeneratorTest, BusyLiveRegionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[0].AddBoolAttribute(ax::mojom::BoolAttribute::kBusy,
                                          true);
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 1");
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 2");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[1].string_attributes.clear();
  update.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     "After 1");
  update.nodes[2].string_attributes.clear();
  update.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  update.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     "After 2");

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::NAME_CHANGED, 2),
                  HasEventAtNode(AXEventGenerator::Event::NAME_CHANGED, 3)));
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

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 1),
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 3)));
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

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::CHILDREN_CHANGED, 1)));
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

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::CHILDREN_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, ScrollHorizontalPositionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kScrollX, 10);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(HasEventAtNode(
          AXEventGenerator::Event::SCROLL_HORIZONTAL_POSITION_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, ScrollVerticalPositionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kScrollY, 10);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(HasEventAtNode(
          AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED, 1)));
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
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kLanguage,
                                     "de");
  update.nodes[2].AddIntAttribute(ax::mojom::IntAttribute::kAriaCellColumnIndex,
                                  7);
  update.nodes[3].AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize,
                                    12.0f);
  update.nodes[4].AddBoolAttribute(ax::mojom::BoolAttribute::kModal, true);
  std::vector<int> ids = {2};
  update.nodes[5].AddIntListAttribute(ax::mojom::IntListAttribute::kControlsIds,
                                      ids);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::CONTROLS_CHANGED, 6),
          HasEventAtNode(AXEventGenerator::Event::LANGUAGE_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::OTHER_ATTRIBUTE_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::OTHER_ATTRIBUTE_CHANGED, 4),
          HasEventAtNode(AXEventGenerator::Event::OTHER_ATTRIBUTE_CHANGED, 5),
          HasEventAtNode(AXEventGenerator::Event::RELATED_NODE_CHANGED, 6)));
}

TEST(AXEventGeneratorTest, NameChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     "Hello");
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator, UnorderedElementsAre(HasEventAtNode(
                                   AXEventGenerator::Event::NAME_CHANGED, 2)));
}

TEST(AXEventGeneratorTest, DescriptionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                                     "Hello");
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::DESCRIPTION_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, RoleChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].role = ax::mojom::Role::kCheckBox;
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator, UnorderedElementsAre(HasEventAtNode(
                                   AXEventGenerator::Event::ROLE_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, MenuItemSelected) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kMenu;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[0].AddIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId, 2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kMenuListOption;
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kMenuListOption;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].int_attributes.clear();
  update.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                                  3);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::MENU_ITEM_SELECTED, 3),
          HasEventAtNode(AXEventGenerator::Event::RELATED_NODE_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, NodeBecomesIgnored) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kArticle;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGroup;
  initial_state.nodes[2].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[2].child_ids.push_back(4);
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kGroup;
  initial_state.nodes[3].child_ids.push_back(5);
  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kStaticText;

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[3].AddState(ax::mojom::State::kIgnored);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 2),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 4)));
}

TEST(AXEventGeneratorTest, NodeBecomesIgnored2) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kArticle;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGroup;
  initial_state.nodes[2].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[2].child_ids.push_back(4);
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kGroup;
  initial_state.nodes[3].child_ids.push_back(5);
  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kStaticText;

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  // Marking as ignored should fire CHILDREN_CHANGED on 2
  update.nodes[3].AddState(ax::mojom::State::kIgnored);
  // Remove node id 5 so it also fires CHILDREN_CHANGED on 4.
  update.nodes.pop_back();
  update.nodes[3].child_ids.clear();
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 2),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 4)));
}

TEST(AXEventGeneratorTest, NodeBecomesUnignored) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kArticle;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGroup;
  initial_state.nodes[2].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[2].child_ids.push_back(4);
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kGroup;
  initial_state.nodes[3].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[3].child_ids.push_back(5);
  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kStaticText;

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[3].state = 0;
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 2),
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 4),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 4)));
}

TEST(AXEventGeneratorTest, NodeBecomesUnignored2) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kArticle;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGroup;
  initial_state.nodes[2].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[2].child_ids.push_back(4);
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kGroup;
  initial_state.nodes[3].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[3].child_ids.push_back(5);
  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kStaticText;

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  // Marking as no longer ignored should fire CHILDREN_CHANGED on 2
  update.nodes[3].state = 0;
  // Remove node id 5 so it also fires CHILDREN_CHANGED on 4.
  update.nodes.pop_back();
  update.nodes[3].child_ids.clear();
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 2),
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 4),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 4)));
}

TEST(AXEventGeneratorTest, SubtreeBecomesUnignored) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kArticle;
  initial_state.nodes[1].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGroup;
  initial_state.nodes[2].AddState(ax::mojom::State::kIgnored);

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[1].RemoveState(ax::mojom::State::kIgnored);
  update.nodes[2].RemoveState(ax::mojom::State::kIgnored);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 1),
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 2),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 2),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 3)));
}

TEST(AXEventGeneratorTest, TwoNodesSwapIgnored) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kArticle;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGroup;
  initial_state.nodes[2].AddState(ax::mojom::State::kIgnored);

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[1].AddState(ax::mojom::State::kIgnored);
  update.nodes[2].RemoveState(ax::mojom::State::kIgnored);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 1),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 2),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 3),
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 3)));
}

TEST(AXEventGeneratorTest, TwoNodesSwapIgnored2) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kArticle;
  initial_state.nodes[1].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGroup;

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[1].RemoveState(ax::mojom::State::kIgnored);
  update.nodes[2].AddState(ax::mojom::State::kIgnored);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 1),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 2),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 3),
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 2)));
}

TEST(AXEventGeneratorTest, ActiveDescendantChangeOnDescendant) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGroup;
  initial_state.nodes[2].AddIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId, 4);
  initial_state.nodes[2].child_ids.push_back(4);
  initial_state.nodes[2].child_ids.push_back(5);
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kGroup;
  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kGroup;

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  initial_state.nodes[2].RemoveIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId);
  initial_state.nodes[2].AddIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId, 5);
  AXTreeUpdate update = initial_state;
  update.node_id_to_clear = 2;
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::RELATED_NODE_CHANGED, 3)));
}

TEST(AXEventGeneratorTest, ImageAnnotationChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kImageAnnotation, "Hello");
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::IMAGE_ANNOTATION_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, ImageAnnotationStatusChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded);

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::IMAGE_ANNOTATION_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, StringPropertyChanges) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  struct {
    ax::mojom::StringAttribute id;
    std::string old_value;
    std::string new_value;
  } attributes[] = {
      {ax::mojom::StringAttribute::kAccessKey, "a", "b"},
      {ax::mojom::StringAttribute::kClassName, "a", "b"},
      {ax::mojom::StringAttribute::kKeyShortcuts, "a", "b"},
      {ax::mojom::StringAttribute::kLanguage, "a", "b"},
      {ax::mojom::StringAttribute::kPlaceholder, "a", "b"},
  };
  for (auto&& attrib : attributes) {
    initial_state.nodes.push_back({});
    initial_state.nodes.back().id = initial_state.nodes.size();
    initial_state.nodes.back().AddStringAttribute(attrib.id, attrib.old_value);
    initial_state.nodes[0].child_ids.push_back(initial_state.nodes.size());
  }

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  int index = 1;
  for (auto&& attrib : attributes) {
    initial_state.nodes[index++].AddStringAttribute(attrib.id,
                                                    attrib.new_value);
  }

  AXTreeUpdate update = initial_state;
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::ACCESS_KEY_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::CLASS_NAME_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::KEY_SHORTCUTS_CHANGED, 4),
          HasEventAtNode(AXEventGenerator::Event::LANGUAGE_CHANGED, 5),
          HasEventAtNode(AXEventGenerator::Event::PLACEHOLDER_CHANGED, 6)));
}

TEST(AXEventGeneratorTest, IntPropertyChanges) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  struct {
    ax::mojom::IntAttribute id;
    int old_value;
    int new_value;
  } attributes[] = {
      {ax::mojom::IntAttribute::kHierarchicalLevel, 1, 2},
      {ax::mojom::IntAttribute::kPosInSet, 1, 2},
      {ax::mojom::IntAttribute::kSetSize, 1, 2},
  };
  for (auto&& attrib : attributes) {
    initial_state.nodes.push_back({});
    initial_state.nodes.back().id = initial_state.nodes.size();
    initial_state.nodes.back().AddIntAttribute(attrib.id, attrib.old_value);
    initial_state.nodes[0].child_ids.push_back(initial_state.nodes.size());
  }

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  int index = 1;
  for (auto&& attrib : attributes)
    initial_state.nodes[index++].AddIntAttribute(attrib.id, attrib.new_value);

  AXTreeUpdate update = initial_state;
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::HIERARCHICAL_LEVEL_CHANGED,
                         2),
          HasEventAtNode(AXEventGenerator::Event::POSITION_IN_SET_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::SET_SIZE_CHANGED, 4)));
}

TEST(AXEventGeneratorTest, IntListPropertyChanges) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  struct {
    ax::mojom::IntListAttribute id;
    std::vector<int> old_value;
    std::vector<int> new_value;
  } attributes[] = {
      {ax::mojom::IntListAttribute::kDescribedbyIds, {1}, {2}},
      {ax::mojom::IntListAttribute::kFlowtoIds, {1}, {2}},
      {ax::mojom::IntListAttribute::kLabelledbyIds, {1}, {2}},
  };
  for (auto&& attrib : attributes) {
    initial_state.nodes.push_back({});
    initial_state.nodes.back().id = initial_state.nodes.size();
    initial_state.nodes.back().AddIntListAttribute(attrib.id, attrib.old_value);
    initial_state.nodes[0].child_ids.push_back(initial_state.nodes.size());
  }

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  int index = 1;
  for (auto&& attrib : attributes) {
    initial_state.nodes[index++].AddIntListAttribute(attrib.id,
                                                     attrib.new_value);
  }

  AXTreeUpdate update = initial_state;
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::DESCRIBED_BY_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::FLOW_FROM_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::FLOW_FROM_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::FLOW_TO_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::LABELED_BY_CHANGED, 4),
          HasEventAtNode(AXEventGenerator::Event::RELATED_NODE_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::RELATED_NODE_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::RELATED_NODE_CHANGED, 4)));
}

TEST(AXEventGeneratorTest, AriaBusyChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);
  initial_state.nodes[0].AddBoolAttribute(ax::mojom::BoolAttribute::kBusy,
                                          true);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddBoolAttribute(ax::mojom::BoolAttribute::kBusy, false);

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::BUSY_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::LAYOUT_INVALIDATED, 1)));
}

TEST(AXEventGeneratorTest, MultiselectableStateChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kGrid;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;

  update.nodes[0].AddState(ax::mojom::State::kMultiselectable);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::MULTISELECTABLE_STATE_CHANGED,
                         1),
          HasEventAtNode(AXEventGenerator::Event::STATE_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, RequiredStateChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kTextField;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;

  update.nodes[0].AddState(ax::mojom::State::kRequired);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::REQUIRED_STATE_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::STATE_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, FlowToChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(6);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[0].child_ids.assign({2, 3, 4, 5, 6});
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[1].AddIntListAttribute(
      ax::mojom::IntListAttribute::kFlowtoIds, {3, 4});
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[5].id = 6;
  initial_state.nodes[5].role = ax::mojom::Role::kGenericContainer;

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[1].AddIntListAttribute(ax::mojom::IntListAttribute::kFlowtoIds,
                                      {4, 5, 6});

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::FLOW_FROM_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::FLOW_FROM_CHANGED, 5),
          HasEventAtNode(AXEventGenerator::Event::FLOW_FROM_CHANGED, 6),
          HasEventAtNode(AXEventGenerator::Event::FLOW_TO_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::RELATED_NODE_CHANGED, 2)));
}

TEST(AXEventGeneratorTest, ControlsChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;

  std::vector<int> ids = {2};
  update.nodes[0].AddIntListAttribute(ax::mojom::IntListAttribute::kControlsIds,
                                      ids);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::CONTROLS_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::RELATED_NODE_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, AtomicChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;

  update.nodes[0].AddBoolAttribute(ax::mojom::BoolAttribute::kLiveAtomic, true);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::ATOMIC_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, DropeffectChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;

  update.nodes[0].AddDropeffect(ax::mojom::Dropeffect::kCopy);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::DROPEFFECT_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, GrabbedChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;

  update.nodes[0].AddBoolAttribute(ax::mojom::BoolAttribute::kGrabbed, true);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::GRABBED_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, HasPopupChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;

  update.nodes[0].SetHasPopup(ax::mojom::HasPopup::kTrue);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::HASPOPUP_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, LiveRelevantChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;

  update.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kLiveRelevant,
                                     "all");
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::LIVE_RELEVANT_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, MultilineStateChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;

  update.nodes[0].AddState(ax::mojom::State::kMultiline);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::MULTILINE_STATE_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::STATE_CHANGED, 1)));
}

}  // namespace ui
