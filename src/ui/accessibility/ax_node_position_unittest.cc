// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ui {

using TestPositionType = std::unique_ptr<AXPosition<AXNodePosition, AXNode>>;

namespace {

int32_t ROOT_ID = 1;
int32_t BUTTON_ID = 2;
int32_t CHECK_BOX_ID = 3;
int32_t TEXT_FIELD_ID = 4;
int32_t STATIC_TEXT1_ID = 5;
int32_t INLINE_BOX1_ID = 6;
int32_t LINE_BREAK_ID = 7;
int32_t STATIC_TEXT2_ID = 8;
int32_t INLINE_BOX2_ID = 9;

class AXPositionTest : public testing::Test {
 public:
  static const char* TEXT_VALUE;

  AXPositionTest() = default;
  ~AXPositionTest() override = default;

 protected:
  void SetUp() override;
  void TearDown() override;

  AXNodeData root_;
  AXNodeData button_;
  AXNodeData check_box_;
  AXNodeData text_field_;
  AXNodeData static_text1_;
  AXNodeData line_break_;
  AXNodeData static_text2_;
  AXNodeData inline_box1_;
  AXNodeData inline_box2_;

  AXTree tree_;

  DISALLOW_COPY_AND_ASSIGN(AXPositionTest);
};

// Used by parameterized tests.
// The test starts from a pre-determined position and repeats until there are no
// more expectations.
// TODO(nektar): Only text positions are tested for now.
struct TestParam {
  TestParam() = default;

  // Required by GTest framework.
  TestParam(const TestParam& other) = default;
  TestParam& operator=(const TestParam& other) = default;

  ~TestParam() = default;

  // Stores the method that should be called repeatedly by the test to create
  // the next position.
  base::RepeatingCallback<TestPositionType(const TestPositionType&)> TestMethod;

  // The node at which the test should start.
  int32_t start_node_id_;

  // The text offset at which the test should start.
  int start_offset_;

  // A list of positions that should be returned from the method being tested,
  // in stringified form.
  std::vector<std::string> expectations;
};

class AXPositionTestWithParam : public AXPositionTest,
                                public testing::WithParamInterface<TestParam> {
 public:
  AXPositionTestWithParam() = default;
  ~AXPositionTestWithParam() override = default;

  DISALLOW_COPY_AND_ASSIGN(AXPositionTestWithParam);
};

const char* AXPositionTest::TEXT_VALUE = "Line 1\nLine 2";

void AXPositionTest::SetUp() {
  root_.id = ROOT_ID;
  button_.id = BUTTON_ID;
  check_box_.id = CHECK_BOX_ID;
  text_field_.id = TEXT_FIELD_ID;
  static_text1_.id = STATIC_TEXT1_ID;
  inline_box1_.id = INLINE_BOX1_ID;
  line_break_.id = LINE_BREAK_ID;
  static_text2_.id = STATIC_TEXT2_ID;
  inline_box2_.id = INLINE_BOX2_ID;

  root_.role = ax::mojom::Role::kRootWebArea;

  button_.role = ax::mojom::Role::kButton;
  button_.SetHasPopup(ax::mojom::HasPopup::kMenu);
  button_.SetName("Button");
  button_.relative_bounds.bounds = gfx::RectF(20, 20, 200, 30);
  button_.AddIntAttribute(ax::mojom::IntAttribute::kNextOnLineId,
                          check_box_.id);
  root_.child_ids.push_back(button_.id);

  check_box_.role = ax::mojom::Role::kCheckBox;
  check_box_.SetCheckedState(ax::mojom::CheckedState::kTrue);
  check_box_.SetName("Check box");
  check_box_.relative_bounds.bounds = gfx::RectF(20, 50, 200, 30);
  check_box_.AddIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                             button_.id);
  root_.child_ids.push_back(check_box_.id);

  text_field_.role = ax::mojom::Role::kTextField;
  text_field_.AddState(ax::mojom::State::kEditable);
  text_field_.SetValue(TEXT_VALUE);
  text_field_.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCachedLineStarts,
      std::vector<int32_t>{0, 7});
  text_field_.child_ids.push_back(static_text1_.id);
  text_field_.child_ids.push_back(line_break_.id);
  text_field_.child_ids.push_back(static_text2_.id);
  root_.child_ids.push_back(text_field_.id);

  static_text1_.role = ax::mojom::Role::kStaticText;
  static_text1_.AddState(ax::mojom::State::kEditable);
  static_text1_.SetName("Line 1");
  static_text1_.child_ids.push_back(inline_box1_.id);
  static_text1_.AddIntAttribute(
      ax::mojom::IntAttribute::kTextStyle,
      static_cast<int32_t>(ax::mojom::TextStyle::kBold));

  inline_box1_.role = ax::mojom::Role::kInlineTextBox;
  inline_box1_.AddState(ax::mojom::State::kEditable);
  inline_box1_.SetName("Line 1");
  inline_box1_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                   std::vector<int32_t>{0, 5});
  inline_box1_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds,
                                   std::vector<int32_t>{4, 6});
  inline_box1_.AddIntAttribute(ax::mojom::IntAttribute::kNextOnLineId,
                               line_break_.id);

  line_break_.role = ax::mojom::Role::kLineBreak;
  line_break_.AddState(ax::mojom::State::kEditable);
  line_break_.SetName("\n");
  line_break_.AddIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                              inline_box1_.id);

  static_text2_.role = ax::mojom::Role::kStaticText;
  static_text2_.AddState(ax::mojom::State::kEditable);
  static_text2_.SetName("Line 2");
  static_text2_.child_ids.push_back(inline_box2_.id);
  static_text2_.AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize, 1.0f);

  inline_box2_.role = ax::mojom::Role::kInlineTextBox;
  inline_box2_.AddState(ax::mojom::State::kEditable);
  inline_box2_.SetName("Line 2");
  inline_box2_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                   std::vector<int32_t>{0, 5});
  inline_box2_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds,
                                   std::vector<int32_t>{4, 6});

  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.push_back(root_);
  initial_state.nodes.push_back(button_);
  initial_state.nodes.push_back(check_box_);
  initial_state.nodes.push_back(text_field_);
  initial_state.nodes.push_back(static_text1_);
  initial_state.nodes.push_back(inline_box1_);
  initial_state.nodes.push_back(line_break_);
  initial_state.nodes.push_back(static_text2_);
  initial_state.nodes.push_back(inline_box2_);
  initial_state.has_tree_data = true;
  initial_state.tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
  initial_state.tree_data.title = "Dialog title";
  AXSerializableTree src_tree(initial_state);

  std::unique_ptr<AXTreeSource<const AXNode*, AXNodeData, AXTreeData>>
      tree_source(src_tree.CreateTreeSource());
  AXTreeSerializer<const AXNode*, AXNodeData, AXTreeData> serializer(
      tree_source.get());
  AXTreeUpdate update;
  serializer.SerializeChanges(src_tree.root(), &update);
  ASSERT_TRUE(tree_.Unserialize(update));
  AXNodePosition::SetTreeForTesting(&tree_);
}

void AXPositionTest::TearDown() {
  AXNodePosition::SetTreeForTesting(nullptr);
}

}  // namespace

TEST_F(AXPositionTest, Clone) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType copy_position = null_position->Clone();
  ASSERT_NE(nullptr, copy_position);
  EXPECT_TRUE(copy_position->IsNullPosition());

  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  copy_position = tree_position->Clone();
  ASSERT_NE(nullptr, copy_position);
  EXPECT_TRUE(copy_position->IsTreePosition());
  EXPECT_EQ(root_.id, copy_position->anchor_id());
  EXPECT_EQ(1, copy_position->child_index());
  EXPECT_EQ(AXNodePosition::INVALID_OFFSET, copy_position->text_offset());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, AXNodePosition::BEFORE_TEXT);
  ASSERT_NE(nullptr, tree_position);
  copy_position = tree_position->Clone();
  ASSERT_NE(nullptr, copy_position);
  EXPECT_TRUE(copy_position->IsTreePosition());
  EXPECT_EQ(root_.id, copy_position->anchor_id());
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, copy_position->child_index());
  EXPECT_EQ(AXNodePosition::INVALID_OFFSET, copy_position->text_offset());

  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  copy_position = text_position->Clone();
  ASSERT_NE(nullptr, copy_position);
  EXPECT_TRUE(copy_position->IsTextPosition());
  EXPECT_EQ(text_field_.id, copy_position->anchor_id());
  EXPECT_EQ(0, copy_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kUpstream, copy_position->affinity());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  copy_position = text_position->Clone();
  ASSERT_NE(nullptr, copy_position);
  EXPECT_TRUE(copy_position->IsTextPosition());
  EXPECT_EQ(text_field_.id, copy_position->anchor_id());
  EXPECT_EQ(0, copy_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, copy_position->affinity());
  EXPECT_EQ(AXNodePosition::INVALID_INDEX, copy_position->child_index());
}

