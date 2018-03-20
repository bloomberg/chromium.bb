// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/MultiColumnFragmentainerGroup.h"

#include "core/layout/LayoutMultiColumnFlowThread.h"
#include "core/layout/LayoutMultiColumnSet.h"
#include "core/testing/CoreUnitTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class MultiColumnFragmentainerGroupTest : public RenderingTest {
 public:
  MultiColumnFragmentainerGroupTest()
      : flow_thread_(nullptr), column_set_(nullptr) {}

 protected:
  void SetUp() override;
  void TearDown() override;

  LayoutMultiColumnSet& ColumnSet() { return *column_set_; }

  static int GroupCount(const MultiColumnFragmentainerGroupList&);

 private:
  LayoutMultiColumnFlowThread* flow_thread_;
  LayoutMultiColumnSet* column_set_;
};

void MultiColumnFragmentainerGroupTest::SetUp() {
  RenderingTest::SetUp();
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  flow_thread_ =
      LayoutMultiColumnFlowThread::CreateAnonymous(GetDocument(), *style.get());
  column_set_ = LayoutMultiColumnSet::CreateAnonymous(*flow_thread_,
                                                      *flow_thread_->Style());
}

void MultiColumnFragmentainerGroupTest::TearDown() {
  column_set_->Destroy();
  flow_thread_->Destroy();
  RenderingTest::TearDown();
}

int MultiColumnFragmentainerGroupTest::GroupCount(
    const MultiColumnFragmentainerGroupList& group_list) {
  int count = 0;
  for (const auto& dummy_group : group_list) {
    (void)dummy_group;
    count++;
  }
  return count;
}

TEST_F(MultiColumnFragmentainerGroupTest, Create) {
  MultiColumnFragmentainerGroupList group_list(ColumnSet());
  EXPECT_EQ(GroupCount(group_list), 1);
}

TEST_F(MultiColumnFragmentainerGroupTest, DeleteExtra) {
  MultiColumnFragmentainerGroupList group_list(ColumnSet());
  EXPECT_EQ(GroupCount(group_list), 1);
  group_list.DeleteExtraGroups();
  EXPECT_EQ(GroupCount(group_list), 1);
}

TEST_F(MultiColumnFragmentainerGroupTest, AddThenDeleteExtra) {
  MultiColumnFragmentainerGroupList group_list(ColumnSet());
  EXPECT_EQ(GroupCount(group_list), 1);
  group_list.AddExtraGroup();
  EXPECT_EQ(GroupCount(group_list), 2);
  group_list.DeleteExtraGroups();
  EXPECT_EQ(GroupCount(group_list), 1);
}

TEST_F(MultiColumnFragmentainerGroupTest,
       AddTwoThenDeleteExtraThenAddThreeThenDeleteExtra) {
  MultiColumnFragmentainerGroupList group_list(ColumnSet());
  EXPECT_EQ(GroupCount(group_list), 1);
  group_list.AddExtraGroup();
  EXPECT_EQ(GroupCount(group_list), 2);
  group_list.AddExtraGroup();
  EXPECT_EQ(GroupCount(group_list), 3);
  group_list.DeleteExtraGroups();
  EXPECT_EQ(GroupCount(group_list), 1);
  group_list.AddExtraGroup();
  EXPECT_EQ(GroupCount(group_list), 2);
  group_list.AddExtraGroup();
  EXPECT_EQ(GroupCount(group_list), 3);
  group_list.AddExtraGroup();
  EXPECT_EQ(GroupCount(group_list), 4);
  group_list.DeleteExtraGroups();
  EXPECT_EQ(GroupCount(group_list), 1);
}

