// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/MultiColumnFragmentainerGroup.h"

#include "core/layout/LayoutMultiColumnFlowThread.h"
#include "core/layout/LayoutMultiColumnSet.h"
#include "core/layout/LayoutTestHelper.h"
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

}  // anonymous namespace

}  // namespace blink