TEST_F(AXPositionTest, GetTextFromNullPosition) {
  TestPositionType text_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsNullPosition());
  ASSERT_EQ(base::WideToUTF16(L""), text_position->GetText());
}

TEST_F(AXPositionTest, GetTextFromRoot) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(base::WideToUTF16(L"Line 1\nLine 2"), text_position->GetText());
}

TEST_F(AXPositionTest, GetTextFromButton) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, button_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(base::WideToUTF16(L""), text_position->GetText());
}

TEST_F(AXPositionTest, GetTextFromCheckbox) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, check_box_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(base::WideToUTF16(L""), text_position->GetText());
}

TEST_F(AXPositionTest, GetTextFromTextField) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(base::WideToUTF16(L"Line 1\nLine 2"), text_position->GetText());
}

TEST_F(AXPositionTest, GetTextFromStaticText) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, static_text1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(base::WideToUTF16(L"Line 1"), text_position->GetText());
}

TEST_F(AXPositionTest, GetTextFromInlineTextBox) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(base::WideToUTF16(L"Line 1"), text_position->GetText());
}

TEST_F(AXPositionTest, GetTextFromLineBreak) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(base::WideToUTF16(L"\n"), text_position->GetText());
}

TEST_F(AXPositionTest, GetMaxTextOffsetFromNullPosition) {
  TestPositionType text_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsNullPosition());
  ASSERT_EQ(AXNodePosition::INVALID_INDEX, text_position->MaxTextOffset());
}

TEST_F(AXPositionTest, GetMaxTextOffsetFromRoot) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(13, text_position->MaxTextOffset());
}

TEST_F(AXPositionTest, GetMaxTextOffsetFromButton) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, button_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(0, text_position->MaxTextOffset());
}

TEST_F(AXPositionTest, GetMaxTextOffsetFromCheckbox) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, check_box_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(0, text_position->MaxTextOffset());
}

TEST_F(AXPositionTest, GetMaxTextOffsetFromTextfield) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(13, text_position->MaxTextOffset());
}

TEST_F(AXPositionTest, GetMaxTextOffsetFromStaticText) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, static_text1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(6, text_position->MaxTextOffset());
}

TEST_F(AXPositionTest, GetMaxTextOffsetFromInlineTextBox) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(6, text_position->MaxTextOffset());
}

TEST_F(AXPositionTest, GetMaxTextOffsetFromLineBreak) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(1, text_position->MaxTextOffset());
}

TEST_F(AXPositionTest, AtStartOfAnchorWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  EXPECT_FALSE(null_position->AtStartOfAnchor());
}

TEST_F(AXPositionTest, AtStartOfAnchorWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  EXPECT_TRUE(tree_position->AtStartOfAnchor());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  EXPECT_FALSE(tree_position->AtStartOfAnchor());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 3 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  EXPECT_FALSE(tree_position->AtStartOfAnchor());

  // A "before text" position.
  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, AXNodePosition::BEFORE_TEXT);
  ASSERT_NE(nullptr, tree_position);
  EXPECT_TRUE(tree_position->AtStartOfAnchor());

  // An "after text" position.
  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  EXPECT_FALSE(tree_position->AtStartOfAnchor());
}

TEST_F(AXPositionTest, AtStartOfAnchorWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtStartOfAnchor());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtStartOfAnchor());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtStartOfAnchor());
}

TEST_F(AXPositionTest, AtEndOfAnchorWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  EXPECT_FALSE(null_position->AtEndOfAnchor());
}

TEST_F(AXPositionTest, AtEndOfAnchorWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 3 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  EXPECT_TRUE(tree_position->AtEndOfAnchor());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 2 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  EXPECT_FALSE(tree_position->AtEndOfAnchor());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  EXPECT_FALSE(tree_position->AtEndOfAnchor());
}

TEST_F(AXPositionTest, AtEndOfAnchorWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtEndOfAnchor());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtEndOfAnchor());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtEndOfAnchor());
}

TEST_F(AXPositionTest, AtStartOfLineWithTextPosition) {
  // An upstream affinity should not affect the outcome since there is no soft
  // line break.
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtStartOfLine());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtStartOfLine());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtStartOfLine());

  // An "after text" position anchored at the line break should not be the same
  // as a text position at the start of the next line.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtStartOfLine());

  // An upstream affinity should not affect the outcome since there is no soft
  // line break.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtStartOfLine());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtStartOfLine());
}

TEST_F(AXPositionTest, AtEndOfLineWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtEndOfLine());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtEndOfLine());

  // A "before text" position anchored at the line break should visually be the
  // same as a text position at the end of the previous line.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtEndOfLine());

  // The following position comes after the soft line break, so it should not be
  // marked as the end of the line.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtEndOfLine());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtEndOfLine());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtEndOfLine());
}

TEST_F(AXPositionTest, LowestCommonAncestor) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  // An "after children" position.
  TestPositionType root_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 3 /* child_index */);
  ASSERT_NE(nullptr, root_position);
  // A "before text" position.
  TestPositionType button_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, button_.id, AXNodePosition::BEFORE_TEXT);
  ASSERT_NE(nullptr, button_position);
  TestPositionType text_field_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 2 /* child_index */);
  ASSERT_NE(nullptr, text_field_position);
  TestPositionType static_text1_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, static_text1_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, static_text1_position);
  TestPositionType static_text2_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, static_text2_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, static_text2_position);
  TestPositionType inline_box1_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, inline_box1_position);
  ASSERT_TRUE(inline_box1_position->IsTextPosition());
  TestPositionType inline_box2_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, inline_box2_position);
  ASSERT_TRUE(inline_box2_position->IsTextPosition());

  TestPositionType test_position =
      root_position->LowestCommonAncestor(*null_position.get());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());

  test_position = root_position->LowestCommonAncestor(*root_position.get());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  // The child index should be for an "after children" position, i.e. it should
  // be unchanged.
  EXPECT_EQ(3, test_position->child_index());

  test_position =
      button_position->LowestCommonAncestor(*text_field_position.get());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  // The child index should point to the button.
  EXPECT_EQ(0, test_position->child_index());

  test_position =
      static_text2_position->LowestCommonAncestor(*static_text1_position.get());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(text_field_.id, test_position->anchor_id());
  // The child index should point to the second static text node.
  EXPECT_EQ(2, test_position->child_index());

  test_position =
      static_text1_position->LowestCommonAncestor(*text_field_position.get());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(text_field_.id, test_position->anchor_id());
  // The child index should point to the first static text node.
  EXPECT_EQ(0, test_position->child_index());

  test_position =
      inline_box1_position->LowestCommonAncestor(*inline_box2_position.get());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(text_field_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position =
      inline_box2_position->LowestCommonAncestor(*inline_box1_position.get());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(text_field_.id, test_position->anchor_id());
  // The text offset should point to the second line.
  EXPECT_EQ(7, test_position->text_offset());
}

TEST_F(AXPositionTest, AsTreePositionWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->AsTreePosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, AsTreePositionWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  TestPositionType test_position = tree_position->AsTreePosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->child_index());
  EXPECT_EQ(AXNodePosition::INVALID_OFFSET, test_position->text_offset());
}

TEST_F(AXPositionTest, AsTreePositionWithTextPosition) {
  // Create a text position pointing to the last character in the text field.
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 12 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position = text_position->AsTreePosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(text_field_.id, test_position->anchor_id());
  // The created tree position should point to the second static text node
  // inside the text field.
  EXPECT_EQ(2, test_position->child_index());
  // But its text offset should be unchanged.
  EXPECT_EQ(12, test_position->text_offset());

  // Test for a "before text" position.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsTreePosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, test_position->child_index());
  EXPECT_EQ(0, test_position->text_offset());

  // Test for an "after text" position.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsTreePosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());
  EXPECT_EQ(6, test_position->text_offset());
}

