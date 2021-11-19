// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_fuchsia.h"

#include <fuchsia/accessibility/semantics/cpp/fidl.h>
#include <lib/ui/scenic/cpp/commands.h>

#include <map>
#include <memory>

#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/test_browser_accessibility_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"

namespace content {
namespace {

using AXRole = ax::mojom::Role;
using fuchsia::accessibility::semantics::Role;

constexpr int32_t kRootId = 182;
constexpr int32_t kRowNodeId1 = 2;
constexpr int32_t kRowNodeId2 = 3;
constexpr int32_t kCellNodeId = 7;

ui::AXTreeUpdate CreateTableUpdate() {
  ui::AXTreeUpdate update;
  update.root_id = kRootId;
  update.nodes.resize(8);
  auto& table = update.nodes[0];
  table.id = kRootId;
  table.role = ax::mojom::Role::kTable;
  table.AddIntAttribute(ax::mojom::IntAttribute::kTableRowCount, 2);
  table.AddIntAttribute(ax::mojom::IntAttribute::kTableColumnCount, 2);
  table.child_ids = {888, kRowNodeId2};

  auto& row_group = update.nodes[1];
  row_group.id = 888;
  row_group.role = ax::mojom::Role::kRowGroup;
  row_group.child_ids = {kRowNodeId1};

  auto& row_1 = update.nodes[2];
  row_1.id = kRowNodeId1;
  row_1.role = ax::mojom::Role::kRow;
  row_1.AddIntAttribute(ax::mojom::IntAttribute::kTableRowIndex, 0);
  row_1.child_ids = {4, 5};

  auto& row_2 = update.nodes[3];
  row_2.id = kRowNodeId2;
  row_2.role = ax::mojom::Role::kRow;
  row_2.AddIntAttribute(ax::mojom::IntAttribute::kTableRowIndex, 1);
  row_2.child_ids = {6, kCellNodeId};

  auto& column_header_1 = update.nodes[4];
  column_header_1.id = 4;
  column_header_1.role = ax::mojom::Role::kColumnHeader;
  column_header_1.AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex,
                                  0);
  column_header_1.AddIntAttribute(
      ax::mojom::IntAttribute::kTableCellColumnIndex, 0);

  auto& column_header_2 = update.nodes[5];
  column_header_2.id = 5;
  column_header_2.role = ax::mojom::Role::kColumnHeader;
  column_header_2.AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex,
                                  0);
  column_header_2.AddIntAttribute(
      ax::mojom::IntAttribute::kTableCellColumnIndex, 1);

  auto& cell_1 = update.nodes[6];
  cell_1.id = 6;
  cell_1.role = ax::mojom::Role::kCell;
  cell_1.AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex, 1);
  cell_1.AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnIndex, 0);

  auto& cell_2 = update.nodes[7];
  cell_2.id = kCellNodeId;
  cell_2.role = ax::mojom::Role::kCell;
  cell_2.AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex, 1);
  cell_2.AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnIndex, 1);

  return update;
}

}  // namespace

class BrowserAccessibilityFuchsiaTest : public testing::Test {
 public:
  BrowserAccessibilityFuchsiaTest() = default;
  ~BrowserAccessibilityFuchsiaTest() override = default;

  void SetUp() override;

 protected:
  std::unique_ptr<TestBrowserAccessibilityDelegate>
      test_browser_accessibility_delegate_;

 private:
  content::BrowserTaskEnvironment task_environment_;

  // Disallow copy and assign.
  BrowserAccessibilityFuchsiaTest(const BrowserAccessibilityFuchsiaTest&) =
      delete;
  BrowserAccessibilityFuchsiaTest& operator=(
      const BrowserAccessibilityFuchsiaTest&) = delete;
};

void BrowserAccessibilityFuchsiaTest::SetUp() {
  test_browser_accessibility_delegate_ =
      std::make_unique<TestBrowserAccessibilityDelegate>();
}

