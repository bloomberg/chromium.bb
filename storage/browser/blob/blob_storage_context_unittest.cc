// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_storage_context.h"

#include <stdint.h>

#include <limits>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/blob_transport_host.h"
#include "storage/common/blob_storage/blob_item_bytes_request.h"
#include "storage/common/blob_storage/blob_item_bytes_response.h"
#include "testing/gtest/include/gtest/gtest.h"

using RequestMemoryCallback = storage::BlobTransportHost::RequestMemoryCallback;
using FileCreationInfo = storage::BlobMemoryController::FileCreationInfo;

namespace storage {
namespace {
using base::TestSimpleTaskRunner;

const int kTestDiskCacheStreamIndex = 0;

const std::string kBlobStorageDirectory = "blob_storage";
const size_t kTestBlobStorageIPCThresholdBytes = 20;
const size_t kTestBlobStorageMaxSharedMemoryBytes = 50;

const size_t kTestBlobStorageMaxBlobMemorySize = 400;
const uint64_t kTestBlobStorageMaxDiskSpace = 4000;
const uint64_t kTestBlobStorageMinFileSizeBytes = 10;
const uint64_t kTestBlobStorageMaxFileSizeBytes = 100;

// Our disk cache tests don't need a real data handle since the tests themselves
// scope the disk cache and entries.
class EmptyDataHandle : public storage::BlobDataBuilder::DataHandle {
 private:
  ~EmptyDataHandle() override {}
};

std::unique_ptr<disk_cache::Backend> CreateInMemoryDiskCache() {
  std::unique_ptr<disk_cache::Backend> cache;
  net::TestCompletionCallback callback;
  int rv = disk_cache::CreateCacheBackend(
      net::MEMORY_CACHE, net::CACHE_BACKEND_DEFAULT, base::FilePath(), 0, false,
      nullptr, &cache, callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));

  return cache;
}

disk_cache::ScopedEntryPtr CreateDiskCacheEntry(disk_cache::Backend* cache,
                                                const char* key,
                                                const std::string& data) {
  disk_cache::Entry* temp_entry = nullptr;
  net::TestCompletionCallback callback;
  int rv = cache->CreateEntry(key, &temp_entry, callback.callback());
  if (callback.GetResult(rv) != net::OK)
    return nullptr;
  disk_cache::ScopedEntryPtr entry(temp_entry);

  scoped_refptr<net::StringIOBuffer> iobuffer = new net::StringIOBuffer(data);
  rv = entry->WriteData(kTestDiskCacheStreamIndex, 0, iobuffer.get(),
                        iobuffer->size(), callback.callback(), false);
  EXPECT_EQ(static_cast<int>(data.size()), callback.GetResult(rv));
  return entry;
}

void SaveBlobStatus(BlobStatus* status_ptr, BlobStatus status) {
  *status_ptr = status;
}

void SaveBlobStatusAndFiles(BlobStatus* status_ptr,
                            std::vector<FileCreationInfo>* files_ptr,
                            BlobStatus status,
                            std::vector<FileCreationInfo> files) {
  EXPECT_FALSE(BlobStatusIsError(status));
  *status_ptr = status;
  for (FileCreationInfo& info : files) {
    files_ptr->push_back(std::move(info));
  }
}

void IncrementNumber(size_t* number, BlobStatus status) {
  EXPECT_EQ(BlobStatus::DONE, status);
  *number = *number + 1;
}

}  // namespace

class BlobStorageContextTest : public testing::Test {
 protected:
  BlobStorageContextTest() {}
  ~BlobStorageContextTest() override {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    context_ = base::MakeUnique<BlobStorageContext>();
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    file_runner_->RunPendingTasks();
    ASSERT_TRUE(temp_dir_.Delete());
  }

  std::unique_ptr<BlobDataHandle> SetupBasicBlob(const std::string& id) {
    BlobDataBuilder builder(id);
    builder.AppendData("1", 1);
    builder.set_content_type("text/plain");
    return context_->AddFinishedBlob(builder);
  }

  void SetTestMemoryLimits() {
    BlobStorageLimits limits;
    limits.max_ipc_memory_size = kTestBlobStorageIPCThresholdBytes;
    limits.max_shared_memory_size = kTestBlobStorageMaxSharedMemoryBytes;
    limits.max_blob_in_memory_space = kTestBlobStorageMaxBlobMemorySize;
    limits.desired_max_disk_space = kTestBlobStorageMaxDiskSpace;
    limits.effective_max_disk_space = kTestBlobStorageMaxDiskSpace;
    limits.min_page_file_size = kTestBlobStorageMinFileSizeBytes;
    limits.max_file_size = kTestBlobStorageMaxFileSizeBytes;
    context_->mutable_memory_controller()->set_limits_for_testing(limits);
  }