// The following test tests that we restrict actual column count, to not run
// into unnecessary performance problems. The code that applies this limitation
// is in MultiColumnFragmentainerGroup::ActualColumnCount().
TEST_F(MultiColumnFragmentainerGroupTest, ShortColumnTallContent) {
  SetBodyInnerHTML(R"HTML(
    <div id="multicol" style="columns:3; column-gap:1px; width:101px; height:1px;">
      <div style="height:1000000px;"></div>
    </div>
  )HTML");
  const auto* multicol = GetLayoutObjectByElementId("multicol");
  ASSERT_TRUE(multicol);
  ASSERT_TRUE(multicol->IsLayoutBlockFlow());
  const auto* column_set = multicol->SlowLastChild();
  ASSERT_TRUE(column_set);
  ASSERT_TRUE(column_set->IsLayoutMultiColumnSet());
  const auto& fragmentainer_group =
      ToLayoutMultiColumnSet(column_set)->FirstFragmentainerGroup();
  EXPECT_EQ(fragmentainer_group.ActualColumnCount(), 10U);
  EXPECT_EQ(fragmentainer_group.GroupLogicalHeight(), LayoutUnit(1));
  auto overflow = ToLayoutBox(multicol)->LayoutOverflowRect();
  EXPECT_EQ(ToLayoutBox(multicol)->LogicalWidth(), LayoutUnit(101));
  EXPECT_EQ(ToLayoutBox(multicol)->LogicalHeight(), LayoutUnit(1));
  EXPECT_EQ(overflow.Width(), LayoutUnit(339));
  EXPECT_EQ(overflow.Height(), LayoutUnit(999991));
}

// The following test tests that we restrict actual column count, to not run
// into unnecessary performance problems. The code that applies this limitation
// is in MultiColumnFragmentainerGroup::ActualColumnCount().
TEST_F(MultiColumnFragmentainerGroupTest, MediumTallColumnTallContent) {
  SetBodyInnerHTML(R"HTML(
    <div id="multicol" style="columns:3; column-gap:1px; width:101px; height:100px;">
      <div style="height:1000000px;"></div>
    </div>
  )HTML");
  const auto* multicol = GetLayoutObjectByElementId("multicol");
  ASSERT_TRUE(multicol);
  ASSERT_TRUE(multicol->IsLayoutBlockFlow());
  const auto* column_set = multicol->SlowLastChild();
  ASSERT_TRUE(column_set);
  ASSERT_TRUE(column_set->IsLayoutMultiColumnSet());
  const auto& fragmentainer_group =
      ToLayoutMultiColumnSet(column_set)->FirstFragmentainerGroup();
  EXPECT_EQ(fragmentainer_group.ActualColumnCount(), 100U);
  EXPECT_EQ(fragmentainer_group.GroupLogicalHeight(), LayoutUnit(100));
  auto overflow = ToLayoutBox(multicol)->LayoutOverflowRect();
  EXPECT_EQ(ToLayoutBox(multicol)->LogicalWidth(), LayoutUnit(101));
  EXPECT_EQ(ToLayoutBox(multicol)->LogicalHeight(), LayoutUnit(100));
  EXPECT_EQ(overflow.Width(), LayoutUnit(3399));
  EXPECT_EQ(overflow.Height(), LayoutUnit(990100));
}

// The following test tests that we restrict actual column count, to not run
// into unnecessary performance problems. The code that applies this limitation
// is in MultiColumnFragmentainerGroup::ActualColumnCount().
TEST_F(MultiColumnFragmentainerGroupTest, TallColumnTallContent) {
  SetBodyInnerHTML(R"HTML(
    <div id="multicol" style="columns:3; column-gap:1px; width:101px; height:1000px;">
      <div style="height:1000000px;"></div>
    </div>
  )HTML");
  const auto* multicol = GetLayoutObjectByElementId("multicol");
  ASSERT_TRUE(multicol);
  ASSERT_TRUE(multicol->IsLayoutBlockFlow());
  const auto* column_set = multicol->SlowLastChild();
  ASSERT_TRUE(column_set);
  ASSERT_TRUE(column_set->IsLayoutMultiColumnSet());
  const auto& fragmentainer_group =
      ToLayoutMultiColumnSet(column_set)->FirstFragmentainerGroup();
  EXPECT_EQ(fragmentainer_group.ActualColumnCount(), 500U);
  EXPECT_EQ(fragmentainer_group.GroupLogicalHeight(), LayoutUnit(1000));
  auto overflow = ToLayoutBox(multicol)->LayoutOverflowRect();
  EXPECT_EQ(ToLayoutBox(multicol)->LogicalWidth(), LayoutUnit(101));
  EXPECT_EQ(ToLayoutBox(multicol)->LogicalHeight(), LayoutUnit(1000));
  EXPECT_EQ(overflow.Width(), LayoutUnit(16999));
  EXPECT_EQ(overflow.Height(), LayoutUnit(501000));
}

}  // anonymous namespace

}  // namespace blink
