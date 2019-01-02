// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_journal_mutation.h"

#include "components/feed/core/feed_journal_operation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

namespace {

const char kJournalName1[] = "Journal1";
const char kJournalName2[] = "Journal2";
const char kJournalValue[] = "value";

}  // namespace

class FeedJournalMutationTest : public testing::Test {
 public:
  FeedJournalMutationTest() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(FeedJournalMutationTest);
};

TEST_F(FeedJournalMutationTest, AddAppendOperation) {
  JournalMutation mutation(kJournalName1);
  EXPECT_EQ(mutation.journal_name(), kJournalName1);
  EXPECT_TRUE(mutation.Empty());

  mutation.AddAppendOperation(kJournalValue);
  EXPECT_FALSE(mutation.Empty());

  JournalOperation operation = mutation.TakeFristOperation();
  EXPECT_TRUE(mutation.Empty());
  EXPECT_EQ(operation.type(), JournalOperation::JOURNAL_APPEND);
  EXPECT_EQ(operation.value(), kJournalValue);
}

TEST_F(FeedJournalMutationTest, AddCopyOperation) {
  JournalMutation mutation(kJournalName1);
  EXPECT_EQ(mutation.journal_name(), kJournalName1);
  EXPECT_TRUE(mutation.Empty());

  mutation.AddCopyOperation(kJournalName2);
  EXPECT_FALSE(mutation.Empty());

  JournalOperation operation = mutation.TakeFristOperation();
  EXPECT_TRUE(mutation.Empty());
  EXPECT_EQ(operation.type(), JournalOperation::JOURNAL_COPY);
  EXPECT_EQ(operation.to_journal_name(), kJournalName2);
}

TEST_F(FeedJournalMutationTest, AddDeleteOperation) {
  JournalMutation mutation(kJournalName1);
  EXPECT_EQ(mutation.journal_name(), kJournalName1);
  EXPECT_TRUE(mutation.Empty());

  mutation.AddDeleteOperation();
  EXPECT_FALSE(mutation.Empty());

  JournalOperation operation = mutation.TakeFristOperation();
  EXPECT_TRUE(mutation.Empty());
  EXPECT_EQ(operation.type(), JournalOperation::JOURNAL_DELETE);
}

}  // namespace feed