  void IncrementRefCount(const std::string& uuid) {
    context_->IncrementBlobRefCount(uuid);
  }

  void DecrementRefCount(const std::string& uuid) {
    context_->DecrementBlobRefCount(uuid);
  }

  std::vector<FileCreationInfo> files_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<TestSimpleTaskRunner> file_runner_ = new TestSimpleTaskRunner();

  base::MessageLoop fake_io_message_loop_;
  std::unique_ptr<BlobStorageContext> context_;
};

TEST_F(BlobStorageContextTest, BuildBlobAsync) {
  const std::string kId("id");
  const size_t kSize = 10u;
  BlobStatus status = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;

  BlobDataBuilder builder(kId);
  builder.AppendFutureData(kSize);
  builder.set_content_type("text/plain");
  EXPECT_EQ(0lu, context_->memory_controller().memory_usage());
  std::unique_ptr<BlobDataHandle> handle = context_->BuildBlob(
      builder, base::Bind(&SaveBlobStatusAndFiles, &status, &files_));
  EXPECT_EQ(10lu, context_->memory_controller().memory_usage());
  EXPECT_TRUE(handle->IsBeingBuilt())
      << static_cast<int>(handle->GetBlobStatus());
  EXPECT_EQ(BlobStatus::PENDING_TRANSPORT, status);

  BlobStatus construction_done = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  handle->RunOnConstructionComplete(
      base::Bind(&SaveBlobStatus, &construction_done));

  EXPECT_EQ(10u, context_->memory_controller().memory_usage());

  builder.PopulateFutureData(0, "abcdefghij", 0, 10u);
  context_->NotifyTransportComplete(kId);

  // Check we're done.
  EXPECT_EQ(BlobStatus::DONE, handle->GetBlobStatus());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BlobStatus::DONE, construction_done);

  EXPECT_EQ(builder, *handle->CreateSnapshot());

  handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0lu, context_->memory_controller().memory_usage());
}

TEST_F(BlobStorageContextTest, BuildBlobAndCancel) {
  const std::string kId("id");
  const size_t kSize = 10u;
  BlobStatus status = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;

  BlobDataBuilder builder(kId);
  builder.AppendFutureData(kSize);
  builder.set_content_type("text/plain");
  EXPECT_EQ(0lu, context_->memory_controller().memory_usage());
  std::unique_ptr<BlobDataHandle> handle = context_->BuildBlob(
      builder, base::Bind(&SaveBlobStatusAndFiles, &status, &files_));
  EXPECT_EQ(10lu, context_->memory_controller().memory_usage());
  EXPECT_TRUE(handle->IsBeingBuilt());
  EXPECT_EQ(BlobStatus::PENDING_TRANSPORT, status);
  EXPECT_EQ(10u, context_->memory_controller().memory_usage());

  BlobStatus construction_done = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  handle->RunOnConstructionComplete(
      base::Bind(&SaveBlobStatus, &construction_done));

  context_->CancelBuildingBlob(kId, BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT);
  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(0lu, context_->memory_controller().memory_usage());

  // Check we're broken.
  EXPECT_EQ(BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT, handle->GetBlobStatus());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT, construction_done);
}

