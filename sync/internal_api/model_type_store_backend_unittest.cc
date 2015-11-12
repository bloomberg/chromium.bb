// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/model_type_store_backend.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace syncer_v2 {

class ModelTypeStoreBackendTest : public testing::Test {
 public:
  void SetUp() override {
    in_memory_env_ = ModelTypeStoreBackend::CreateInMemoryEnv();
  }

  void TearDown() override { in_memory_env_.reset(); }

  scoped_ptr<ModelTypeStoreBackend> CreateBackend() {
    scoped_ptr<ModelTypeStoreBackend> backend(new ModelTypeStoreBackend());
    std::string path;
    in_memory_env_->GetTestDirectory(&path);
    path += "/test_db";
    ModelTypeStore::Result result = backend->Init(path, in_memory_env_.get());
    EXPECT_EQ(ModelTypeStore::Result::SUCCESS, result);
    return backend.Pass();
  }

 protected:
  scoped_ptr<leveldb::Env> in_memory_env_;
};

// Test that after record is written to backend it can be read back even after
// backend is destroyed and recreated in the same environment.
TEST_F(ModelTypeStoreBackendTest, WriteThenRead) {
  scoped_ptr<ModelTypeStoreBackend> backend = CreateBackend();

  // Write record.
  scoped_ptr<leveldb::WriteBatch> write_batch(new leveldb::WriteBatch());
  write_batch->Put("prefix:id1", "data1");
  ModelTypeStore::Result result =
      backend->WriteModifications(write_batch.Pass());
  ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);

  // Read all records with prefix.
  ModelTypeStore::RecordList record_list;
  result = backend->ReadAllRecordsWithPrefix("prefix:", &record_list);
  ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  ASSERT_EQ(1ul, record_list.size());
  ASSERT_EQ("id1", record_list[0].id);
  ASSERT_EQ("data1", record_list[0].value);
  record_list.clear();

  // Recreate backend and read all records with prefix.
  backend = CreateBackend();
  result = backend->ReadAllRecordsWithPrefix("prefix:", &record_list);
  ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  ASSERT_EQ(1ul, record_list.size());
  ASSERT_EQ("id1", record_list[0].id);
  ASSERT_EQ("data1", record_list[0].value);
}

// Test that ReadAllRecordsWithPrefix correclty filters records by prefix.
TEST_F(ModelTypeStoreBackendTest, ReadAllRecordsWithPrefix) {
  scoped_ptr<ModelTypeStoreBackend> backend = CreateBackend();

  scoped_ptr<leveldb::WriteBatch> write_batch(new leveldb::WriteBatch());
  write_batch->Put("prefix1:id1", "data1");
  write_batch->Put("prefix2:id2", "data2");
  ModelTypeStore::Result result =
      backend->WriteModifications(write_batch.Pass());
  ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);

  ModelTypeStore::RecordList record_list;
  result = backend->ReadAllRecordsWithPrefix("prefix1:", &record_list);
  ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  ASSERT_EQ(1UL, record_list.size());
  ASSERT_EQ("id1", record_list[0].id);
  ASSERT_EQ("data1", record_list[0].value);
}

// Test that deleted records are correctly marked as milling in results of
// ReadRecordsWithPrefix.
TEST_F(ModelTypeStoreBackendTest, ReadDeletedRecord) {
  scoped_ptr<ModelTypeStoreBackend> backend = CreateBackend();

  // Create records, ensure they are returned by ReadRecordsWithPrefix.
  scoped_ptr<leveldb::WriteBatch> write_batch(new leveldb::WriteBatch());
  write_batch->Put("prefix:id1", "data1");
  write_batch->Put("prefix:id2", "data2");
  ModelTypeStore::Result result =
      backend->WriteModifications(write_batch.Pass());
  ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);

  ModelTypeStore::IdList id_list;
  ModelTypeStore::IdList missing_id_list;
  ModelTypeStore::RecordList record_list;
  id_list.push_back("id1");
  id_list.push_back("id2");
  result = backend->ReadRecordsWithPrefix("prefix:", id_list, &record_list,
                                          &missing_id_list);
  ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  ASSERT_EQ(2UL, record_list.size());
  ASSERT_TRUE(missing_id_list.empty());

  // Delete one record.
  write_batch.reset(new leveldb::WriteBatch());
  write_batch->Delete("prefix:id2");
  result = backend->WriteModifications(write_batch.Pass());
  ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);

  // Ensure deleted record id is returned in missing_id_list.
  record_list.clear();
  missing_id_list.clear();
  result = backend->ReadRecordsWithPrefix("prefix:", id_list, &record_list,
                                          &missing_id_list);
  ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  ASSERT_EQ(1UL, record_list.size());
  ASSERT_EQ("id1", record_list[0].id);
  ASSERT_EQ(1UL, missing_id_list.size());
  ASSERT_EQ("id2", missing_id_list[0]);
}

}  // namespace syncer_v2
