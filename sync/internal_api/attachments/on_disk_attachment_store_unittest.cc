// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/attachment_store.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "sync/internal_api/attachments/attachment_store_test_template.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

void AttachmentStoreCreated(scoped_refptr<AttachmentStore>* store_dest,
                            AttachmentStore::Result* result_dest,
                            const AttachmentStore::Result& result,
                            const scoped_refptr<AttachmentStore>& store) {
  *result_dest = result;
  *store_dest = store;
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
    AttachmentStore::CreateOnDiskStore(
        temp_dir_.path(),
        base::ThreadTaskRunnerHandle::Get(),
        base::Bind(&AttachmentStoreCreated, &store, &result));
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
    EXPECT_EQ(result, AttachmentStore::SUCCESS);
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
      const AttachmentStore::Result& source_result,
      scoped_ptr<AttachmentMap> source_attachments,
      scoped_ptr<AttachmentIdList> source_failed_attachment_ids) {
    CopyResult(destination_result, source_result);
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
  AttachmentStore::CreateOnDiskStore(
      temp_dir_.path(),
      base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &store_, &result));
  RunLoop();
  EXPECT_EQ(result, AttachmentStore::SUCCESS);

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
  EXPECT_EQ(result, AttachmentStore::SUCCESS);

  // Close attachment store.
  store_ = nullptr;
  result = AttachmentStore::UNSPECIFIED_ERROR;
  AttachmentStore::CreateOnDiskStore(
      temp_dir_.path(),
      base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &store_, &result));
  RunLoop();
  EXPECT_EQ(result, AttachmentStore::SUCCESS);

  result = AttachmentStore::UNSPECIFIED_ERROR;
  AttachmentIdList attachment_ids;
  attachment_ids.push_back(attachment.GetId());
  store_->Read(
      attachment_ids,
      base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResultAttachments,
                 base::Unretained(this),
                 &result));
  RunLoop();
  EXPECT_EQ(result, AttachmentStore::SUCCESS);
}

// Ensure loading corrupt attachment store fails.
TEST_F(OnDiskAttachmentStoreSpecificTest, FailToOpen) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath db_path =
      temp_dir_.path().Append(FILE_PATH_LITERAL("leveldb"));
  base::CreateDirectory(db_path);

  // To simulate corrupt database write garbage to CURRENT file.
  std::string current_file_content = "abra.cadabra";
  base::WriteFile(db_path.Append(FILE_PATH_LITERAL("CURRENT")),
                  current_file_content.c_str(),
                  current_file_content.size());

  AttachmentStore::Result result = AttachmentStore::SUCCESS;
  AttachmentStore::CreateOnDiskStore(
      temp_dir_.path(),
      base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &store_, &result));
  RunLoop();
  EXPECT_EQ(result, AttachmentStore::UNSPECIFIED_ERROR);
  EXPECT_EQ(store_.get(), nullptr);
}

}  // namespace syncer