TEST_F(BlobStorageContextTest, CancelledReference) {
  const std::string kId1("id1");
  const std::string kId2("id2");
  const size_t kSize = 10u;
  BlobStatus status = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;

  // Start our first blob.
  BlobDataBuilder builder(kId1);
  builder.AppendFutureData(kSize);
  builder.set_content_type("text/plain");
  EXPECT_EQ(0lu, context_->memory_controller().memory_usage());
  std::unique_ptr<BlobDataHandle> handle = context_->BuildBlob(
      builder, base::Bind(&SaveBlobStatusAndFiles, &status, &files_));
  EXPECT_EQ(10lu, context_->memory_controller().memory_usage());
  EXPECT_TRUE(handle->IsBeingBuilt());
  EXPECT_EQ(BlobStatus::PENDING_TRANSPORT, status);

  BlobStatus construction_done = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  handle->RunOnConstructionComplete(
      base::Bind(&SaveBlobStatus, &construction_done));

  EXPECT_EQ(10u, context_->memory_controller().memory_usage());

  // Create our second blob, which depends on the first.
  BlobDataBuilder builder2(kId2);
  builder2.AppendBlob(kId1);
  builder2.set_content_type("text/plain");
  std::unique_ptr<BlobDataHandle> handle2 = context_->BuildBlob(
      builder2, BlobStorageContext::TransportAllowedCallback());
  BlobStatus construction_done2 =
      BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  handle->RunOnConstructionComplete(
      base::Bind(&SaveBlobStatus, &construction_done2));
  EXPECT_TRUE(handle2->IsBeingBuilt());

  EXPECT_EQ(10lu, context_->memory_controller().memory_usage());

  // Cancel the first blob.
  context_->CancelBuildingBlob(kId1, BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT);

  base::RunLoop().RunUntilIdle();
  // Check we broke successfully.
  EXPECT_EQ(BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT, construction_done);
  EXPECT_EQ(BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT, handle->GetBlobStatus());
  EXPECT_EQ(0lu, context_->memory_controller().memory_usage());
  EXPECT_TRUE(handle->IsBroken());

  // Check that it propagated.
  EXPECT_TRUE(handle2->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT, construction_done2);
  EXPECT_EQ(BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT, handle->GetBlobStatus());
}

TEST_F(BlobStorageContextTest, IncorrectSlice) {
  const std::string kId1("id1");
  const std::string kId2("id2");

  std::unique_ptr<BlobDataHandle> handle = SetupBasicBlob(kId1);

  EXPECT_EQ(1lu, context_->memory_controller().memory_usage());

  BlobDataBuilder builder(kId2);
  builder.AppendBlob(kId1, 1, 10);
  std::unique_ptr<BlobDataHandle> handle2 = context_->BuildBlob(
      builder, BlobStorageContext::TransportAllowedCallback());

  EXPECT_TRUE(handle2->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
            handle2->GetBlobStatus());
}

TEST_F(BlobStorageContextTest, IncrementDecrementRef) {
  // Build up a basic blob.
  const std::string kId("id");
  std::unique_ptr<BlobDataHandle> blob_data_handle = SetupBasicBlob(kId);

  // Do an extra increment to keep it around after we kill the handle.
  IncrementRefCount(kId);
  IncrementRefCount(kId);
  DecrementRefCount(kId);
  blob_data_handle = context_->GetBlobDataFromUUID(kId);
  EXPECT_TRUE(blob_data_handle);
  blob_data_handle.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(context_->registry().HasEntry(kId));
  DecrementRefCount(kId);
  EXPECT_FALSE(context_->registry().HasEntry(kId));

  // Make sure it goes away in the end.
  blob_data_handle = context_->GetBlobDataFromUUID(kId);
  EXPECT_FALSE(blob_data_handle);
}

TEST_F(BlobStorageContextTest, BlobDataHandle) {
  // Build up a basic blob.
  const std::string kId("id");
  std::unique_ptr<BlobDataHandle> blob_data_handle = SetupBasicBlob(kId);
  EXPECT_TRUE(blob_data_handle);

  // Get another handle
  std::unique_ptr<BlobDataHandle> another_handle =
      context_->GetBlobDataFromUUID(kId);
  EXPECT_TRUE(another_handle);

  // Should disappear after dropping both handles.
  blob_data_handle.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(context_->registry().HasEntry(kId));

  another_handle.reset();
  base::RunLoop().RunUntilIdle();

  blob_data_handle = context_->GetBlobDataFromUUID(kId);
  EXPECT_FALSE(blob_data_handle);
}

TEST_F(BlobStorageContextTest, MemoryUsage) {
  const std::string kId1("id1");
  const std::string kId2("id2");

  BlobDataBuilder builder1(kId1);
  BlobDataBuilder builder2(kId2);
  builder1.AppendData("Data1Data2");
  builder2.AppendBlob(kId1);
  builder2.AppendBlob(kId1);
  builder2.AppendBlob(kId1);
  builder2.AppendBlob(kId1);
  builder2.AppendBlob(kId1);
  builder2.AppendBlob(kId1);
  builder2.AppendBlob(kId1);

  EXPECT_EQ(0lu, context_->memory_controller().memory_usage());

  std::unique_ptr<BlobDataHandle> blob_data_handle =
      context_->AddFinishedBlob(&builder1);
  EXPECT_EQ(10lu, context_->memory_controller().memory_usage());
  std::unique_ptr<BlobDataHandle> blob_data_handle2 =
      context_->AddFinishedBlob(&builder2);
  EXPECT_EQ(10lu, context_->memory_controller().memory_usage());

  EXPECT_EQ(2u, context_->registry().blob_count());

  blob_data_handle.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(10lu, context_->memory_controller().memory_usage());
  EXPECT_EQ(1u, context_->registry().blob_count());
  blob_data_handle2.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0lu, context_->memory_controller().memory_usage());
  EXPECT_EQ(0u, context_->registry().blob_count());
}

