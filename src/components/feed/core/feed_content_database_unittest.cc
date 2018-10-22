// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_content_database.h"

#include <map>

#include "base/test/scoped_task_environment.h"
#include "components/feed/core/feed_content_mutation.h"
#include "components/feed/core/proto/content_storage.pb.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb_proto::test::FakeDB;
using testing::_;

namespace feed {

namespace {

const char kContentKeyPrefix[] = "ContentKey";
const char kContentKey1[] = "ContentKey1";
const char kContentKey2[] = "ContentKey2";
const char kContentKey3[] = "ContentKey3";
const char kContentData1[] = "Content Data1";
const char kContentData2[] = "Content Data2";

}  // namespace

class FeedContentDatabaseTest : public testing::Test {
 public:
  FeedContentDatabaseTest() : content_db_(nullptr) {}

  void CreateDatabase(bool init_database) {
    // The FakeDBs are owned by |feed_db_|, so clear our pointers before
    // resetting |feed_db_| itself.
    content_db_ = nullptr;
    // Explicitly destroy any existing database before creating a new one.
    feed_db_.reset();

    auto storage_db =
        std::make_unique<FakeDB<ContentStorageProto>>(&content_db_storage_);

    content_db_ = storage_db.get();
    feed_db_ = std::make_unique<FeedContentDatabase>(base::FilePath(),
                                                     std::move(storage_db));
    if (init_database) {
      content_db_->InitCallback(true);
      ASSERT_TRUE(db()->IsInitialized());
    }
  }

  void InjectContentStorageProto(const std::string& key,
                                 const std::string& data) {
    ContentStorageProto storage_proto;
    storage_proto.set_key(key);
    storage_proto.set_content_data(data);
    content_db_storage_[key] = storage_proto;
  }

  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

  FakeDB<ContentStorageProto>* storage_db() { return content_db_; }

  FeedContentDatabase* db() { return feed_db_.get(); }

  MOCK_METHOD1(OnContentEntriesReceived,
               void(std::vector<std::pair<std::string, std::string>>));
  MOCK_METHOD1(OnContentKeyReceived, void(std::vector<std::string>));
  MOCK_METHOD1(OnStorageCommitted, void(bool));

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::map<std::string, ContentStorageProto> content_db_storage_;

  // Owned by |feed_db_|.
  FakeDB<ContentStorageProto>* content_db_;

  std::unique_ptr<FeedContentDatabase> feed_db_;

  DISALLOW_COPY_AND_ASSIGN(FeedContentDatabaseTest);
};

TEST_F(FeedContentDatabaseTest, Init) {
  ASSERT_FALSE(db());

  CreateDatabase(/*init_database=*/false);

  storage_db()->InitCallback(true);
  EXPECT_TRUE(db()->IsInitialized());
}

TEST_F(FeedContentDatabaseTest, LoadContentAfterInitSuccess) {
  CreateDatabase(/*init_database=*/true);

  EXPECT_CALL(*this, OnContentEntriesReceived(_));
  db()->LoadContent(
      {kContentKey1},
      base::BindOnce(&FeedContentDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  storage_db()->LoadCallback(true);
}

TEST_F(FeedContentDatabaseTest, LoadContentsEntries) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1| and |kContentKey2|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);

  // Try to Load |kContentKey2| and |kContentKey3|, only |kContentKey2| should
  // return.
  EXPECT_CALL(*this, OnContentEntriesReceived(_))
      .WillOnce([](std::vector<std::pair<std::string, std::string>> results) {
        ASSERT_EQ(results.size(), 1U);
        EXPECT_EQ(results[0].first, kContentKey2);
        EXPECT_EQ(results[0].second, kContentData2);
      });
  db()->LoadContent(
      {kContentKey2, kContentKey3},
      base::BindOnce(&FeedContentDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  storage_db()->LoadCallback(true);
}

TEST_F(FeedContentDatabaseTest, LoadContentsEntriesByPrefix) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1|, |kContentKey2|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);

  // Try to Load "ContentKey", both |kContentKey1| and |kContentKey2| should
  // return.
  EXPECT_CALL(*this, OnContentEntriesReceived(_))
      .WillOnce([](std::vector<std::pair<std::string, std::string>> results) {
        ASSERT_EQ(results.size(), 2U);
        EXPECT_EQ(results[0].first, kContentKey1);
        EXPECT_EQ(results[0].second, kContentData1);
        EXPECT_EQ(results[1].first, kContentKey2);
        EXPECT_EQ(results[1].second, kContentData2);
      });
  db()->LoadContentByPrefix(
      kContentKeyPrefix,
      base::BindOnce(&FeedContentDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  storage_db()->LoadCallback(true);
}

TEST_F(FeedContentDatabaseTest, LoadAllContentKeys) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1|, |kContentKey2|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);

  EXPECT_CALL(*this, OnContentKeyReceived(_))
      .WillOnce([](std::vector<std::string> results) {
        ASSERT_EQ(results.size(), 2U);
        EXPECT_EQ(results[0], kContentKey1);
        EXPECT_EQ(results[1], kContentKey2);
      });
  db()->LoadAllContentKeys(base::BindOnce(
      &FeedContentDatabaseTest::OnContentKeyReceived, base::Unretained(this)));
  storage_db()->LoadKeysCallback(true);
}

TEST_F(FeedContentDatabaseTest, SaveContent) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1|, |kContentKey2|.
  std::unique_ptr<ContentMutation> content_mutation =
      std::make_unique<ContentMutation>();
  content_mutation->AppendUpsertOperation(kContentKey1, kContentData1);
  content_mutation->AppendUpsertOperation(kContentKey2, kContentData2);
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->CommitContentMutation(
      std::move(content_mutation),
      base::BindOnce(&FeedContentDatabaseTest::OnStorageCommitted,
                     base::Unretained(this)));
  storage_db()->UpdateCallback(true);
  storage_db()->UpdateCallback(true);

