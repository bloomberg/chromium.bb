// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/on_disk_attachment_store.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "sync/internal_api/attachments/attachment_store_test_template.h"
#include "sync/internal_api/attachments/proto/attachment_store.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace syncer {

namespace {

void AttachmentStoreCreated(AttachmentStore::Result* result_dest,
                            const AttachmentStore::Result& result) {
  *result_dest = result;
}

}  // namespace

// Instantiation of common attachment store tests.
class OnDiskAttachmentStoreFactory {
 public:
  OnDiskAttachmentStoreFactory() {}
  ~OnDiskAttachmentStoreFactory() {}

  scoped_refptr<AttachmentStore> CreateAttachmentStore() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    scoped_refptr<AttachmentStore> store;
    AttachmentStore::Result result = AttachmentStore::UNSPECIFIED_ERROR;
    store = AttachmentStore::CreateOnDiskStore(
        temp_dir_.path(), base::ThreadTaskRunnerHandle::Get(),
        base::Bind(&AttachmentStoreCreated, &result));
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
    EXPECT_EQ(AttachmentStore::SUCCESS, result);
    return store;
  }

 private:
  base::ScopedTempDir temp_dir_;
};

INSTANTIATE_TYPED_TEST_CASE_P(OnDisk,
                              AttachmentStoreTest,
                              OnDiskAttachmentStoreFactory);

// Tests specific to OnDiskAttachmentStore.
class OnDiskAttachmentStoreSpecificTest : public testing::Test {
 public:
  base::ScopedTempDir temp_dir_;
  base::MessageLoop message_loop_;
  scoped_refptr<AttachmentStore> store_;

  OnDiskAttachmentStoreSpecificTest() {}

  void CopyResult(AttachmentStore::Result* destination_result,
                  const AttachmentStore::Result& source_result) {
    *destination_result = source_result;
  }

  void CopyResultAttachments(
      AttachmentStore::Result* destination_result,
      AttachmentIdList* destination_failed_attachment_ids,
      const AttachmentStore::Result& source_result,
      scoped_ptr<AttachmentMap> source_attachments,
      scoped_ptr<AttachmentIdList> source_failed_attachment_ids) {
    CopyResult(destination_result, source_result);
    *destination_failed_attachment_ids = *source_failed_attachment_ids;
  }

  scoped_ptr<leveldb::DB> OpenLevelDB(const base::FilePath& path) {
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status s = leveldb::DB::Open(options, path.AsUTF8Unsafe(), &db);
    EXPECT_TRUE(s.ok());
    return make_scoped_ptr(db);
  }

  void UpdateStoreMetadataRecord(const base::FilePath& path,
                                 const std::string& content) {
    scoped_ptr<leveldb::DB> db = OpenLevelDB(path);
    leveldb::Status s =
        db->Put(leveldb::WriteOptions(), "database-metadata", content);
    EXPECT_TRUE(s.ok());
  }

  std::string ReadStoreMetadataRecord(const base::FilePath& path) {
    scoped_ptr<leveldb::DB> db = OpenLevelDB(path);
    std::string content;
    leveldb::Status s =
        db->Get(leveldb::ReadOptions(), "database-metadata", &content);
    EXPECT_TRUE(s.ok());
    return content;
  }

  void VerifyAttachmentRecordsPresent(const base::FilePath& path,
                                      const AttachmentId& attachment_id,
                                      bool expect_records_present) {
    scoped_ptr<leveldb::DB> db = OpenLevelDB(path);

    std::string metadata_key =
        OnDiskAttachmentStore::MakeMetadataKeyFromAttachmentId(attachment_id);
    std::string data_key =
        OnDiskAttachmentStore::MakeDataKeyFromAttachmentId(attachment_id);
    std::string data;
    leveldb::Status s = db->Get(leveldb::ReadOptions(), data_key, &data);
    if (expect_records_present)
      EXPECT_TRUE(s.ok());
    else
      EXPECT_TRUE(s.IsNotFound());
    s = db->Get(leveldb::ReadOptions(), metadata_key, &data);
    if (expect_records_present)
      EXPECT_TRUE(s.ok());
    else
      EXPECT_TRUE(s.IsNotFound());
  }

  void RunLoop() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
};