TEST_F(BlobStorageContextTest, AddFinishedBlob) {
  const std::string kId1("id1");
  const std::string kId2("id12");
  const std::string kId3("id3");

  BlobDataBuilder builder1(kId1);
  BlobDataBuilder builder2(kId2);
  BlobDataBuilder canonicalized_blob_data2(kId2);
  builder1.AppendData("Data1Data2");
  builder2.AppendBlob(kId1, 5, 5);
  builder2.AppendData(" is the best");
  canonicalized_blob_data2.AppendData("Data2");
  canonicalized_blob_data2.AppendData(" is the best");

  BlobStorageContext context;

  std::unique_ptr<BlobDataHandle> blob_data_handle =
      context_->AddFinishedBlob(&builder1);
  std::unique_ptr<BlobDataHandle> blob_data_handle2 =
      context_->AddFinishedBlob(&builder2);

  EXPECT_EQ(10u + 12u + 5u, context_->memory_controller().memory_usage());

  ASSERT_TRUE(blob_data_handle);
  ASSERT_TRUE(blob_data_handle2);
  std::unique_ptr<BlobDataSnapshot> data1 = blob_data_handle->CreateSnapshot();
  std::unique_ptr<BlobDataSnapshot> data2 = blob_data_handle2->CreateSnapshot();
  EXPECT_EQ(*data1, builder1);
  EXPECT_EQ(*data2, canonicalized_blob_data2);
  blob_data_handle.reset();
  data2.reset();

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(12u + 5u, context_->memory_controller().memory_usage());

  blob_data_handle = context_->GetBlobDataFromUUID(kId1);
  EXPECT_FALSE(blob_data_handle);
  EXPECT_TRUE(blob_data_handle2);
  data2 = blob_data_handle2->CreateSnapshot();
  EXPECT_EQ(*data2, canonicalized_blob_data2);

  // Test shared elements stick around.
  BlobDataBuilder builder3(kId3);
  builder3.AppendBlob(kId2);
  builder3.AppendBlob(kId2);
  std::unique_ptr<BlobDataHandle> blob_data_handle3 =
      context_->AddFinishedBlob(&builder3);
  EXPECT_FALSE(blob_data_handle3->IsBeingBuilt());
  blob_data_handle2.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(12u + 5u, context_->memory_controller().memory_usage());

  blob_data_handle2 = context_->GetBlobDataFromUUID(kId2);
  EXPECT_FALSE(blob_data_handle2);
  EXPECT_TRUE(blob_data_handle3);
  std::unique_ptr<BlobDataSnapshot> data3 = blob_data_handle3->CreateSnapshot();

  BlobDataBuilder canonicalized_blob_data3(kId3);
  canonicalized_blob_data3.AppendData("Data2");
  canonicalized_blob_data3.AppendData(" is the best");
  canonicalized_blob_data3.AppendData("Data2");
  canonicalized_blob_data3.AppendData(" is the best");
  EXPECT_EQ(*data3, canonicalized_blob_data3);

  blob_data_handle.reset();
  blob_data_handle2.reset();
  blob_data_handle3.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(BlobStorageContextTest, AddFinishedBlob_LargeOffset) {
  // A value which does not fit in a 4-byte data type. Used to confirm that
  // large values are supported on 32-bit Chromium builds. Regression test for:
  // crbug.com/458122.
  const uint64_t kLargeSize = std::numeric_limits<uint64_t>::max() - 1;

  const uint64_t kBlobLength = 5;
  const std::string kId1("id1");
  const std::string kId2("id2");

  BlobDataBuilder builder1(kId1);
  builder1.AppendFileSystemFile(GURL(), 0, kLargeSize, base::Time::Now());

  BlobDataBuilder builder2(kId2);
  builder2.AppendBlob(kId1, kLargeSize - kBlobLength, kBlobLength);

  std::unique_ptr<BlobDataHandle> blob_data_handle1 =
      context_->AddFinishedBlob(&builder1);
  std::unique_ptr<BlobDataHandle> blob_data_handle2 =
      context_->AddFinishedBlob(&builder2);

  ASSERT_TRUE(blob_data_handle1);
  ASSERT_TRUE(blob_data_handle2);
  std::unique_ptr<BlobDataSnapshot> data = blob_data_handle2->CreateSnapshot();
  ASSERT_EQ(1u, data->items().size());
  const scoped_refptr<BlobDataItem> item = data->items()[0];
  EXPECT_EQ(kLargeSize - kBlobLength, item->offset());
  EXPECT_EQ(kBlobLength, item->length());

  blob_data_handle1.reset();
  blob_data_handle2.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(BlobStorageContextTest, BuildDiskCacheBlob) {
  scoped_refptr<BlobDataBuilder::DataHandle>
      data_handle = new EmptyDataHandle();

  {
    BlobStorageContext context;

    std::unique_ptr<disk_cache::Backend> cache = CreateInMemoryDiskCache();
    ASSERT_TRUE(cache);

    const std::string kTestBlobData = "Test Blob Data";
    disk_cache::ScopedEntryPtr entry =
        CreateDiskCacheEntry(cache.get(), "test entry", kTestBlobData);

    const std::string kId1Prime("id1.prime");
    BlobDataBuilder canonicalized_blob_data(kId1Prime);
    canonicalized_blob_data.AppendData(kTestBlobData.c_str());

    const std::string kId1("id1");
    BlobDataBuilder builder(kId1);

    builder.AppendDiskCacheEntry(
        data_handle, entry.get(), kTestDiskCacheStreamIndex);

    std::unique_ptr<BlobDataHandle> blob_data_handle =
        context.AddFinishedBlob(&builder);
    std::unique_ptr<BlobDataSnapshot> data = blob_data_handle->CreateSnapshot();
    EXPECT_EQ(*data, builder);
    EXPECT_FALSE(data_handle->HasOneRef())
        << "Data handle was destructed while context and builder still exist.";
  }
  EXPECT_TRUE(data_handle->HasOneRef())
      << "Data handle was not destructed along with blob storage context.";
  base::RunLoop().RunUntilIdle();
}

TEST_F(BlobStorageContextTest, BuildFutureFileOnlyBlob) {
  const std::string kId1("id1");
  context_ =
      base::MakeUnique<BlobStorageContext>(temp_dir_.GetPath(), file_runner_);
  SetTestMemoryLimits();

  BlobDataBuilder builder(kId1);
  builder.set_content_type("text/plain");
  builder.AppendFutureFile(0, 10, 0);

  BlobStatus status = BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS;
  std::unique_ptr<BlobDataHandle> handle = context_->BuildBlob(
      builder, base::Bind(&SaveBlobStatusAndFiles, &status, &files_));

  size_t blobs_finished = 0;
  EXPECT_EQ(BlobStatus::PENDING_QUOTA, handle->GetBlobStatus());
  EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS, status);
  handle->RunOnConstructionComplete(
      base::Bind(&IncrementNumber, &blobs_finished));
  EXPECT_EQ(0u, blobs_finished);

  EXPECT_TRUE(file_runner_->HasPendingTask());
  file_runner_->RunPendingTasks();
  EXPECT_EQ(0u, blobs_finished);
  EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS, status);
  EXPECT_EQ(BlobStatus::PENDING_QUOTA, handle->GetBlobStatus());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(BlobStatus::PENDING_TRANSPORT, status);
  EXPECT_EQ(BlobStatus::PENDING_TRANSPORT, handle->GetBlobStatus());
  EXPECT_EQ(0u, blobs_finished);

  ASSERT_EQ(1u, files_.size());

  builder.PopulateFutureFile(0, files_[0].file_reference, base::Time::Max());
  context_->NotifyTransportComplete(kId1);

  EXPECT_EQ(BlobStatus::DONE, handle->GetBlobStatus());
  EXPECT_EQ(0u, blobs_finished);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, blobs_finished);

  builder.Clear();
  handle.reset();
  files_.clear();
  base::RunLoop().RunUntilIdle();
  // We should have file cleanup tasks.
  EXPECT_TRUE(file_runner_->HasPendingTask());
  file_runner_->RunPendingTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0lu, context_->memory_controller().memory_usage());
  EXPECT_EQ(0lu, context_->memory_controller().disk_usage());
}