TEST_F(BrowserAccessibilityFuchsiaTest, ToFuchsiaNodeDataTranslatesRoles) {
  std::map<ax::mojom::Role, fuchsia::accessibility::semantics::Role>
      role_mapping = {
          {AXRole::kButton, Role::BUTTON},
          {AXRole::kCheckBox, Role::CHECK_BOX},
          {AXRole::kHeader, Role::HEADER},
          {AXRole::kImage, Role::IMAGE},
          {AXRole::kLink, Role::LINK},
          {AXRole::kRadioButton, Role::RADIO_BUTTON},
          {AXRole::kSlider, Role::SLIDER},
          {AXRole::kTextField, Role::TEXT_FIELD},
          {AXRole::kSearchBox, Role::SEARCH_BOX},
          {AXRole::kTextFieldWithComboBox, Role::TEXT_FIELD_WITH_COMBO_BOX},
          {AXRole::kTable, Role::TABLE},
          {AXRole::kGrid, Role::GRID},
          {AXRole::kRow, Role::TABLE_ROW},
          {AXRole::kCell, Role::CELL},
          {AXRole::kColumnHeader, Role::COLUMN_HEADER},
          {AXRole::kRowGroup, Role::ROW_GROUP},
          {AXRole::kParagraph, Role::PARAGRAPH}};

  for (const auto& role_pair : role_mapping) {
    ui::AXNodeData node;
    node.id = 1;
    node.role = role_pair.first;

    std::unique_ptr<BrowserAccessibilityManager> manager(
        BrowserAccessibilityManager::Create(
            MakeAXTreeUpdate(node),
            test_browser_accessibility_delegate_.get()));

    BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
        ToBrowserAccessibilityFuchsia(manager->GetRoot());

    ASSERT_TRUE(browser_accessibility_fuchsia);
    auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

    EXPECT_EQ(fuchsia_node_data.role(), role_pair.second);
  }
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesNodeActions) {
  std::map<ax::mojom::Action, fuchsia::accessibility::semantics::Action>
      action_mapping = {
          {ax::mojom::Action::kDoDefault,
           fuchsia::accessibility::semantics::Action::DEFAULT},
          {ax::mojom::Action::kFocus,
           fuchsia::accessibility::semantics::Action::SET_FOCUS},
          {ax::mojom::Action::kSetValue,
           fuchsia::accessibility::semantics::Action::SET_VALUE},
          {ax::mojom::Action::kScrollToMakeVisible,
           fuchsia::accessibility::semantics::Action::SHOW_ON_SCREEN}};

  for (const auto& action_pair : action_mapping) {
    ui::AXNodeData node;
    node.id = kRootId;
    node.AddAction(action_pair.first);

    std::unique_ptr<BrowserAccessibilityManager> manager(
        BrowserAccessibilityManager::Create(
            MakeAXTreeUpdate(node),
            test_browser_accessibility_delegate_.get()));

    BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
        ToBrowserAccessibilityFuchsia(manager->GetRoot());

    ASSERT_TRUE(browser_accessibility_fuchsia);
    auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

    const auto& actions = fuchsia_node_data.actions();
    ASSERT_EQ(actions.size(), 1u);
    EXPECT_EQ(actions[0], action_pair.second);
  }
}