TEST_F(AXPositionTest, AsTextPositionWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->AsTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, AsTextPositionWithTreePosition) {
  // Create a tree position pointing to the line break node inside the text
  // field.
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  TestPositionType test_position = tree_position->AsTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(text_field_.id, test_position->anchor_id());
  // The created text position should point to the 6th character inside the text
  // field, i.e. the line break.
  EXPECT_EQ(6, test_position->text_offset());
  // But its child index should be unchanged.
  EXPECT_EQ(1, test_position->child_index());
  // And the affinity cannot be anything other than downstream because we
  // haven't moved up the tree and so there was no opportunity to introduce any
  // ambiguity regarding the new position.
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  // Test for a "before text" position.
  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, AXNodePosition::BEFORE_TEXT);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->AsTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, test_position->child_index());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  // Test for an "after text" position.
  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->AsTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  EXPECT_EQ(0, test_position->child_index());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, AsTextPositionWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position = text_position->AsTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(text_field_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
  EXPECT_EQ(AXNodePosition::INVALID_INDEX, test_position->child_index());
}

TEST_F(AXPositionTest, AsLeafTextPositionWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, AsLeafTextPositionWithTreePosition) {
  // Create a tree position pointing to the first static text node inside the
  // text field.
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  TestPositionType test_position = tree_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  // Create a tree position pointing to the line break node inside the text
  // field.
  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  // Create a text position pointing to the second static text node inside the
  // text field.
  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 2 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, AsLeafTextPositionWithTextPosition) {
  // Create a text position pointing to the end of the root (an "after text"
  // position).
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 13 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  // Even though upstream affinity doesn't make sense on a leaf node, there is
  // no need to reset it to downstream.
  EXPECT_EQ(ax::mojom::TextAffinity::kUpstream, test_position->affinity());

  // Create a text position on the root, pointing to the line break character
  // inside the text field but with an upstream affinity which will cause the
  // leaf text position to be placed after the text of the first inline text
  // box.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  // Even though upstream affinity doesn't make sense on a leaf node, there is
  // no need to reset it to downstream.
  EXPECT_EQ(ax::mojom::TextAffinity::kUpstream, test_position->affinity());

  // Create a text position pointing to the line break character inside the text
  // field but with an upstream affinity which will cause the leaf text position
  // to be placed after the text of the first inline text box.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  // Even though upstream affinity doesn't make sense on a leaf node, there is
  // no need to reset it to downstream.
  EXPECT_EQ(ax::mojom::TextAffinity::kUpstream, test_position->affinity());

  // Create a text position on the root, pointing to the line break character
  // inside the text field.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  // Create a text position pointing to the line break character inside the text
  // field.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  // Create a text position pointing to the offset after the last character in
  // the text field, (an "after text" position).
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 13 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, CreatePositionAtStartOfAnchorWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position =
      null_position->CreatePositionAtStartOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtStartOfAnchorWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  TestPositionType test_position =
      tree_position->CreatePositionAtStartOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->CreatePositionAtStartOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  // An "after text" position.
  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->CreatePositionAtStartOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, test_position->child_index());
}

TEST_F(AXPositionTest, CreatePositionAtStartOfAnchorWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position =
      text_position->CreatePositionAtStartOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->CreatePositionAtStartOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  // Affinity should have been reset to the default value.
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, CreatePositionAtEndOfAnchorWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->CreatePositionAtEndOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtEndOfAnchorWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 3 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  TestPositionType test_position = tree_position->CreatePositionAtEndOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  EXPECT_EQ(3, test_position->child_index());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->CreatePositionAtEndOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  EXPECT_EQ(3, test_position->child_index());
}