TEST_F(BlobStorageContextTest, CompoundBlobs) {
  const std::string kId1("id1");
  const std::string kId2("id2");
  const std::string kId3("id3");

  // Setup a set of blob data for testing.
  base::Time time1, time2;
  ASSERT_TRUE(base::Time::FromString("Tue, 15 Nov 1994, 12:45:26 GMT", &time1));
  ASSERT_TRUE(base::Time::FromString("Mon, 14 Nov 1994, 11:30:49 GMT", &time2));

  BlobDataBuilder blob_data1(kId1);
  blob_data1.AppendData("Data1");
  blob_data1.AppendData("Data2");
  blob_data1.AppendFile(base::FilePath(FILE_PATH_LITERAL("File1.txt")), 10,
                        1024, time1);

  BlobDataBuilder blob_data2(kId2);
  blob_data2.AppendData("Data3");
  blob_data2.AppendBlob(kId1, 8, 100);
  blob_data2.AppendFile(base::FilePath(FILE_PATH_LITERAL("File2.txt")), 0, 20,
                        time2);

  BlobDataBuilder blob_data3(kId3);
  blob_data3.AppendData("Data4");
  std::unique_ptr<disk_cache::Backend> cache = CreateInMemoryDiskCache();
  ASSERT_TRUE(cache);
  disk_cache::ScopedEntryPtr disk_cache_entry =
      CreateDiskCacheEntry(cache.get(), "another key", "Data5");
  blob_data3.AppendDiskCacheEntry(new EmptyDataHandle(), disk_cache_entry.get(),
                                  kTestDiskCacheStreamIndex);

  BlobDataBuilder canonicalized_blob_data2(kId2);
  canonicalized_blob_data2.AppendData("Data3");
  canonicalized_blob_data2.AppendData("a2___", 2);
  canonicalized_blob_data2.AppendFile(
      base::FilePath(FILE_PATH_LITERAL("File1.txt")), 10, 98, time1);
  canonicalized_blob_data2.AppendFile(
      base::FilePath(FILE_PATH_LITERAL("File2.txt")), 0, 20, time2);

  BlobStorageContext context;
  std::unique_ptr<BlobDataHandle> blob_data_handle;

  // Test a blob referring to only data and a file.
  blob_data_handle = context_->AddFinishedBlob(&blob_data1);

  ASSERT_TRUE(blob_data_handle);
  std::unique_ptr<BlobDataSnapshot> data = blob_data_handle->CreateSnapshot();
  ASSERT_TRUE(blob_data_handle);
  EXPECT_EQ(*data, blob_data1);

  // Test a blob composed in part with another blob.
  blob_data_handle = context_->AddFinishedBlob(&blob_data2);
  data = blob_data_handle->CreateSnapshot();
  ASSERT_TRUE(blob_data_handle);
  ASSERT_TRUE(data);
  EXPECT_EQ(*data, canonicalized_blob_data2);

  // Test a blob referring to only data and a disk cache entry.
  blob_data_handle = context_->AddFinishedBlob(&blob_data3);
  data = blob_data_handle->CreateSnapshot();
  ASSERT_TRUE(blob_data_handle);
  EXPECT_EQ(*data, blob_data3);

  blob_data_handle.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(BlobStorageContextTest, PublicBlobUrls) {
  // Build up a basic blob.
  const std::string kId("id");
  std::unique_ptr<BlobDataHandle> first_handle = SetupBasicBlob(kId);

  // Now register a url for that blob.
  GURL kUrl("blob:id");
  context_->RegisterPublicBlobURL(kUrl, kId);
  std::unique_ptr<BlobDataHandle> blob_data_handle =
      context_->GetBlobDataFromPublicURL(kUrl);
  ASSERT_TRUE(blob_data_handle.get());
  EXPECT_EQ(kId, blob_data_handle->uuid());
  std::unique_ptr<BlobDataSnapshot> data = blob_data_handle->CreateSnapshot();
  blob_data_handle.reset();
  first_handle.reset();
  base::RunLoop().RunUntilIdle();

  // The url registration should keep the blob alive even after
  // explicit references are dropped.
  blob_data_handle = context_->GetBlobDataFromPublicURL(kUrl);
  EXPECT_TRUE(blob_data_handle);
  blob_data_handle.reset();

  base::RunLoop().RunUntilIdle();
  // Finally get rid of the url registration and the blob.
  context_->RevokePublicBlobURL(kUrl);
  blob_data_handle = context_->GetBlobDataFromPublicURL(kUrl);
  EXPECT_FALSE(blob_data_handle.get());
  EXPECT_FALSE(context_->registry().HasEntry(kId));
}

TEST_F(BlobStorageContextTest, TestUnknownBrokenAndBuildingBlobReference) {
  const std::string kBrokenId("broken_id");
  const std::string kBuildingId("building_id");
  const std::string kReferencingId("referencing_id");
  const std::string kUnknownId("unknown_id");

  // Create a broken blob.
  std::unique_ptr<BlobDataHandle> broken_handle =
      context_->AddBrokenBlob(kBrokenId, "", "", BlobStatus::ERR_OUT_OF_MEMORY);
  EXPECT_TRUE(broken_handle->GetBlobStatus() == BlobStatus::ERR_OUT_OF_MEMORY);
  EXPECT_TRUE(context_->registry().HasEntry(kBrokenId));

  // Try to create a blob with a reference to an unknown blob.
  BlobDataBuilder builder(kReferencingId);
  builder.AppendData("data");
  builder.AppendBlob(kUnknownId);
  std::unique_ptr<BlobDataHandle> handle = context_->AddFinishedBlob(builder);
  EXPECT_TRUE(handle->IsBroken());
  EXPECT_TRUE(context_->registry().HasEntry(kReferencingId));
  handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->registry().HasEntry(kReferencingId));

  // Try to create a blob with a reference to the broken blob.
  BlobDataBuilder builder2(kReferencingId);
  builder2.AppendData("data");
  builder2.AppendBlob(kBrokenId);
  handle = context_->AddFinishedBlob(builder2);
  EXPECT_TRUE(handle->IsBroken());
  EXPECT_TRUE(context_->registry().HasEntry(kReferencingId));
  handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->registry().HasEntry(kReferencingId));

  // Try to create a blob with a reference to the building blob.
  BlobDataBuilder builder3(kReferencingId);
  builder3.AppendData("data");
  builder3.AppendBlob(kBuildingId);
  handle = context_->AddFinishedBlob(builder3);
  EXPECT_TRUE(handle->IsBroken());
  EXPECT_TRUE(context_->registry().HasEntry(kReferencingId));
  handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->registry().HasEntry(kReferencingId));
}

