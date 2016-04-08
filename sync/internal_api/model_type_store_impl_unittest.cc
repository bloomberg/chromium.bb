// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/model_type_store_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer_v2 {

class ModelTypeStoreImplTest : public testing::Test {
 public:
  void TearDown() override {
    if (store_) {
      store_.reset();
      PumpLoop();
    }
  }

  ModelTypeStore* store() { return store_.get(); }

  void OnInitDone(ModelTypeStore::Result result,
                  std::unique_ptr<ModelTypeStore> store) {
    ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
    store_ = std::move(store);
  }

  void PumpLoop() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  void CreateStore() {
    ModelTypeStore::CreateInMemoryStoreForTest(base::Bind(
        &ModelTypeStoreImplTest::OnInitDone, base::Unretained(this)));
    PumpLoop();
  }

  void WriteTestData() {
    std::unique_ptr<ModelTypeStore::WriteBatch> write_batch =
        store()->CreateWriteBatch();
    store()->WriteData(write_batch.get(), "id1", "data1");
    store()->WriteMetadata(write_batch.get(), "id1", "metadata1");
    store()->WriteData(write_batch.get(), "id2", "data2");
    store()->WriteGlobalMetadata(write_batch.get(), "global_metadata");
    ModelTypeStore::Result result;
    store()->CommitWriteBatch(std::move(write_batch),
                              base::Bind(&CaptureResult, &result));
    PumpLoop();
    ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  }

  // Following functions capture parameters passed to callbacks into variables
  // provided by test. They can be passed as callbacks to ModelTypeStore
  // functions.
  static void CaptureResult(ModelTypeStore::Result* dst,
                            ModelTypeStore::Result result) {
    *dst = result;
  }

  static void CaptureResultWithRecords(
      ModelTypeStore::Result* dst_result,
      std::unique_ptr<ModelTypeStore::RecordList>* dst_records,
      ModelTypeStore::Result result,
      std::unique_ptr<ModelTypeStore::RecordList> records) {
    *dst_result = result;
    *dst_records = std::move(records);
  }

  static void CaptureResutRecordsAndString(
      ModelTypeStore::Result* dst_result,
      std::unique_ptr<ModelTypeStore::RecordList>* dst_records,
      std::string* dst_value,
      ModelTypeStore::Result result,
      std::unique_ptr<ModelTypeStore::RecordList> records,
      const std::string& value) {
    *dst_result = result;
    *dst_records = std::move(records);
    *dst_value = value;
  }

  static void CaptureResutRecordsAndIdList(
      ModelTypeStore::Result* dst_result,
      std::unique_ptr<ModelTypeStore::RecordList>* dst_records,
      std::unique_ptr<ModelTypeStore::IdList>* dst_id_list,
      ModelTypeStore::Result result,
      std::unique_ptr<ModelTypeStore::RecordList> records,
      std::unique_ptr<ModelTypeStore::IdList> missing_id_list) {
    *dst_result = result;
    *dst_records = std::move(records);
    *dst_id_list = std::move(missing_id_list);
  }

 private:
  base::MessageLoop message_loop_;
  std::unique_ptr<ModelTypeStore> store_;
};

// Matcher to verify contents of returned RecordList .
MATCHER_P2(RecordMatches, id, value, "") {
  return arg.id == id && arg.value == value;
}

// Test that CreateInMemoryStoreForTest triggers store initialization that
// results with callback being called with valid store pointer. Test that
// resulting store is empty functional one.
TEST_F(ModelTypeStoreImplTest, CreateInMemoryStore) {
  ModelTypeStore::CreateInMemoryStoreForTest(
      base::Bind(&ModelTypeStoreImplTest::OnInitDone, base::Unretained(this)));
  ASSERT_EQ(nullptr, store());
  PumpLoop();
  ASSERT_NE(nullptr, store());

  ModelTypeStore::Result result;
  std::unique_ptr<ModelTypeStore::RecordList> records;
  store()->ReadAllData(
      base::Bind(&CaptureResultWithRecords, &result, &records));
  PumpLoop();
  ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  ASSERT_TRUE(records->empty());

  std::string global_metadata;
  store()->ReadAllMetadata(base::Bind(&CaptureResutRecordsAndString, &result,
                                      &records, &global_metadata));
  PumpLoop();
  ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  ASSERT_TRUE(records->empty());
  ASSERT_TRUE(global_metadata.empty());
}

// Test that records that are written to store later can be read from it.
TEST_F(ModelTypeStoreImplTest, WriteThenRead) {
  CreateStore();
  WriteTestData();

  ModelTypeStore::Result result;
  std::unique_ptr<ModelTypeStore::RecordList> records;
  store()->ReadAllData(
      base::Bind(&CaptureResultWithRecords, &result, &records));
  PumpLoop();
  ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  ASSERT_THAT(*records,
              testing::UnorderedElementsAre(RecordMatches("id1", "data1"),
                                            RecordMatches("id2", "data2")));

  std::string global_metadata;
  store()->ReadAllMetadata(base::Bind(&CaptureResutRecordsAndString, &result,
                                      &records, &global_metadata));
  PumpLoop();
  ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  ASSERT_THAT(*records,
              testing::UnorderedElementsAre(RecordMatches("id1", "metadata1")));
  ASSERT_EQ("global_metadata", global_metadata);
}

// Test that if global metadata is not set then ReadAllMetadata still succeeds
// and returns entry metadata records.
TEST_F(ModelTypeStoreImplTest, MissingGlobalMetadata) {
  CreateStore();
  WriteTestData();

  ModelTypeStore::Result result;

  std::unique_ptr<ModelTypeStore::WriteBatch> write_batch =
      store()->CreateWriteBatch();
  store()->DeleteGlobalMetadata(write_batch.get());
  store()->CommitWriteBatch(std::move(write_batch),
                            base::Bind(&CaptureResult, &result));
  PumpLoop();
  ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);

  std::unique_ptr<ModelTypeStore::RecordList> records;
  std::string global_metadata;
  store()->ReadAllMetadata(base::Bind(&CaptureResutRecordsAndString, &result,
                                      &records, &global_metadata));
  PumpLoop();
  ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  ASSERT_THAT(*records,
              testing::UnorderedElementsAre(RecordMatches("id1", "metadata1")));
  ASSERT_EQ(std::string(), global_metadata);
}

// Test that when reading data records by id, if one of the ids is missing
// operation still succeeds and missing id is returned in missing_id_list.
TEST_F(ModelTypeStoreImplTest, ReadMissingDataRecords) {
  CreateStore();
  WriteTestData();

  ModelTypeStore::Result result;

  ModelTypeStore::IdList id_list;
  id_list.push_back("id1");
  id_list.push_back("id3");

  std::unique_ptr<ModelTypeStore::RecordList> records;
  std::unique_ptr<ModelTypeStore::IdList> missing_id_list;

  store()->ReadData(id_list, base::Bind(&CaptureResutRecordsAndIdList, &result,
                                        &records, &missing_id_list));
  PumpLoop();
  ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  ASSERT_THAT(*records,
              testing::UnorderedElementsAre(RecordMatches("id1", "data1")));
  ASSERT_THAT(*missing_id_list, testing::UnorderedElementsAre("id3"));
}

}  // namespace syncer_v2