TEST_F(AXPositionTest, CreatePositionAtEndOfAnchorWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position = text_position->CreatePositionAtEndOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->CreatePositionAtEndOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  // Affinity should have been reset to the default value.
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, CreatePositionAtPreviousFormatStartWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position =
      null_position->CreatePreviousFormatStartPosition(
          AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
  test_position = null_position->CreatePreviousFormatStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtPreviousFormatStartWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  TestPositionType test_position =
      tree_position->CreatePreviousFormatStartPosition(
          AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(button_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  // AXBoundaryBehavior::StopIfAlreadyAtBoundary shouldn't move, since it's
  // already at a boundary.
  test_position = test_position->CreatePreviousFormatStartPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(button_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  // AXBoundaryBehavior::CrossBoundary should return a null position when it
  // reaches the start of the document
  test_position = test_position->CreatePreviousFormatStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtPreviousFormatStartWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 2 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position =
      text_position->CreatePreviousFormatStartPosition(
          AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  // AXBoundaryBehavior::StopIfAlreadyAtBoundary shouldn't move, since it's
  // already at a boundary.
  test_position = test_position->CreatePreviousFormatStartPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  // This time, it should move due to AXBoundaryBehavior::CrossBoundary
  test_position = test_position->CreatePreviousFormatStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(button_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
}

TEST_F(AXPositionTest, CreatePositionAtNextFormatEndWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
  test_position = null_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtNextFormatEndWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  TestPositionType test_position = tree_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  // AXBoundaryBehavior::StopIfAlreadyAtBoundary shouldn't move, since it's
  // already at a boundary.
  test_position = test_position->CreatePreviousFormatStartPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  // AXBoundaryBehavior::CrossBoundary should return a null position when it
  // reaches the end of the document
  test_position = test_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  test_position = test_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtNextFormatEndWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 2 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position = text_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());

  // AXBoundaryBehavior::StopIfAlreadyAtBoundary shouldn't move, since it's
  // already at a boundary.
  test_position = test_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());

  // This time, it should move due to AXBoundaryBehavior::CrossBoundary
  test_position = test_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->text_offset());

  test_position = test_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
}

TEST_F(AXPositionTest, CreatePositionAtFormatBoundaryWithTextPosition) {
  // This test updates the tree structure to test a specific edge case -
  // CreatePositionAtFormatBoundary when text lies at the beginning and end
  // of the AX tree.
  AXNodePosition::SetTreeForTesting(nullptr);

  ui::AXNodeData root_data;
  root_data.id = 0;
  root_data.role = ax::mojom::Role::kRootWebArea;

  ui::AXNodeData text_data;
  text_data.id = 1;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.SetName("some text");

  ui::AXNodeData more_text_data;
  more_text_data.id = 2;
  more_text_data.role = ax::mojom::Role::kStaticText;
  more_text_data.SetName("more text");

  root_data.child_ids = {1, 2};

  ui::AXTreeUpdate update;
  ui::AXTreeData tree_data;
  tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  update.tree_data = tree_data;
  update.has_tree_data = true;
  update.root_id = root_data.id;
  update.nodes = {root_data, text_data, more_text_data};

  std::unique_ptr<AXTree> new_tree;
  new_tree.reset(new AXTree(update));
  AXNodePosition::SetTreeForTesting(new_tree.get());

  // Test CreatePreviousFormatStartPosition at the start of the document.
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_data.id, 8 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  TestPositionType test_position =
      text_position->CreatePreviousFormatStartPosition(
          AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(text_data.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  // Test CreateNextFormatEndPosition at the end of the document.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, more_text_data.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  test_position = text_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(more_text_data.id, test_position->anchor_id());
  EXPECT_EQ(9, test_position->text_offset());

  AXNodePosition::SetTreeForTesting(&tree_);
}

TEST_F(AXPositionTest, CreatePositionAtStartOfDocumentWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position =
      null_position->CreatePositionAtStartOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtStartOfDocumentWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  TestPositionType test_position =
      tree_position->CreatePositionAtStartOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_EQ(root_.id, test_position->anchor_id());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->CreatePositionAtStartOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_EQ(root_.id, test_position->anchor_id());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->CreatePositionAtStartOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_EQ(root_.id, test_position->anchor_id());
}

TEST_F(AXPositionTest, CreatePositionAtStartOfDocumentWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  TestPositionType test_position =
      text_position->CreatePositionAtStartOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_EQ(root_.id, test_position->anchor_id());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  test_position = text_position->CreatePositionAtStartOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_EQ(root_.id, test_position->anchor_id());
  // Affinity should have been reset to the default value.
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, CreatePositionAtEndOfDocumentWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position =
      null_position->CreatePositionAtEndOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtEndOfDocumentWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 3 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  TestPositionType test_position =
      tree_position->CreatePositionAtEndOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->CreatePositionAtEndOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->CreatePositionAtEndOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
}

TEST_F(AXPositionTest, CreatePositionAtEndOfDocumentWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  TestPositionType test_position =
      text_position->CreatePositionAtEndOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  test_position = text_position->CreatePositionAtEndOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  // Affinity should have been reset to the default value.
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, CreateChildPositionAtWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->CreateChildPositionAt(0);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateChildPositionAtWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 2 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  TestPositionType test_position = tree_position->CreateChildPositionAt(1);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(check_box_.id, test_position->anchor_id());
  // Since the anchor is a leaf node, |child_index| should signify that this is
  // a "before text" position.
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, test_position->child_index());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, button_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->CreateChildPositionAt(0);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateChildPositionAtWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, static_text1_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position = text_position->CreateChildPositionAt(0);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, static_text2_.id, 4 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->CreateChildPositionAt(1);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateParentPositionWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->CreateParentPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateParentPositionWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, check_box_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  TestPositionType test_position = tree_position->CreateParentPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  // |child_index| should point to the check box node.
  EXPECT_EQ(1, test_position->child_index());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->CreateParentPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateParentPositionWithTextPosition) {
  // Create a position that points at the end of the first line, right after the
  // check box.
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, check_box_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position = text_position->CreateParentPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  // Since the same text offset in the root could be used to point to the
  // beginning of the second line, affinity should have been adjusted to
  // upstream.
  EXPECT_EQ(ax::mojom::TextAffinity::kUpstream, test_position->affinity());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->CreateParentPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(static_text2_.id, test_position->anchor_id());
  EXPECT_EQ(5, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  test_position = test_position->CreateParentPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(text_field_.id, test_position->anchor_id());
  // |text_offset| should point to the same offset on the second line where the
  // static text node position was pointing at.
  EXPECT_EQ(12, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest,
       CreateNextAndPreviousTextAnchorPositionWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position =
      null_position->CreateNextTextAnchorPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
  test_position = null_position->CreatePreviousTextAnchorPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateNextTextAnchorPosition) {
  TestPositionType check_box_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, check_box_position);
  TestPositionType test_position =
      check_box_position->CreateNextTextAnchorPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(check_box_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  // The text offset on the root points to the text coming from inside the text
  // field's first inline text box.
  TestPositionType root_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, root_position);
  ASSERT_TRUE(root_position->IsTextPosition());
  test_position = root_position->CreateNextTextAnchorPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  TestPositionType button_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, button_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, button_position);
  ASSERT_TRUE(button_position->IsTextPosition());
  test_position = button_position->CreateNextTextAnchorPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(check_box_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreateNextTextAnchorPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreateNextTextAnchorPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreateNextTextAnchorPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreateNextTextAnchorPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());

  TestPositionType text_field_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 2 /* child_index */);
  ASSERT_NE(nullptr, text_field_position);
  test_position = text_field_position->CreateNextTextAnchorPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
}

TEST_F(AXPositionTest, CreatePreviousTextAnchorPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position =
      text_position->CreatePreviousTextAnchorPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  // Create a "before text" tree position on the second line of the text box.
  TestPositionType before_text_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box2_.id, AXNodePosition::BEFORE_TEXT);
  ASSERT_NE(nullptr, before_text_position);
  test_position = before_text_position->CreatePreviousTextAnchorPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreatePreviousTextAnchorPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreatePreviousTextAnchorPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(check_box_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreatePreviousTextAnchorPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(button_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreatePreviousTextAnchorPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());

  TestPositionType text_field_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 2 /* child_index */);
  ASSERT_NE(nullptr, text_field_position);
  test_position = text_field_position->CreatePreviousTextAnchorPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(check_box_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  // The text offset on the root points to the text coming from inside the check
  // box.
  TestPositionType check_box_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, check_box_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, check_box_position);
  ASSERT_TRUE(check_box_position->IsTextPosition());
  test_position = check_box_position->CreatePreviousTextAnchorPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(button_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
}

TEST_F(AXPositionTest, AsPositionBeforeAndAfterCharacterWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  ASSERT_TRUE(null_position->IsNullPosition());
  TestPositionType test_position = null_position->AsPositionBeforeCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
  test_position = null_position->AsPositionAfterCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, AsPositionBeforeCharacter) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position = text_position->AsPositionBeforeCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 3 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsPositionBeforeCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(3, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsPositionBeforeCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsPositionBeforeCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsPositionBeforeCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, AsPositionAfterCharacter) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position = text_position->AsPositionAfterCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsPositionAfterCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(5, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsPositionAfterCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsPositionAfterCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 13 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsPositionAfterCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
}

TEST_F(AXPositionTest, CreateNextAndPreviousCharacterPositionWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
  test_position = null_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateNextCharacterPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 4 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(5, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopAtAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  test_position = test_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopAtAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  test_position = test_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->text_offset());

  test_position = test_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopAtAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->text_offset());
  test_position = test_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, check_box_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopAtAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(check_box_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->text_offset());
  // Affinity should have been reset to downstream.
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 12 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  // Affinity should have been reset to downstream.
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, CreatePreviousCharacterPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position =
      text_position->CreatePreviousCharacterPosition(
          AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(4, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopAtAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopAtAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  test_position = test_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(5, test_position->text_offset());

  test_position = test_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(4, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopAtAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  // Affinity should have been reset to downstream.
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, ReciprocalCreateNextAndPreviousCharacterPosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 0 /* child_index */);
  TestPositionType text_position = tree_position->AsTextPosition();
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  size_t next_character_moves = 0;
  while (!text_position->IsNullPosition()) {
    TestPositionType moved_position =
        text_position->CreateNextCharacterPosition(
            AXBoundaryBehavior::CrossBoundary);
    ASSERT_NE(nullptr, moved_position);

    text_position = std::move(moved_position);
    ++next_character_moves;
  }

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, root_.child_ids.size() /* child_index */);
  text_position = tree_position->AsTextPosition();
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  size_t previous_character_moves = 0;
  while (!text_position->IsNullPosition()) {
    TestPositionType moved_position =
        text_position->CreatePreviousCharacterPosition(
            AXBoundaryBehavior::CrossBoundary);
    ASSERT_NE(nullptr, moved_position);

    text_position = std::move(moved_position);
    ++previous_character_moves;
  }

  EXPECT_EQ(next_character_moves, previous_character_moves);
  EXPECT_EQ(strlen(TEXT_VALUE), next_character_moves - 1);
}

TEST_F(AXPositionTest, CreateNextAndPreviousWordStartPositionWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->CreateNextWordStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
  test_position = null_position->CreatePreviousWordStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateNextAndPreviousWordEndPositionWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->CreateNextWordEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
  test_position = null_position->CreatePreviousWordEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, OperatorEquals) {
  TestPositionType null_position1 = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position1);
  TestPositionType null_position2 = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position2);
  EXPECT_EQ(*null_position1, *null_position2);

  // Child indices must match.
  TestPositionType button_position1 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, button_position1);
  TestPositionType button_position2 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, button_position2);
  EXPECT_EQ(*button_position1, *button_position2);

  // Both child indices are invalid. It should result in equivalent null
  // positions.
  TestPositionType tree_position1 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 4 /* child_index */);
  ASSERT_NE(nullptr, tree_position1);
  TestPositionType tree_position2 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, AXNodePosition::INVALID_INDEX);
  ASSERT_NE(nullptr, tree_position2);
  EXPECT_EQ(*tree_position1, *tree_position2);

  // An invalid position should not be equivalent to an "after children"
  // position.
  tree_position1 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 3 /* child_index */);
  ASSERT_NE(nullptr, tree_position1);
  tree_position2 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, -1 /* child_index */);
  ASSERT_NE(nullptr, tree_position2);
  EXPECT_NE(*tree_position1, *tree_position2);

  // Two "after children" positions on the same node should be equivalent.
  tree_position1 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 3 /* child_index */);
  ASSERT_NE(nullptr, tree_position1);
  tree_position2 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 3 /* child_index */);
  ASSERT_NE(nullptr, tree_position2);
  EXPECT_EQ(*tree_position1, *tree_position2);

  // Two "before text" positions on the same node should be equivalent.
  tree_position1 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, AXNodePosition::BEFORE_TEXT);
  ASSERT_NE(nullptr, tree_position1);
  tree_position2 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, AXNodePosition::BEFORE_TEXT);
  ASSERT_NE(nullptr, tree_position2);
  EXPECT_EQ(*tree_position1, *tree_position2);

  // Both text offsets are invalid. It should result in equivalent null
  // positions.
  TestPositionType text_position1 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 15 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position1);
  ASSERT_TRUE(text_position1->IsNullPosition());
  TestPositionType text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, -1 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsNullPosition());
  EXPECT_EQ(*text_position1, *text_position2);

  text_position1 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position1);
  ASSERT_TRUE(text_position1->IsTextPosition());
  text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsTextPosition());
  EXPECT_EQ(*text_position1, *text_position2);

  // Affinities should not matter.
  text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsTextPosition());
  EXPECT_EQ(*text_position1, *text_position2);

  // Text offsets should match.
  text_position1 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position1);
  ASSERT_TRUE(text_position1->IsTextPosition());
  EXPECT_NE(*text_position1, *text_position2);

  // Two "after text" positions on the same node should be equivalent.
  text_position1 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position1);
  ASSERT_TRUE(text_position1->IsTextPosition());
  text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsTextPosition());
  EXPECT_EQ(*text_position1, *text_position2);

  // Two text positions that are consecutive, one "before text" and one "after
  // text".
  text_position1 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position1);
  ASSERT_TRUE(text_position1->IsTextPosition());
  text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsTextPosition());
  EXPECT_EQ(*text_position1, *text_position2);
}