// Ensure that store can be closed and reopen while retaining stored
// attachments.
TEST_F(OnDiskAttachmentStoreSpecificTest, CloseAndReopen) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  AttachmentStore::Result result;

  result = AttachmentStore::UNSPECIFIED_ERROR;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.path(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &result));
  RunLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, result);

  result = AttachmentStore::UNSPECIFIED_ERROR;
  std::string some_data = "data";
  Attachment attachment =
      Attachment::Create(base::RefCountedString::TakeString(&some_data));
  AttachmentList attachments;
  attachments.push_back(attachment);
  store_->Write(attachments,
                base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResult,
                           base::Unretained(this),
                           &result));
  RunLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, result);

  // Close and reopen attachment store.
  store_ = nullptr;
  result = AttachmentStore::UNSPECIFIED_ERROR;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.path(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &result));
  RunLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, result);

  result = AttachmentStore::UNSPECIFIED_ERROR;
  AttachmentIdList attachment_ids;
  attachment_ids.push_back(attachment.GetId());
  AttachmentIdList failed_attachment_ids;
  store_->Read(
      attachment_ids,
      base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResultAttachments,
                 base::Unretained(this), &result, &failed_attachment_ids));
  RunLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, result);
  EXPECT_TRUE(failed_attachment_ids.empty());
}

// Ensure loading corrupt attachment store fails.
TEST_F(OnDiskAttachmentStoreSpecificTest, FailToOpen) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath db_path =
      temp_dir_.path().Append(FILE_PATH_LITERAL("leveldb"));
  base::CreateDirectory(db_path);

  // To simulate corrupt database write empty CURRENT file.
  std::string current_file_content = "";
  base::WriteFile(db_path.Append(FILE_PATH_LITERAL("CURRENT")),
                  current_file_content.c_str(),
                  current_file_content.size());

  AttachmentStore::Result result = AttachmentStore::SUCCESS;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.path(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &result));
  RunLoop();
  EXPECT_EQ(AttachmentStore::UNSPECIFIED_ERROR, result);
}

// Ensure that attachment store works correctly when store metadata is missing,
// corrupt or has unknown schema version.
TEST_F(OnDiskAttachmentStoreSpecificTest, StoreMetadata) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath db_path =
      temp_dir_.path().Append(FILE_PATH_LITERAL("leveldb"));
  base::CreateDirectory(db_path);

  // Create and close empty database.
  OpenLevelDB(db_path);
  // Open database with AttachmentStore.
  AttachmentStore::Result result = AttachmentStore::UNSPECIFIED_ERROR;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.path(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &result));
  RunLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, result);
  // Close AttachmentStore so that test can check content.
  store_ = nullptr;
  RunLoop();

  // AttachmentStore should create metadata record.
  std::string data = ReadStoreMetadataRecord(db_path);
  attachment_store_pb::StoreMetadata metadata;
  EXPECT_TRUE(metadata.ParseFromString(data));
  EXPECT_EQ(1, metadata.schema_version());

  // Set unknown future schema version.
  metadata.set_schema_version(2);
  data = metadata.SerializeAsString();
  UpdateStoreMetadataRecord(db_path, data);

  // AttachmentStore should fail to load.
  result = AttachmentStore::SUCCESS;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.path(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &result));
  RunLoop();
  EXPECT_EQ(AttachmentStore::UNSPECIFIED_ERROR, result);

  // Write garbage into metadata record.
  UpdateStoreMetadataRecord(db_path, "abra.cadabra");

  // AttachmentStore should fail to load.
  result = AttachmentStore::SUCCESS;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.path(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &result));
  RunLoop();
  EXPECT_EQ(AttachmentStore::UNSPECIFIED_ERROR, result);
}

// Ensure that attachment store correctly maintains metadata records for
// attachments.
TEST_F(OnDiskAttachmentStoreSpecificTest, RecordMetadata) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath db_path =
      temp_dir_.path().Append(FILE_PATH_LITERAL("leveldb"));

  // Create attachment store.
  AttachmentStore::Result create_result = AttachmentStore::UNSPECIFIED_ERROR;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.path(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &create_result));

  // Write two attachments.
  AttachmentStore::Result write_result = AttachmentStore::UNSPECIFIED_ERROR;
  std::string some_data;
  AttachmentList attachments;
  some_data = "data1";
  attachments.push_back(
      Attachment::Create(base::RefCountedString::TakeString(&some_data)));
  some_data = "data2";
  attachments.push_back(
      Attachment::Create(base::RefCountedString::TakeString(&some_data)));
  store_->Write(attachments,
                base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResult,
                           base::Unretained(this), &write_result));

  // Delete one of written attachments.
  AttachmentStore::Result drop_result = AttachmentStore::UNSPECIFIED_ERROR;
  AttachmentIdList attachment_ids;
  attachment_ids.push_back(attachments[0].GetId());
  store_->Drop(attachment_ids,
               base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResult,
                          base::Unretained(this), &drop_result));
  store_ = nullptr;
  RunLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, create_result);
  EXPECT_EQ(AttachmentStore::SUCCESS, write_result);
  EXPECT_EQ(AttachmentStore::SUCCESS, drop_result);

  // Verify that attachment store contains only records for second attachment.
  VerifyAttachmentRecordsPresent(db_path, attachments[0].GetId(), false);
  VerifyAttachmentRecordsPresent(db_path, attachments[1].GetId(), true);
}