TEST_F(BrowserAccessibilityFuchsiaTest, ToFuchsiaNodeDataTranslatesLabels) {
  const std::string kLabel = "label";
  const std::string kSecondaryLabel = "secondary label";

  ui::AXNodeData node;
  node.id = kRootId;
  node.AddStringAttribute(ax::mojom::StringAttribute::kName, kLabel);
  node.AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                          kSecondaryLabel);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(node), test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      ToBrowserAccessibilityFuchsia(manager->GetRoot());

  ASSERT_TRUE(browser_accessibility_fuchsia);
  auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

  ASSERT_TRUE(fuchsia_node_data.has_attributes());
  ASSERT_TRUE(fuchsia_node_data.attributes().has_label());
  EXPECT_EQ(fuchsia_node_data.attributes().label(), kLabel);
  EXPECT_EQ(fuchsia_node_data.attributes().secondary_label(), kSecondaryLabel);
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesRangeAttributes) {
  const float kMin = 1.f;
  const float kMax = 2.f;
  const float kStep = 0.1f;
  const float kValue = 1.5f;

  ui::AXNodeData node;
  node.id = kRootId;
  node.role = ax::mojom::Role::kSlider;
  node.AddFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange, kMin);
  node.AddFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange, kMax);
  node.AddFloatAttribute(ax::mojom::FloatAttribute::kStepValueForRange, kStep);
  node.AddFloatAttribute(ax::mojom::FloatAttribute::kValueForRange, kValue);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(node), test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      ToBrowserAccessibilityFuchsia(manager->GetRoot());

  ASSERT_TRUE(browser_accessibility_fuchsia);
  auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

  ASSERT_TRUE(fuchsia_node_data.has_attributes());
  ASSERT_TRUE(fuchsia_node_data.attributes().has_range());
  const auto& range_attributes = fuchsia_node_data.attributes().range();
  EXPECT_EQ(range_attributes.min_value(), kMin);
  EXPECT_EQ(range_attributes.max_value(), kMax);
  EXPECT_EQ(range_attributes.step_delta(), kStep);
  EXPECT_TRUE(fuchsia_node_data.has_states());
  EXPECT_TRUE(fuchsia_node_data.states().has_range_value());
  EXPECT_EQ(fuchsia_node_data.states().range_value(), kValue);
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesTableAttributes) {
  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          CreateTableUpdate(), test_browser_accessibility_delegate_.get()));

  // Verify table node translation.
  {
    BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
        ToBrowserAccessibilityFuchsia(manager->GetRoot());

    ASSERT_TRUE(browser_accessibility_fuchsia);
    auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

    EXPECT_EQ(fuchsia_node_data.role(),
              fuchsia::accessibility::semantics::Role::TABLE);
    ASSERT_TRUE(fuchsia_node_data.has_attributes());
    ASSERT_TRUE(fuchsia_node_data.attributes().has_table_attributes());
    EXPECT_EQ(
        fuchsia_node_data.attributes().table_attributes().number_of_rows(), 2u);
    EXPECT_EQ(
        fuchsia_node_data.attributes().table_attributes().number_of_columns(),
        2u);
  }

  // Verify table row node translation.
  {
    BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
        ToBrowserAccessibilityFuchsia(manager->GetFromID(kRowNodeId2));

    ASSERT_TRUE(browser_accessibility_fuchsia);
    auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

    EXPECT_EQ(fuchsia_node_data.role(),
              fuchsia::accessibility::semantics::Role::TABLE_ROW);
    ASSERT_TRUE(fuchsia_node_data.has_attributes());
    ASSERT_TRUE(fuchsia_node_data.attributes().has_table_row_attributes());
    EXPECT_EQ(fuchsia_node_data.attributes().table_row_attributes().row_index(),
              1u);
  }

  // Verify table cell node translation.
  {
    BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
        ToBrowserAccessibilityFuchsia(manager->GetFromID(kCellNodeId));

    ASSERT_TRUE(browser_accessibility_fuchsia);
    auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

    EXPECT_EQ(fuchsia_node_data.role(),
              fuchsia::accessibility::semantics::Role::CELL);
    ASSERT_TRUE(fuchsia_node_data.has_attributes());
    ASSERT_TRUE(fuchsia_node_data.attributes().has_table_cell_attributes());
    EXPECT_EQ(
        fuchsia_node_data.attributes().table_cell_attributes().row_index(), 1u);
    EXPECT_EQ(
        fuchsia_node_data.attributes().table_cell_attributes().column_index(),
        1u);
  }
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesCheckedState) {
  std::map<ax::mojom::CheckedState,
           fuchsia::accessibility::semantics::CheckedState>
      state_mapping = {
          {ax::mojom::CheckedState::kNone,
           fuchsia::accessibility::semantics::CheckedState::NONE},
          {ax::mojom::CheckedState::kTrue,
           fuchsia::accessibility::semantics::CheckedState::CHECKED},
          {ax::mojom::CheckedState::kFalse,
           fuchsia::accessibility::semantics::CheckedState::UNCHECKED},
          {ax::mojom::CheckedState::kMixed,
           fuchsia::accessibility::semantics::CheckedState::MIXED}};

  for (const auto state_pair : state_mapping) {
    ui::AXNodeData node;
    node.id = kRootId;
    node.AddIntAttribute(ax::mojom::IntAttribute::kCheckedState,
                         static_cast<int32_t>(state_pair.first));

    std::unique_ptr<BrowserAccessibilityManager> manager(
        BrowserAccessibilityManager::Create(
            MakeAXTreeUpdate(node),
            test_browser_accessibility_delegate_.get()));

    BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
        ToBrowserAccessibilityFuchsia(manager->GetRoot());

    ASSERT_TRUE(browser_accessibility_fuchsia);
    auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

    ASSERT_TRUE(fuchsia_node_data.has_states());
    ASSERT_TRUE(fuchsia_node_data.states().has_checked_state());
    EXPECT_EQ(fuchsia_node_data.states().checked_state(), state_pair.second);
  }
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesSelectedState) {
  ui::AXNodeData node;
  node.id = kRootId;
  node.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(node), test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      ToBrowserAccessibilityFuchsia(manager->GetRoot());

  ASSERT_TRUE(browser_accessibility_fuchsia);
  auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

  ASSERT_TRUE(fuchsia_node_data.has_states());
  EXPECT_TRUE(fuchsia_node_data.states().has_selected());
  EXPECT_TRUE(fuchsia_node_data.states().selected());
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesInvisibleState) {
  ui::AXNodeData node;
  node.id = kRootId;
  node.AddState(ax::mojom::State::kInvisible);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(node), test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      ToBrowserAccessibilityFuchsia(manager->GetRoot());

  ASSERT_TRUE(browser_accessibility_fuchsia);
  auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

  ASSERT_TRUE(fuchsia_node_data.has_states());
  ASSERT_TRUE(fuchsia_node_data.states().has_hidden());
  EXPECT_TRUE(fuchsia_node_data.states().hidden());
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesIgnoredState) {
  ui::AXNodeData node;
  node.id = kRootId;
  node.AddState(ax::mojom::State::kIgnored);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(node), test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      ToBrowserAccessibilityFuchsia(manager->GetRoot());

  ASSERT_TRUE(browser_accessibility_fuchsia);
  auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

  ASSERT_TRUE(fuchsia_node_data.has_states());
  ASSERT_TRUE(fuchsia_node_data.states().has_hidden());
  EXPECT_TRUE(fuchsia_node_data.states().hidden());
}