  // Make sure they're there.
  EXPECT_CALL(*this, OnContentEntriesReceived(_))
      .WillOnce([](std::vector<std::pair<std::string, std::string>> results) {
        ASSERT_EQ(results.size(), 2U);
        EXPECT_EQ(results[0].first, kContentKey1);
        EXPECT_EQ(results[0].second, kContentData1);
        EXPECT_EQ(results[1].first, kContentKey2);
        EXPECT_EQ(results[1].second, kContentData2);
      });
  db()->LoadContent(
      {kContentKey1, kContentKey2},
      base::BindOnce(&FeedContentDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  storage_db()->LoadCallback(true);
}

TEST_F(FeedContentDatabaseTest, DeleteContent) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1| and |kContentKey2|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);

  // Delete |kContentKey2| and |kContentKey3|
  std::unique_ptr<ContentMutation> content_mutation =
      std::make_unique<ContentMutation>();
  content_mutation->AppendDeleteOperation(kContentKey2);
  content_mutation->AppendDeleteOperation(kContentKey3);
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->CommitContentMutation(
      std::move(content_mutation),
      base::BindOnce(&FeedContentDatabaseTest::OnStorageCommitted,
                     base::Unretained(this)));
  storage_db()->UpdateCallback(true);
  storage_db()->UpdateCallback(true);

  // Make sure only |kContentKey2| got deleted.
  EXPECT_CALL(*this, OnContentEntriesReceived(_))
      .WillOnce([](std::vector<std::pair<std::string, std::string>> results) {
        EXPECT_EQ(results.size(), 1U);
        EXPECT_EQ(results[0].first, kContentKey1);
        EXPECT_EQ(results[0].second, kContentData1);
      });
  db()->LoadContent(
      {kContentKey1, kContentKey2},
      base::BindOnce(&FeedContentDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  storage_db()->LoadCallback(true);
}

TEST_F(FeedContentDatabaseTest, DeleteContentByPrefix) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1| and |kContentKey2|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);

  // Delete |kContentKey1| and |kContentKey2|
  std::unique_ptr<ContentMutation> content_mutation =
      std::make_unique<ContentMutation>();
  content_mutation->AppendDeleteByPrefixOperation(kContentKeyPrefix);
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->CommitContentMutation(
      std::move(content_mutation),
      base::BindOnce(&FeedContentDatabaseTest::OnStorageCommitted,
                     base::Unretained(this)));
  storage_db()->UpdateCallback(true);

  // Make sure |kContentKey1| and |kContentKey2| got deleted.
  EXPECT_CALL(*this, OnContentEntriesReceived(_))
      .WillOnce([](std::vector<std::pair<std::string, std::string>> results) {
        EXPECT_EQ(results.size(), 0U);
      });
  db()->LoadContent(
      {kContentKey1, kContentKey2},
      base::BindOnce(&FeedContentDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  storage_db()->LoadCallback(true);
}

TEST_F(FeedContentDatabaseTest, DeleteAllContent) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1| and |kContentKey2|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);

  // Delete all content, meaning |kContentKey1| and |kContentKey2| are expected
  // to be deleted.
  std::unique_ptr<ContentMutation> content_mutation =
      std::make_unique<ContentMutation>();
  content_mutation->AppendDeleteAllOperation();
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->CommitContentMutation(
      std::move(content_mutation),
      base::BindOnce(&FeedContentDatabaseTest::OnStorageCommitted,
                     base::Unretained(this)));
  storage_db()->UpdateCallback(true);

  // Make sure |kContentKey1| and |kContentKey2| got deleted.
  EXPECT_CALL(*this, OnContentEntriesReceived(_))
      .WillOnce([](std::vector<std::pair<std::string, std::string>> results) {
        EXPECT_EQ(results.size(), 0U);
      });
  db()->LoadContent(
      {kContentKey1, kContentKey2},
      base::BindOnce(&FeedContentDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  storage_db()->LoadCallback(true);
}

TEST_F(FeedContentDatabaseTest, SaveAndDeleteContent) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1|, |kContentKey2|.
  std::unique_ptr<ContentMutation> content_mutation =
      std::make_unique<ContentMutation>();
  content_mutation->AppendUpsertOperation(kContentKey1, kContentData1);
  content_mutation->AppendUpsertOperation(kContentKey2, kContentData2);
  content_mutation->AppendDeleteOperation(kContentKey2);
  content_mutation->AppendDeleteOperation(kContentKey3);
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->CommitContentMutation(
      std::move(content_mutation),
      base::BindOnce(&FeedContentDatabaseTest::OnStorageCommitted,
                     base::Unretained(this)));
  storage_db()->UpdateCallback(true);
  storage_db()->UpdateCallback(true);
  storage_db()->UpdateCallback(true);
  storage_db()->UpdateCallback(true);

  // Make sure only |kContentKey2| got deleted.
  EXPECT_CALL(*this, OnContentEntriesReceived(_))
      .WillOnce([](std::vector<std::pair<std::string, std::string>> results) {
        EXPECT_EQ(results.size(), 1U);
        EXPECT_EQ(results[0].first, kContentKey1);
        EXPECT_EQ(results[0].second, kContentData1);
      });
  db()->LoadContent(
      {kContentKey1, kContentKey2},
      base::BindOnce(&FeedContentDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  storage_db()->LoadCallback(true);
}

}  // namespace feed