namespace {
constexpr size_t kTotalRawBlobs = 200;
constexpr size_t kTotalSlicedBlobs = 100;
constexpr char kTestDiskCacheData[] = "Test Blob Data";

// Appends data and data types that depend on the index. This is designed to
// exercise all types of combinations of data, future data, files, future files,
// and disk cache entries.
size_t AppendDataInBuilder(BlobDataBuilder* builder,
                           size_t index,
                           disk_cache::Entry* cache_entry) {
  size_t size = 0;
  // We can't have both future data and future files, so split those up.
  if (index % 2 != 0) {
    builder->AppendFutureData(5u);
    size += 5u;
    if (index % 3 == 1) {
      builder->AppendData("abcdefghij", 4u);
      size += 4u;
    }
    if (index % 3 == 0) {
      builder->AppendFutureData(1u);
      size += 1u;
    }
  } else if (index % 3 == 0) {
    builder->AppendFutureFile(0lu, 3lu, 0);
    size += 3u;
  }
  if (index % 5 != 0) {
    builder->AppendFile(
        base::FilePath::FromUTF8Unsafe(base::SizeTToString(index)), 0ul, 20ul,
        base::Time::Max());
    size += 20u;
  }
  if (index % 3 != 0) {
    scoped_refptr<BlobDataBuilder::DataHandle> disk_cache_data_handle =
        new EmptyDataHandle();
    builder->AppendDiskCacheEntry(disk_cache_data_handle, cache_entry,
                                  kTestDiskCacheStreamIndex);
    size += strlen(kTestDiskCacheData);
  }
  return size;
}

bool DoesBuilderHaveFutureData(size_t index) {
  return index < kTotalRawBlobs && (index % 2 != 0 || index % 3 == 0);
}

void PopulateDataInBuilder(BlobDataBuilder* builder,
                           size_t index,
                           base::TaskRunner* file_runner) {
  if (index % 2 != 0) {
    builder->PopulateFutureData(0, "abcde", 0, 5);
    if (index % 3 == 0) {
      builder->PopulateFutureData(1, "z", 0, 1);
    }
  } else if (index % 3 == 0) {
    scoped_refptr<ShareableFileReference> file_ref =
        ShareableFileReference::GetOrCreate(
            base::FilePath::FromUTF8Unsafe(
                base::SizeTToString(index + kTotalRawBlobs)),
            ShareableFileReference::DONT_DELETE_ON_FINAL_RELEASE, file_runner);
    builder->PopulateFutureFile(0, file_ref, base::Time::Max());
  }
}
}  // namespace