// Ensure that attachment store fails to load attachment with mismatched crc.
TEST_F(OnDiskAttachmentStoreSpecificTest, MismatchedCrc) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  // Create attachment store.
  AttachmentStore::Result create_result = AttachmentStore::UNSPECIFIED_ERROR;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.path(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &create_result));

  // Write attachment with incorrect crc32c.
  AttachmentStore::Result write_result = AttachmentStore::UNSPECIFIED_ERROR;
  const uint32_t intentionally_wrong_crc32c = 0;
  std::string some_data("data1");
  Attachment attachment = Attachment::CreateFromParts(
      AttachmentId::Create(), base::RefCountedString::TakeString(&some_data),
      intentionally_wrong_crc32c);
  AttachmentList attachments;
  attachments.push_back(attachment);
  store_->Write(attachments,
                base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResult,
                           base::Unretained(this), &write_result));

  // Read attachment.
  AttachmentStore::Result read_result = AttachmentStore::UNSPECIFIED_ERROR;
  AttachmentIdList attachment_ids;
  attachment_ids.push_back(attachment.GetId());
  AttachmentIdList failed_attachment_ids;
  store_->Read(
      attachment_ids,
      base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResultAttachments,
                 base::Unretained(this), &read_result, &failed_attachment_ids));
  RunLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, create_result);
  EXPECT_EQ(AttachmentStore::SUCCESS, write_result);
  EXPECT_EQ(AttachmentStore::UNSPECIFIED_ERROR, read_result);
  EXPECT_THAT(failed_attachment_ids, testing::ElementsAre(attachment.GetId()));
}

// Ensure that after store initialization failure ReadWrite/Drop operations fail
// with correct error.
TEST_F(OnDiskAttachmentStoreSpecificTest, OpsAfterInitializationFailed) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath db_path =
      temp_dir_.path().Append(FILE_PATH_LITERAL("leveldb"));
  base::CreateDirectory(db_path);

  // To simulate corrupt database write empty CURRENT file.
  std::string current_file_content = "";
  base::WriteFile(db_path.Append(FILE_PATH_LITERAL("CURRENT")),
                  current_file_content.c_str(), current_file_content.size());

  AttachmentStore::Result create_result = AttachmentStore::SUCCESS;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.path(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &create_result));

  // Reading from uninitialized store should result in
  // STORE_INITIALIZATION_FAILED.
  AttachmentStore::Result read_result = AttachmentStore::SUCCESS;
  AttachmentIdList attachment_ids;
  attachment_ids.push_back(AttachmentId::Create());
  AttachmentIdList failed_attachment_ids;
  store_->Read(
      attachment_ids,
      base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResultAttachments,
                 base::Unretained(this), &read_result, &failed_attachment_ids));

  // Dropping from uninitialized store should result in
  // STORE_INITIALIZATION_FAILED.
  AttachmentStore::Result drop_result = AttachmentStore::SUCCESS;
  store_->Drop(attachment_ids,
               base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResult,
                          base::Unretained(this), &drop_result));

  // Writing to uninitialized store should result in
  // STORE_INITIALIZATION_FAILED.
  AttachmentStore::Result write_result = AttachmentStore::SUCCESS;
  std::string some_data;
  AttachmentList attachments;
  some_data = "data1";
  attachments.push_back(
      Attachment::Create(base::RefCountedString::TakeString(&some_data)));
  store_->Write(attachments,
                base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResult,
                           base::Unretained(this), &write_result));

  RunLoop();
  EXPECT_EQ(AttachmentStore::UNSPECIFIED_ERROR, create_result);
  EXPECT_EQ(AttachmentStore::STORE_INITIALIZATION_FAILED, read_result);
  EXPECT_THAT(failed_attachment_ids, testing::ElementsAre(attachment_ids[0]));
  EXPECT_EQ(AttachmentStore::STORE_INITIALIZATION_FAILED, drop_result);
  EXPECT_EQ(AttachmentStore::STORE_INITIALIZATION_FAILED, write_result);
}

}  // namespace syncer
