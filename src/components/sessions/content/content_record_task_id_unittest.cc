// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/content_record_task_id.h"

#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sessions {

class ContentRecordTaskIDTest : public testing::Test {
 public:
  ContentRecordTaskIDTest() {}
  ~ContentRecordTaskIDTest() override {}

  void SetUp() override {
    navigation_entry_ = content::NavigationEntry::Create();
  }

 protected:
  std::unique_ptr<content::NavigationEntry> navigation_entry_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentRecordTaskIDTest);
};

TEST_F(ContentRecordTaskIDTest, TaskIDTest) {
  ContextRecordTaskId* context_record_task_id =
      ContextRecordTaskId::Get(navigation_entry_.get());
  context_record_task_id->set_task_id(test_data::kTaskId);
  context_record_task_id->set_parent_task_id(test_data::kParentTaskId);
  context_record_task_id->set_root_task_id(test_data::kRootTaskId);
  context_record_task_id->set_children_task_ids(test_data::kChildrenTaskIds);

  EXPECT_EQ(test_data::kTaskId,
            ContextRecordTaskId::Get(navigation_entry_.get())->task_id());
  EXPECT_EQ(
      test_data::kParentTaskId,
      ContextRecordTaskId::Get(navigation_entry_.get())->parent_task_id());
  EXPECT_EQ(test_data::kRootTaskId,
            ContextRecordTaskId::Get(navigation_entry_.get())->root_task_id());
  EXPECT_EQ(
      test_data::kChildrenTaskIds,
      ContextRecordTaskId::Get(navigation_entry_.get())->children_task_ids());

  ContextRecordTaskId cloned_context_record_task_id(*context_record_task_id);

  EXPECT_EQ(test_data::kTaskId, cloned_context_record_task_id.task_id());
  EXPECT_EQ(test_data::kParentTaskId,
            cloned_context_record_task_id.parent_task_id());
  EXPECT_EQ(test_data::kRootTaskId,
            cloned_context_record_task_id.root_task_id());
  EXPECT_EQ(test_data::kChildrenTaskIds,
            cloned_context_record_task_id.children_task_ids());
}

}  // namespace sessions