TEST_F(AXPositionTest, OperatorEqualsSameTextOffsetSameAnchorId) {
  TestPositionType text_position_one = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position_one);
  ASSERT_TRUE(text_position_one->IsTextPosition());

  TestPositionType text_position_two = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position_two);
  ASSERT_TRUE(text_position_two->IsTextPosition());

  ASSERT_TRUE(*text_position_one == *text_position_two);
  ASSERT_TRUE(*text_position_two == *text_position_one);
}

TEST_F(AXPositionTest, OperatorEqualsSameTextOffsetDifferentAnchorIdRoot) {
  TestPositionType text_position_one = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position_one);
  ASSERT_TRUE(text_position_one->IsTextPosition());

  TestPositionType text_position_two = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, check_box_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position_two);
  ASSERT_TRUE(text_position_two->IsTextPosition());

  ASSERT_TRUE(*text_position_one == *text_position_two);
  ASSERT_TRUE(*text_position_two == *text_position_one);
}

TEST_F(AXPositionTest, OperatorEqualsSameTextOffsetDifferentAnchorIdLeaf) {
  TestPositionType text_position_one = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, button_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position_one);
  ASSERT_TRUE(text_position_one->IsTextPosition());

  TestPositionType text_position_two = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, check_box_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position_two);
  ASSERT_TRUE(text_position_two->IsTextPosition());

  ASSERT_TRUE(*text_position_one == *text_position_two);
  ASSERT_TRUE(*text_position_two == *text_position_one);
}

TEST_F(AXPositionTest, OperatorsLessThanAndGreaterThan) {
  TestPositionType null_position1 = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position1);
  TestPositionType null_position2 = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position2);
  EXPECT_FALSE(*null_position1 < *null_position2);
  EXPECT_FALSE(*null_position1 > *null_position2);

  TestPositionType button_position1 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, button_position1);
  TestPositionType button_position2 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, button_position2);
  EXPECT_LT(*button_position1, *button_position2);
  EXPECT_GT(*button_position2, *button_position1);

  TestPositionType tree_position1 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 2 /* child_index */);
  ASSERT_NE(nullptr, tree_position1);
  // An "after children" position.
  TestPositionType tree_position2 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 3 /* child_index */);
  ASSERT_NE(nullptr, tree_position2);
  EXPECT_LT(*tree_position1, *tree_position2);
  EXPECT_GT(*tree_position2, *tree_position1);

  // A "before text" position.
  tree_position1 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, AXNodePosition::BEFORE_TEXT);
  ASSERT_NE(nullptr, tree_position1);
  // An "after text" position.
  tree_position2 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position2);
  EXPECT_LT(*tree_position1, *tree_position2);
  EXPECT_GT(*tree_position2, *tree_position1);

  // Two text positions that share a common anchor.
  TestPositionType text_position1 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 2 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position1);
  ASSERT_TRUE(text_position1->IsTextPosition());
  TestPositionType text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsTextPosition());
  EXPECT_GT(*text_position1, *text_position2);
  EXPECT_LT(*text_position2, *text_position1);

  // Affinities should not matter.
  text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsTextPosition());
  EXPECT_GT(*text_position1, *text_position2);
  EXPECT_LT(*text_position2, *text_position1);

  // An "after text" position.
  text_position1 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position1);
  ASSERT_TRUE(text_position1->IsTextPosition());
  // A "before text" position.
  text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsTextPosition());
  EXPECT_GT(*text_position1, *text_position2);
  EXPECT_LT(*text_position2, *text_position1);

  // A text position that is an ancestor of another.
  text_position1 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position1);
  ASSERT_TRUE(text_position1->IsTextPosition());
  text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsTextPosition());
  EXPECT_GT(*text_position1, *text_position2);
  EXPECT_LT(*text_position2, *text_position1);

  // Two text positions that share a common ancestor.
  text_position1 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position1);
  ASSERT_TRUE(text_position1->IsTextPosition());
  text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsTextPosition());
  EXPECT_GT(*text_position1, *text_position2);
  EXPECT_LT(*text_position2, *text_position1);

  // Two consequtive positions. One "before text" and one "after text".
  text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsTextPosition());
  EXPECT_EQ(*text_position1, *text_position2);
}

TEST_F(AXPositionTest, CreateNextAnchorPosition) {
  // This test updates the tree structure to test a specific edge case -
  // CreateNextAnchorPosition on an empty text field.
  AXNodePosition::SetTreeForTesting(nullptr);

  ui::AXNodeData root_data;
  root_data.id = 0;
  root_data.role = ax::mojom::Role::kRootWebArea;

  ui::AXNodeData text_data;
  text_data.id = 1;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.SetName("some text");

  ui::AXNodeData text_field_data;
  text_field_data.id = 2;
  text_field_data.role = ax::mojom::Role::kTextField;

  ui::AXNodeData empty_text_data;
  empty_text_data.id = 3;
  empty_text_data.role = ax::mojom::Role::kStaticText;
  empty_text_data.SetName("");

  ui::AXNodeData more_text_data;
  more_text_data.id = 4;
  more_text_data.role = ax::mojom::Role::kStaticText;
  more_text_data.SetName("more text");

  root_data.child_ids = {1, 2, 4};
  text_field_data.child_ids = {3};

  ui::AXTreeUpdate update;
  ui::AXTreeData tree_data;
  tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  update.tree_data = tree_data;
  update.has_tree_data = true;
  update.root_id = root_data.id;
  update.nodes = {root_data, text_data, text_field_data, empty_text_data,
                  more_text_data};

  std::unique_ptr<AXTree> new_tree;
  new_tree.reset(new AXTree(update));
  AXNodePosition::SetTreeForTesting(new_tree.get());

  // Test that CreateNextAnchorPosition will successfully navigate past the
  // empty text field.
  TestPositionType text_position1 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, text_data.id, 8 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position1);
  ASSERT_FALSE(text_position1->CreateNextAnchorPosition()
                   ->CreateNextAnchorPosition()
                   ->IsNullPosition());

  AXNodePosition::SetTreeForTesting(&tree_);
}

//
// Parameterized tests.
//

TEST_P(AXPositionTestWithParam, TraverseTreeStartingWithAffinityDownstream) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, GetParam().start_node_id_, GetParam().start_offset_,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_TRUE(text_position->IsTextPosition());
  for (const std::string& expectation : GetParam().expectations) {
    text_position = GetParam().TestMethod.Run(text_position);
    EXPECT_NE(nullptr, text_position);
    EXPECT_EQ(expectation, text_position->ToString());
  }
}