TEST_F(BlobStorageContextTest, BuildBlobCombinations) {
  const std::string kId("id");

  context_ =
      base::MakeUnique<BlobStorageContext>(temp_dir_.GetPath(), file_runner_);

  SetTestMemoryLimits();
  std::unique_ptr<disk_cache::Backend> cache = CreateInMemoryDiskCache();
  ASSERT_TRUE(cache);
  disk_cache::ScopedEntryPtr entry =
      CreateDiskCacheEntry(cache.get(), "test entry", kTestDiskCacheData);

  // This tests mixed blob content with both synchronous and asynchronous
  // construction. Blobs should also be paged to disk during execution.
  std::vector<std::unique_ptr<BlobDataBuilder>> builders;
  std::vector<size_t> sizes;
  for (size_t i = 0; i < kTotalRawBlobs; i++) {
    builders.emplace_back(new BlobDataBuilder(base::SizeTToString(i)));
    auto& builder = *builders.back();
    size_t size = AppendDataInBuilder(&builder, i, entry.get());
    EXPECT_NE(0u, size);
    sizes.push_back(size);
  }

  for (size_t i = 0; i < kTotalSlicedBlobs; i++) {
    builders.emplace_back(
        new BlobDataBuilder(base::SizeTToString(i + kTotalRawBlobs)));
    size_t source_size = sizes[i];
    size_t offset = source_size == 1 ? 0 : i % (source_size - 1);
    size_t size = (i % (source_size - offset)) + 1;
    builders.back()->AppendBlob(base::SizeTToString(i), offset, size);
  }

  size_t total_finished_blobs = 0;
  std::vector<std::unique_ptr<BlobDataHandle>> handles;
  std::vector<BlobStatus> statuses;
  std::vector<bool> populated;
  statuses.resize(kTotalRawBlobs,
                  BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS);
  populated.resize(kTotalRawBlobs, false);
  for (size_t i = 0; i < builders.size(); i++) {
    BlobDataBuilder& builder = *builders[i];
    builder.set_content_type("text/plain");
    bool has_pending_memory = DoesBuilderHaveFutureData(i);
    std::unique_ptr<BlobDataHandle> handle = context_->BuildBlob(
        builder,
        has_pending_memory
            ? base::Bind(&SaveBlobStatusAndFiles, &statuses[0] + i, &files_)
            : BlobStorageContext::TransportAllowedCallback());
    handle->RunOnConstructionComplete(
        base::Bind(&IncrementNumber, &total_finished_blobs));
    handles.push_back(std::move(handle));
  }
  base::RunLoop().RunUntilIdle();

  // We should be needing to send a page or two to disk.
  EXPECT_TRUE(file_runner_->HasPendingTask());
  do {
    file_runner_->RunPendingTasks();
    base::RunLoop().RunUntilIdle();
    // Continue populating data for items that can fit.
    for (size_t i = 0; i < kTotalRawBlobs; i++) {
      BlobDataBuilder* builder = builders[i].get();
      if (DoesBuilderHaveFutureData(i) && !populated[i] &&
          statuses[i] == BlobStatus::PENDING_TRANSPORT) {
        PopulateDataInBuilder(builder, i, file_runner_.get());
        context_->NotifyTransportComplete(base::SizeTToString(i));
        populated[i] = true;
      }
    }
    base::RunLoop().RunUntilIdle();
  } while (file_runner_->HasPendingTask());

  // Check all builders with future items were signalled and populated.
  for (size_t i = 0; i < populated.size(); i++) {
    if (DoesBuilderHaveFutureData(i)) {
      EXPECT_EQ(BlobStatus::PENDING_TRANSPORT, statuses[i]) << i;
      EXPECT_TRUE(populated[i]) << i;
    }
  }
  base::RunLoop().RunUntilIdle();

  // We should be completely built now.
  EXPECT_EQ(kTotalRawBlobs + kTotalSlicedBlobs, total_finished_blobs);
  for (std::unique_ptr<BlobDataHandle>& handle : handles) {
    EXPECT_EQ(BlobStatus::DONE, handle->GetBlobStatus());
  }
  handles.clear();
  base::RunLoop().RunUntilIdle();
  files_.clear();
  // We should have file cleanup tasks.
  EXPECT_TRUE(file_runner_->HasPendingTask());
  file_runner_->RunPendingTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0lu, context_->memory_controller().memory_usage());
  EXPECT_EQ(0lu, context_->memory_controller().disk_usage());
}

// TODO(michaeln): tests for the deprecated url stuff

}  // namespace storage
