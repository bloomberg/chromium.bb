// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_builder_from_stream.h"

#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/common/data_pipe_utils.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

namespace {

constexpr size_t kTestBlobStorageMaxBytesDataItemSize = 13;
constexpr size_t kTestBlobStorageMaxBlobMemorySize = 500;
constexpr uint64_t kTestBlobStorageMinFileSizeBytes = 32;
constexpr uint64_t kTestBlobStorageMaxDiskSpace = 1000;
}  // namespace

class BlobBuilderFromStreamTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    context_ = std::make_unique<BlobStorageContext>(
        data_dir_.GetPath(),
        base::CreateTaskRunnerWithTraits({base::MayBlock()}));

    limits_.max_bytes_data_item_size = kTestBlobStorageMaxBytesDataItemSize;
    limits_.max_blob_in_memory_space = kTestBlobStorageMaxBlobMemorySize;
    limits_.min_page_file_size = kTestBlobStorageMinFileSizeBytes;
    limits_.desired_max_disk_space = kTestBlobStorageMaxDiskSpace;
    limits_.effective_max_disk_space = kTestBlobStorageMaxDiskSpace;
    context_->set_limits_for_testing(limits_);
  }

  void TearDown() override {
    // Make sure we clean up files.
    base::RunLoop().RunUntilIdle();
    base::TaskScheduler::GetInstance()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<BlobDataHandle> BuildFromString(std::string data) {
    mojo::DataPipe pipe;
    base::RunLoop loop;
    std::unique_ptr<BlobDataHandle> result;
    BlobBuilderFromStream* finished_builder = nullptr;
    BlobBuilderFromStream builder(
        context_->AsWeakPtr(), kContentType, kContentDisposition, 0,
        std::move(pipe.consumer_handle),
        base::BindLambdaForTesting([&](BlobBuilderFromStream* result_builder,
                                       std::unique_ptr<BlobDataHandle> blob) {
          finished_builder = result_builder;
          result = std::move(blob);
          loop.Quit();
        }));

    mojo::common::BlockingCopyFromString(data, pipe.producer_handle);
    pipe.producer_handle.reset();

    loop.Run();
    EXPECT_EQ(&builder, finished_builder);
    return result;
  }

 protected:
  const std::string kContentType = "content/type";
  const std::string kContentDisposition = "disposition";

  base::ScopedTempDir data_dir_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  BlobStorageLimits limits_;
  std::unique_ptr<BlobStorageContext> context_;
};

TEST_F(BlobBuilderFromStreamTest, CallbackCalledOnDeletion) {
  mojo::DataPipe pipe;

  base::RunLoop loop;
  BlobBuilderFromStream* builder_ptr = nullptr;
  auto builder = std::make_unique<BlobBuilderFromStream>(
      context_->AsWeakPtr(), "", "", 0, std::move(pipe.consumer_handle),
      base::BindLambdaForTesting([&](BlobBuilderFromStream* result_builder,
                                     std::unique_ptr<BlobDataHandle> blob) {
        EXPECT_EQ(builder_ptr, result_builder);
        EXPECT_FALSE(blob);
        loop.Quit();
      }));
  builder_ptr = builder.get();
  builder.reset();
  loop.Run();
}

TEST_F(BlobBuilderFromStreamTest, EmptyStream) {
  std::unique_ptr<BlobDataHandle> result = BuildFromString("");

  ASSERT_TRUE(result);
  EXPECT_FALSE(result->uuid().empty());
  EXPECT_EQ(BlobStatus::DONE, result->GetBlobStatus());
  EXPECT_EQ(kContentType, result->content_type());
  EXPECT_EQ(kContentDisposition, result->content_disposition());
  EXPECT_EQ(0u, result->size());

  // Verify memory usage.
  EXPECT_EQ(0u, context_->memory_controller().memory_usage());
  EXPECT_EQ(0u, context_->memory_controller().disk_usage());
}

TEST_F(BlobBuilderFromStreamTest, SmallStream) {
  const std::string kData =
      base::RandBytesAsString(kTestBlobStorageMaxBytesDataItemSize + 5);
  std::unique_ptr<BlobDataHandle> result = BuildFromString(kData);

  ASSERT_TRUE(result);
  EXPECT_FALSE(result->uuid().empty());
  EXPECT_EQ(BlobStatus::DONE, result->GetBlobStatus());
  EXPECT_EQ(kData.size(), result->size());

  // Verify memory usage.
  EXPECT_EQ(kData.size(), context_->memory_controller().memory_usage());
  EXPECT_EQ(0u, context_->memory_controller().disk_usage());
}

TEST_F(BlobBuilderFromStreamTest, MediumStream) {
  const std::string kData =
      base::RandBytesAsString(kTestBlobStorageMinFileSizeBytes * 3 + 13);
  std::unique_ptr<BlobDataHandle> result = BuildFromString(kData);

  ASSERT_TRUE(result);
  EXPECT_FALSE(result->uuid().empty());
  EXPECT_EQ(BlobStatus::DONE, result->GetBlobStatus());
  EXPECT_EQ(kData.size(), result->size());

  // Verify memory usage.
  EXPECT_EQ(2 * kTestBlobStorageMaxBytesDataItemSize,
            context_->memory_controller().memory_usage());
  EXPECT_EQ(kData.size() - 2 * kTestBlobStorageMaxBytesDataItemSize,
            context_->memory_controller().disk_usage());
}

TEST_F(BlobBuilderFromStreamTest, TooLargeForQuota) {
  const std::string kData = base::RandBytesAsString(
      kTestBlobStorageMaxDiskSpace + kTestBlobStorageMaxBlobMemorySize + 1);
  std::unique_ptr<BlobDataHandle> result = BuildFromString(kData);
  EXPECT_FALSE(result);
}

TEST_F(BlobBuilderFromStreamTest, TooLargeForQuotaAndNoDisk) {
  context_->DisableFilePagingForTesting();

  const std::string kData =
      base::RandBytesAsString(kTestBlobStorageMaxBlobMemorySize + 1);
  std::unique_ptr<BlobDataHandle> result = BuildFromString(kData);
  EXPECT_FALSE(result);
}

}  // namespace storage