TEST_P(AXPositionTestWithParam, TraverseTreeStartingWithAffinityUpstream) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, GetParam().start_node_id_, GetParam().start_offset_,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_TRUE(text_position->IsTextPosition());
  for (const std::string& expectation : GetParam().expectations) {
    text_position = GetParam().TestMethod.Run(text_position);
    EXPECT_NE(nullptr, text_position);
    EXPECT_EQ(expectation, text_position->ToString());
  }
}

//
// Instantiations of parameterized tests.
//

INSTANTIATE_TEST_SUITE_P(
    CreateNextWordStartPositionWithBoundaryBehaviorCrossBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordStartPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  ROOT_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=1 text_offset=5 "
                   "affinity=downstream annotated_text=Line <1>\nLine 2",
                   "TextPosition anchor_id=1 text_offset=7 "
                   "affinity=downstream annotated_text=Line 1\n<L>ine 2",
                   "TextPosition anchor_id=1 text_offset=12 "
                   "affinity=downstream annotated_text=Line 1\nLine <2>",
                   "TextPosition anchor_id=1 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordStartPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  TEXT_FIELD_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=4 text_offset=5 "
                   "affinity=downstream annotated_text=Line <1>\nLine 2",
                   "TextPosition anchor_id=4 text_offset=7 "
                   "affinity=downstream annotated_text=Line 1\n<L>ine 2",
                   "TextPosition anchor_id=4 text_offset=12 "
                   "affinity=downstream annotated_text=Line 1\nLine <2>",
                   "TextPosition anchor_id=4 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordStartPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  1 /* text_offset */,
                  {"TextPosition anchor_id=5 text_offset=5 "
                   "affinity=downstream annotated_text=Line <1>",
                   "TextPosition anchor_id=9 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 2",
                   "TextPosition anchor_id=9 text_offset=5 "
                   "affinity=downstream annotated_text=Line <2>",
                   "TextPosition anchor_id=9 text_offset=6 "
                   "affinity=downstream annotated_text=Line 2<>",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordStartPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=5 "
                   "affinity=downstream annotated_text=Line <2>",
                   "TextPosition anchor_id=9 text_offset=6 "
                   "affinity=downstream annotated_text=Line 2<>",
                   "NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextWordStartPositionWithBoundaryBehaviorStopAtAnchorBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordStartPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  ROOT_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=1 text_offset=5 "
                   "affinity=downstream annotated_text=Line <1>\nLine 2",
                   "TextPosition anchor_id=1 text_offset=7 "
                   "affinity=downstream annotated_text=Line 1\n<L>ine 2",
                   "TextPosition anchor_id=1 text_offset=12 "
                   "affinity=downstream annotated_text=Line 1\nLine <2>",
                   "TextPosition anchor_id=1 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordStartPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  TEXT_FIELD_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=4 text_offset=5 "
                   "affinity=downstream annotated_text=Line <1>\nLine 2",
                   "TextPosition anchor_id=4 text_offset=7 "
                   "affinity=downstream annotated_text=Line 1\n<L>ine 2",
                   "TextPosition anchor_id=4 text_offset=12 "
                   "affinity=downstream annotated_text=Line 1\nLine <2>",
                   "TextPosition anchor_id=4 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordStartPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  1 /* text_offset */,
                  {"TextPosition anchor_id=5 text_offset=5 "
                   "affinity=downstream annotated_text=Line <1>",
                   "TextPosition anchor_id=5 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<>"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordStartPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=5 "
                   "affinity=downstream annotated_text=Line <2>",
                   "TextPosition anchor_id=9 text_offset=6 "
                   "affinity=downstream annotated_text=Line 2<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextWordStartPositionWithBoundaryBehaviorStopIfAlreadyAtBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordStartPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  ROOT_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=1 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                   "TextPosition anchor_id=1 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordStartPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  TEXT_FIELD_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=4 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                   "TextPosition anchor_id=4 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordStartPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  1 /* text_offset */,
                  {"TextPosition anchor_id=5 text_offset=5 "
                   "affinity=downstream annotated_text=Line <1>",
                   "TextPosition anchor_id=5 text_offset=5 "
                   "affinity=downstream annotated_text=Line <1>"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordStartPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=5 "
                   "affinity=downstream annotated_text=Line <2>",
                   "TextPosition anchor_id=9 text_offset=5 "
                   "affinity=downstream annotated_text=Line <2>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousWordStartPositionWithBoundaryBehaviorCrossBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordStartPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  ROOT_ID,
                  13 /* text_offset at end of root. */,
                  {"TextPosition anchor_id=1 text_offset=12 "
                   "affinity=downstream annotated_text=Line 1\nLine <2>",
                   "TextPosition anchor_id=1 text_offset=7 "
                   "affinity=downstream annotated_text=Line 1\n<L>ine 2",
                   "TextPosition anchor_id=1 text_offset=5 "
                   "affinity=downstream annotated_text=Line <1>\nLine 2",
                   "TextPosition anchor_id=1 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordStartPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  TEXT_FIELD_ID,
                  13 /* text_offset at end of text field */,
                  {"TextPosition anchor_id=4 text_offset=12 "
                   "affinity=downstream annotated_text=Line 1\nLine <2>",
                   "TextPosition anchor_id=4 text_offset=7 "
                   "affinity=downstream annotated_text=Line 1\n<L>ine 2",
                   "TextPosition anchor_id=4 text_offset=5 "
                   "affinity=downstream annotated_text=Line <1>\nLine 2",
                   "TextPosition anchor_id=4 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordStartPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  5 /* text_offset */,
                  {"TextPosition anchor_id=5 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordStartPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 2",
                   "TextPosition anchor_id=6 text_offset=5 "
                   "affinity=downstream annotated_text=Line <1>",
                   "TextPosition anchor_id=6 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1",
                   "NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousWordStartPositionWithBoundaryBehaviorStopAtAnchorBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordStartPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  ROOT_ID,
                  13 /* text_offset at end of root. */,
                  {"TextPosition anchor_id=1 text_offset=12 "
                   "affinity=downstream annotated_text=Line 1\nLine <2>",
                   "TextPosition anchor_id=1 text_offset=7 "
                   "affinity=downstream annotated_text=Line 1\n<L>ine 2",
                   "TextPosition anchor_id=1 text_offset=5 "
                   "affinity=downstream annotated_text=Line <1>\nLine 2",
                   "TextPosition anchor_id=1 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                   "TextPosition anchor_id=1 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordStartPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  TEXT_FIELD_ID,
                  13 /* text_offset at end of text field */,
                  {"TextPosition anchor_id=4 text_offset=12 "
                   "affinity=downstream annotated_text=Line 1\nLine <2>",
                   "TextPosition anchor_id=4 text_offset=7 "
                   "affinity=downstream annotated_text=Line 1\n<L>ine 2",
                   "TextPosition anchor_id=4 text_offset=5 "
                   "affinity=downstream annotated_text=Line <1>\nLine 2",
                   "TextPosition anchor_id=4 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                   "TextPosition anchor_id=4 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordStartPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  5 /* text_offset */,
                  {"TextPosition anchor_id=5 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1",
                   "TextPosition anchor_id=5 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordStartPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 2",
                   "TextPosition anchor_id=9 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 2"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousWordStartPositionWithBoundaryBehaviorStopIfAlreadyAtBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordStartPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  ROOT_ID,
                  13 /* text_offset at end of root. */,
                  {"TextPosition anchor_id=1 text_offset=12 "
                   "affinity=downstream annotated_text=Line 1\nLine <2>",
                   "TextPosition anchor_id=1 text_offset=12 "
                   "affinity=downstream annotated_text=Line 1\nLine <2>"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordStartPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  TEXT_FIELD_ID,
                  13 /* text_offset at end of text field */,
                  {"TextPosition anchor_id=4 text_offset=12 "
                   "affinity=downstream annotated_text=Line 1\nLine <2>",
                   "TextPosition anchor_id=4 text_offset=12 "
                   "affinity=downstream annotated_text=Line 1\nLine <2>"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordStartPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  5 /* text_offset */,
                  {"TextPosition anchor_id=5 text_offset=5 "
                   "affinity=downstream annotated_text=Line <1>"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordStartPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 2",
                   "TextPosition anchor_id=9 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 2"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextWordEndPositionWithBoundaryBehaviorCrossBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordEndPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  ROOT_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=1 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >1\nLine 2",
                   "TextPosition anchor_id=1 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2",
                   "TextPosition anchor_id=1 text_offset=11 "
                   "affinity=downstream annotated_text=Line 1\nLine< >2",
                   "TextPosition anchor_id=1 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordEndPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  TEXT_FIELD_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=4 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >1\nLine 2",
                   "TextPosition anchor_id=4 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2",
                   "TextPosition anchor_id=4 text_offset=11 "
                   "affinity=downstream annotated_text=Line 1\nLine< >2",
                   "TextPosition anchor_id=4 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordEndPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  1 /* text_offset */,
                  {"TextPosition anchor_id=5 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >1",
                   "TextPosition anchor_id=5 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<>",
                   "TextPosition anchor_id=9 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >2",
                   "TextPosition anchor_id=9 text_offset=6 "
                   "affinity=downstream annotated_text=Line 2<>",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordEndPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=6 "
                   "affinity=downstream annotated_text=Line 2<>",
                   "NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextWordEndPositionWithBoundaryBehaviorStopAtAnchorBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordEndPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  ROOT_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=1 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >1\nLine 2",
                   "TextPosition anchor_id=1 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2",
                   "TextPosition anchor_id=1 text_offset=11 "
                   "affinity=downstream annotated_text=Line 1\nLine< >2",
                   "TextPosition anchor_id=1 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>",
                   "TextPosition anchor_id=1 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordEndPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  TEXT_FIELD_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=4 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >1\nLine 2",
                   "TextPosition anchor_id=4 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2",
                   "TextPosition anchor_id=4 text_offset=11 "
                   "affinity=downstream annotated_text=Line 1\nLine< >2",
                   "TextPosition anchor_id=4 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>",
                   "TextPosition anchor_id=4 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordEndPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  1 /* text_offset */,
                  {"TextPosition anchor_id=5 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >1",
                   "TextPosition anchor_id=5 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<>",
                   "TextPosition anchor_id=5 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<>"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordEndPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=6 "
                   "affinity=downstream annotated_text=Line 2<>",
                   "TextPosition anchor_id=9 text_offset=6 "
                   "affinity=downstream annotated_text=Line 2<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextWordEndPositionWithBoundaryBehaviorStopIfAlreadyAtBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordEndPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  ROOT_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=1 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >1\nLine 2",
                   "TextPosition anchor_id=1 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >1\nLine 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordEndPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  TEXT_FIELD_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=4 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >1\nLine 2",
                   "TextPosition anchor_id=4 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >1\nLine 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordEndPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  1 /* text_offset */,
                  {"TextPosition anchor_id=5 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >1",
                   "TextPosition anchor_id=5 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >1"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextWordEndPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >2"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousWordEndPositionWithBoundaryBehaviorCrossBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordEndPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  ROOT_ID,
                  13 /* text_offset at end of root. */,
                  {"TextPosition anchor_id=1 text_offset=11 "
                   "affinity=downstream annotated_text=Line 1\nLine< >2",
                   "TextPosition anchor_id=1 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2",
                   "TextPosition anchor_id=1 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >1\nLine 2",
                   "TextPosition anchor_id=1 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordEndPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  TEXT_FIELD_ID,
                  13 /* text_offset at end of text field */,
                  {"TextPosition anchor_id=4 text_offset=11 "
                   "affinity=downstream annotated_text=Line 1\nLine< >2",
                   "TextPosition anchor_id=4 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2",
                   "TextPosition anchor_id=4 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >1\nLine 2",
                   "TextPosition anchor_id=4 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordEndPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  5 /* text_offset */,
                  {"TextPosition anchor_id=5 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >1",
                   "TextPosition anchor_id=5 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordEndPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=6 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<>",
                   "TextPosition anchor_id=6 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >1",
                   "TextPosition anchor_id=6 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1",
                   "NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousWordEndPositionWithBoundaryBehaviorStopAtAnchorBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordEndPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  ROOT_ID,
                  13 /* text_offset at end of root. */,
                  {
                      "TextPosition anchor_id=1 text_offset=11 "
                      "affinity=downstream annotated_text=Line 1\nLine< >2",
                      "TextPosition anchor_id=1 text_offset=6 "
                      "affinity=downstream annotated_text=Line 1<\n>Line 2",
                      "TextPosition anchor_id=1 text_offset=4 "
                      "affinity=downstream annotated_text=Line< >1\nLine 2",
                      "TextPosition anchor_id=1 text_offset=0 "
                      "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                  }},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordEndPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  TEXT_FIELD_ID,
                  13 /* text_offset at end of text field */,
                  {"TextPosition anchor_id=4 text_offset=11 "
                   "affinity=downstream annotated_text=Line 1\nLine< >2",
                   "TextPosition anchor_id=4 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2",
                   "TextPosition anchor_id=4 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >1\nLine 2",
                   "TextPosition anchor_id=4 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordEndPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  5 /* text_offset */,
                  {"TextPosition anchor_id=5 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >1",
                   "TextPosition anchor_id=5 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordEndPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 2"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousWordEndPositionWithBoundaryBehaviorStopIfAlreadyAtBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordEndPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  ROOT_ID,
                  13 /* text_offset at end of root. */,
                  {"TextPosition anchor_id=1 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordEndPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  TEXT_FIELD_ID,
                  13 /* text_offset at end of text field */,
                  {"TextPosition anchor_id=4 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordEndPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  5 /* text_offset */,
                  {"TextPosition anchor_id=5 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >1",
                   "TextPosition anchor_id=5 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >1"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousWordEndPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=4 "
                   "affinity=downstream annotated_text=Line< >2"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextLineStartPositionWithBoundaryBehaviorCrossBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineStartPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  ROOT_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=1 text_offset=7 "
                   "affinity=downstream annotated_text=Line 1\n<L>ine 2",
                   "TextPosition anchor_id=1 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineStartPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  TEXT_FIELD_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=4 text_offset=7 "
                   "affinity=downstream annotated_text=Line 1\n<L>ine 2",
                   "TextPosition anchor_id=4 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineStartPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  1 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 2",
                   "TextPosition anchor_id=9 text_offset=6 "
                   "affinity=downstream annotated_text=Line 2<>",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineStartPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=6 "
                   "affinity=downstream annotated_text=Line 2<>",
                   "NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextLineStartPositionWithBoundaryBehaviorStopAtAnchorBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineStartPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  ROOT_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=1 text_offset=7 "
                   "affinity=downstream annotated_text=Line 1\n<L>ine 2",
                   "TextPosition anchor_id=1 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineStartPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  TEXT_FIELD_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=4 text_offset=7 "
                   "affinity=downstream annotated_text=Line 1\n<L>ine 2",
                   "TextPosition anchor_id=4 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineStartPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  1 /* text_offset */,
                  {"TextPosition anchor_id=5 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<>"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineStartPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=6 "
                   "affinity=downstream annotated_text=Line 2<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextLineStartPositionWithBoundaryBehaviorStopIfAlreadyAtBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineStartPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  ROOT_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=1 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                   "TextPosition anchor_id=1 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineStartPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  TEXT_FIELD_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=4 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                   "TextPosition anchor_id=4 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineStartPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  1 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 2",
                   "TextPosition anchor_id=9 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineStartPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=6 affinity=downstream "
                   "annotated_text=Line 2<>",
                   "TextPosition anchor_id=9 text_offset=6 affinity=downstream "
                   "annotated_text=Line 2<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousLineStartPositionWithBoundaryBehaviorCrossBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineStartPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  ROOT_ID,
                  13 /* text_offset at the end of root. */,
                  {"TextPosition anchor_id=1 text_offset=7 "
                   "affinity=downstream annotated_text=Line 1\n<L>ine 2",
                   "TextPosition anchor_id=1 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineStartPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  TEXT_FIELD_ID,
                  13 /* text_offset at end of text field */,
                  {"TextPosition anchor_id=4 text_offset=7 "
                   "affinity=downstream annotated_text=Line 1\n<L>ine 2",
                   "TextPosition anchor_id=4 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineStartPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  5 /* text_offset */,
                  {"TextPosition anchor_id=5 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineStartPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 2",
                   "TextPosition anchor_id=6 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1",
                   "NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousLineStartPositionWithBoundaryBehaviorStopAtAnchorBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineStartPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  ROOT_ID,
                  13 /* text_offset at the end of root. */,
                  {"TextPosition anchor_id=1 text_offset=7 "
                   "affinity=downstream annotated_text=Line 1\n<L>ine 2",
                   "TextPosition anchor_id=1 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                   "TextPosition anchor_id=1 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineStartPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  TEXT_FIELD_ID,
                  13 /* text_offset at end of text field */,
                  {"TextPosition anchor_id=4 text_offset=7 "
                   "affinity=downstream annotated_text=Line 1\n<L>ine 2",
                   "TextPosition anchor_id=4 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                   "TextPosition anchor_id=4 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineStartPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  5 /* text_offset */,
                  {"TextPosition anchor_id=5 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1",
                   "TextPosition anchor_id=5 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineStartPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 2",
                   "TextPosition anchor_id=9 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 2"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousLineStartPositionWithBoundaryBehaviorStopIfAlreadyAtBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineStartPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  ROOT_ID,
                  13 /* text_offset at the end of root. */,
                  {"TextPosition anchor_id=1 text_offset=7 "
                   "affinity=downstream annotated_text=Line 1\n<L>ine 2",
                   "TextPosition anchor_id=1 text_offset=7 "
                   "affinity=downstream annotated_text=Line 1\n<L>ine 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineStartPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  TEXT_FIELD_ID,
                  13 /* text_offset at end of text field */,
                  {"TextPosition anchor_id=4 text_offset=7 "
                   "affinity=downstream annotated_text=Line 1\n<L>ine 2",
                   "TextPosition anchor_id=4 text_offset=7 "
                   "affinity=downstream annotated_text=Line 1\n<L>ine 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineStartPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  5 /* text_offset */,
                  {"TextPosition anchor_id=5 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1",
                   "TextPosition anchor_id=5 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineStartPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 2",
                   "TextPosition anchor_id=9 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 2"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextLineEndPositionWithBoundaryBehaviorCrossBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineEndPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  ROOT_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=1 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2",
                   "TextPosition anchor_id=1 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineEndPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  TEXT_FIELD_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=4 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2",
                   "TextPosition anchor_id=4 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineEndPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  1 /* text_offset */,
                  {"TextPosition anchor_id=5 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<>",
                   "TextPosition anchor_id=9 text_offset=6 "
                   "affinity=downstream annotated_text=Line 2<>",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineEndPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=6 "
                   "affinity=downstream annotated_text=Line 2<>",
                   "NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextLineEndPositionWithBoundaryBehaviorStopAtAnchorBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineEndPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  ROOT_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=1 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2",
                   "TextPosition anchor_id=1 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>",
                   "TextPosition anchor_id=1 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineEndPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  TEXT_FIELD_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=4 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2",
                   "TextPosition anchor_id=4 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>",
                   "TextPosition anchor_id=4 text_offset=13 "
                   "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineEndPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  1 /* text_offset */,
                  {"TextPosition anchor_id=5 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<>",
                   "TextPosition anchor_id=5 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<>"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineEndPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=6 "
                   "affinity=downstream annotated_text=Line 2<>",
                   "TextPosition anchor_id=9 text_offset=6 "
                   "affinity=downstream annotated_text=Line 2<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextLineEndPositionWithBoundaryBehaviorStopIfAlreadyAtBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineEndPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  ROOT_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=1 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2",
                   "TextPosition anchor_id=1 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineEndPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  TEXT_FIELD_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=4 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2",
                   "TextPosition anchor_id=4 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineEndPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  STATIC_TEXT1_ID,
                  1 /* text_offset */,
                  {"TextPosition anchor_id=5 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<>",
                   "TextPosition anchor_id=5 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<>"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreateNextLineEndPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=6 "
                   "affinity=downstream annotated_text=Line 2<>",
                   "TextPosition anchor_id=9 text_offset=6 "
                   "affinity=downstream annotated_text=Line 2<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousLineEndPositionWithBoundaryBehaviorCrossBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineEndPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  ROOT_ID,
                  13 /* text_offset at end of root. */,
                  {"TextPosition anchor_id=1 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2",
                   "TextPosition anchor_id=1 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineEndPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  TEXT_FIELD_ID,
                  13 /* text_offset at end of text field */,
                  {"TextPosition anchor_id=4 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2",
                   "TextPosition anchor_id=4 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineEndPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  ROOT_ID,
                  5 /* text_offset on the last character of "Line 1". */,
                  {"TextPosition anchor_id=1 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineEndPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  TEXT_FIELD_ID,
                  5 /* text_offset on the last character of "Line 1". */,
                  {"TextPosition anchor_id=4 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineEndPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=6 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<>",
                   "TextPosition anchor_id=6 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1",
                   "NullPosition"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineEndPosition(
                        AXBoundaryBehavior::CrossBoundary);
                  }),
                  INLINE_BOX2_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=6 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<>",
                   "TextPosition anchor_id=6 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1",
                   "NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousLineEndPositionWithBoundaryBehaviorStopAtAnchorBoundary,
    AXPositionTestWithParam,
    testing::Values(
        // Note that for the first two tests we can't go past the line ending at
        // "Line 1" to test for "NullPosition'", because the text position at
        // the beginning of the soft line break is equivalent to the position at
        // the end of the line's text and so an infinite recursion will occur.
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineEndPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  ROOT_ID,
                  13 /* text_offset at end of root. */,
                  {"TextPosition anchor_id=1 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2",
                   "TextPosition anchor_id=1 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineEndPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  TEXT_FIELD_ID,
                  13 /* text_offset at end of text field */,
                  {"TextPosition anchor_id=4 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2",
                   "TextPosition anchor_id=4 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineEndPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  ROOT_ID,
                  5 /* text_offset on the last character of "Line 1". */,
                  {"TextPosition anchor_id=1 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                   "TextPosition anchor_id=1 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineEndPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  TEXT_FIELD_ID,
                  5 /* text_offset on the last character of "Line 1". */,
                  {"TextPosition anchor_id=4 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2",
                   "TextPosition anchor_id=4 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineEndPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 2",
                   "TextPosition anchor_id=9 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineEndPosition(
                        AXBoundaryBehavior::StopAtAnchorBoundary);
                  }),
                  INLINE_BOX2_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=9 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 2",
                   "TextPosition anchor_id=9 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 2"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousLineEndPositionWithBoundaryBehaviorStopIfAlreadyAtBoundary,
    AXPositionTestWithParam,
    testing::Values(
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineEndPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  ROOT_ID,
                  12 /* text_offset one before the end of root. */,
                  {"TextPosition anchor_id=1 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2",
                   "TextPosition anchor_id=1 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineEndPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  TEXT_FIELD_ID,
                  12 /* text_offset one before the end of text field */,
                  {"TextPosition anchor_id=4 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2",
                   "TextPosition anchor_id=4 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<\n>Line 2"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineEndPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  INLINE_BOX1_ID,
                  2 /* text_offset */,
                  {"TextPosition anchor_id=6 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1",
                   "TextPosition anchor_id=6 text_offset=0 "
                   "affinity=downstream annotated_text=<L>ine 1"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineEndPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  INLINE_BOX2_ID,
                  4 /* text_offset */,
                  {"TextPosition anchor_id=6 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<>",
                   "TextPosition anchor_id=6 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<>"}},
        TestParam{base::BindRepeating([](const TestPositionType& position) {
                    return position->CreatePreviousLineEndPosition(
                        AXBoundaryBehavior::StopIfAlreadyAtBoundary);
                  }),
                  INLINE_BOX2_ID,
                  0 /* text_offset */,
                  {"TextPosition anchor_id=6 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<>",
                   "TextPosition anchor_id=6 text_offset=6 "
                   "affinity=downstream annotated_text=Line 1<>"}}));
}  // namespace ui