TEST_F(BrowserAccessibilityFuchsiaTest, ToFuchsiaNodeDataTranslatesValue) {
  const auto full_value =
      std::string(fuchsia::accessibility::semantics::MAX_LABEL_SIZE + 1, 'a');
  const auto truncated_value =
      std::string(fuchsia::accessibility::semantics::MAX_LABEL_SIZE, 'a');

  ui::AXNodeData node;
  node.id = kRootId;
  node.AddStringAttribute(ax::mojom::StringAttribute::kValue, full_value);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(node), test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      ToBrowserAccessibilityFuchsia(manager->GetRoot());

  ASSERT_TRUE(browser_accessibility_fuchsia);
  auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

  ASSERT_TRUE(fuchsia_node_data.has_states());
  ASSERT_TRUE(fuchsia_node_data.states().has_value());
  EXPECT_EQ(fuchsia_node_data.states().value(), truncated_value);
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesScrollOffset) {
  const int32_t kScrollX = 1;
  const int32_t kScrollY = 2;

  ui::AXNodeData node;
  node.id = kRootId;
  node.AddIntAttribute(ax::mojom::IntAttribute::kScrollX, kScrollX);
  node.AddIntAttribute(ax::mojom::IntAttribute::kScrollY, kScrollY);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(node), test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      ToBrowserAccessibilityFuchsia(manager->GetRoot());

  ASSERT_TRUE(browser_accessibility_fuchsia);
  auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

  ASSERT_TRUE(fuchsia_node_data.has_states());
  ASSERT_TRUE(fuchsia_node_data.states().has_viewport_offset());
  const auto& viewport_offset = fuchsia_node_data.states().viewport_offset();
  EXPECT_EQ(viewport_offset.x, kScrollX);
  EXPECT_EQ(viewport_offset.y, kScrollY);
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesSpatialInfo) {
  const float x_scale = 0.7f;
  const float y_scale = 0.8f;
  const float z_scale = 0.9f;
  const float x_translation = 10.f;
  const float y_translation = 11.f;
  const float z_translation = 12.f;
  const float x_min = 1.f;
  const float y_min = 2.f;
  const float x_max = 3.f;
  const float y_max = 4.f;

  ui::AXNodeData node;
  node.id = kRootId;
  // gfx::Transform constructor takes arguments in row-major order.
  node.relative_bounds.transform = std::make_unique<gfx::Transform>(
      x_scale, 0, 0, x_translation, 0, y_scale, 0, y_translation, 0, 0, z_scale,
      z_translation, 0, 0, 0, 1);
  node.relative_bounds.bounds = gfx::RectF(
      x_min, y_min, /* width = */ x_max - x_min, /* height = */ y_max - y_min);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(node), test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      ToBrowserAccessibilityFuchsia(manager->GetRoot());

  ASSERT_TRUE(browser_accessibility_fuchsia);
  auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

  ASSERT_TRUE(fuchsia_node_data.has_node_to_container_transform());
  const auto& transform =
      fuchsia_node_data.node_to_container_transform().matrix;
  EXPECT_EQ(transform[0], x_scale);
  EXPECT_EQ(transform[5], y_scale);
  EXPECT_EQ(transform[10], z_scale);
  EXPECT_EQ(transform[12], x_translation);
  EXPECT_EQ(transform[13], y_translation);
  EXPECT_EQ(transform[14], z_translation);
  ASSERT_TRUE(fuchsia_node_data.has_location());
  const auto& location = fuchsia_node_data.location();
  EXPECT_EQ(location.min.x, x_min);
  EXPECT_EQ(location.min.y, y_min);
  EXPECT_EQ(location.max.x, x_max);
  EXPECT_EQ(location.max.y, y_max);
}

}  // namespace content
