// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_transport_strategy.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/task_scheduler/post_task.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/test/mock_bytes_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

namespace {

using MemoryStrategy = BlobMemoryController::Strategy;
using FileInfoVector = std::vector<BlobMemoryController::FileCreationInfo>;

const size_t kTestBlobStorageIPCThresholdBytes = 5;
const size_t kTestBlobStorageMaxSharedMemoryBytes = 20;
const size_t kTestBlobStorageMaxBytesDataItemSize = 13;

const size_t kTestBlobStorageMaxBlobMemorySize = 400;
const uint64_t kTestBlobStorageMaxDiskSpace = 4000;
const uint64_t kTestBlobStorageMinFileSizeBytes = 10;
const uint64_t kTestBlobStorageMaxFileSizeBytes = 100;

const char kId[] = "blob-id";

void BindBytesProvider(std::unique_ptr<MockBytesProvider> impl,
                       mojom::BytesProviderRequest request) {
  mojo::MakeStrongBinding(std::move(impl), std::move(request));
}

class BlobTransportStrategyTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());

    bytes_provider_runner_ =
        base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()});
    mock_time_ = base::Time::Now();

    limits_.max_ipc_memory_size = kTestBlobStorageIPCThresholdBytes;
    limits_.max_shared_memory_size = kTestBlobStorageMaxSharedMemoryBytes;
    limits_.max_bytes_data_item_size = kTestBlobStorageMaxBytesDataItemSize;
    limits_.max_blob_in_memory_space = kTestBlobStorageMaxBlobMemorySize;
    limits_.desired_max_disk_space = kTestBlobStorageMaxDiskSpace;
    limits_.effective_max_disk_space = kTestBlobStorageMaxDiskSpace;
    limits_.min_page_file_size = kTestBlobStorageMinFileSizeBytes;
    limits_.max_file_size = kTestBlobStorageMaxFileSizeBytes;

    mojo::edk::SetDefaultProcessErrorCallback(base::Bind(
        &BlobTransportStrategyTest::OnBadMessage, base::Unretained(this)));

    // Disallow IO on the main loop.
    base::ThreadRestrictions::SetIOAllowed(false);
  }

  void TearDown() override {
    base::ThreadRestrictions::SetIOAllowed(true);

    mojo::edk::SetDefaultProcessErrorCallback(
        mojo::edk::ProcessErrorCallback());
  }

  void OnBadMessage(const std::string& error) {
    bad_messages_.push_back(error);
  }

  mojom::BytesProviderPtr CreateBytesProvider(const std::string& bytes,
                                              base::Optional<base::Time> time) {
    mojom::BytesProviderPtr result;
    auto provider = base::MakeUnique<MockBytesProvider>(
        std::vector<uint8_t>(bytes.begin(), bytes.end()), &reply_request_count_,
        &stream_request_count_, &file_request_count_, time);
    bytes_provider_runner_->PostTask(
        FROM_HERE, base::BindOnce(&BindBytesProvider, std::move(provider),
                                  MakeRequest(&result)));
    return result;
  }

 protected:
  base::ScopedTempDir data_dir_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  scoped_refptr<base::SequencedTaskRunner> bytes_provider_runner_;
  base::Time mock_time_;
  storage::BlobStorageLimits limits_;

  std::vector<std::string> bad_messages_;

  size_t reply_request_count_ = 0;
  size_t stream_request_count_ = 0;
  size_t file_request_count_ = 0;
};

class BasicTests : public BlobTransportStrategyTest,
                   public testing::WithParamInterface<MemoryStrategy> {};

TEST_P(BasicTests, NoBytes) {
  BlobDataBuilder builder(kId);
  BlobDataBuilder expected(kId);

  base::RunLoop loop;
  BlobStatus status = BlobStatus::PENDING_TRANSPORT;
  auto strategy = BlobTransportStrategy::Create(
      GetParam(), &builder,
      base::BindOnce(
          [](BlobStatus* result_out, base::OnceClosure closure,
             BlobStatus result) {
            *result_out = result;
            std::move(closure).Run();
          },
          &status, loop.QuitClosure()),
      limits_);

  strategy->BeginTransport(FileInfoVector());
  loop.Run();

  EXPECT_EQ(BlobStatus::DONE, status);
  EXPECT_EQ(expected, builder);
  EXPECT_EQ(0u,
            reply_request_count_ + stream_request_count_ + file_request_count_);
  EXPECT_TRUE(bad_messages_.empty());
}

TEST_P(BasicTests, WithBytes) {
  BlobDataBuilder builder(kId);
  BlobDataBuilder expected(kId);

  base::RunLoop loop;
  BlobStatus status = BlobStatus::PENDING_TRANSPORT;
  auto strategy = BlobTransportStrategy::Create(
      GetParam(), &builder,
      base::BindOnce(
          [](BlobStatus* result_out, base::OnceClosure closure,
             BlobStatus result) {
            *result_out = result;
            std::move(closure).Run();
          },
          &status, loop.QuitClosure()),
      limits_);

  std::string data = base::RandBytesAsString(7);
  mojom::DataElementBytes bytes1(data.size(),
                                 std::vector<uint8_t>(data.begin(), data.end()),
                                 CreateBytesProvider(data, mock_time_));
  strategy->AddBytesElement(&bytes1);
  expected.AppendData(data);

  data = base::RandBytesAsString(3);
  mojom::DataElementBytes bytes2(data.size(),
                                 std::vector<uint8_t>(data.begin(), data.end()),
                                 CreateBytesProvider(data, mock_time_));
  strategy->AddBytesElement(&bytes2);
  expected.AppendData(data);

  data = base::RandBytesAsString(10);
  mojom::DataElementBytes bytes3(data.size(), base::nullopt,
                                 CreateBytesProvider(data, mock_time_));
  if (GetParam() != MemoryStrategy::NONE_NEEDED) {
    strategy->AddBytesElement(&bytes3);
    expected.AppendData(data);
  }

  strategy->BeginTransport(FileInfoVector());
  loop.Run();

  EXPECT_EQ(BlobStatus::DONE, status);
  EXPECT_EQ(expected, builder);
  EXPECT_TRUE(bad_messages_.empty());

  if (GetParam() == MemoryStrategy::NONE_NEEDED) {
    EXPECT_EQ(
        0u, reply_request_count_ + stream_request_count_ + file_request_count_);
  } else {
    EXPECT_EQ(
        3u, reply_request_count_ + stream_request_count_ + file_request_count_);
    switch (GetParam()) {
      case MemoryStrategy::IPC:
        EXPECT_EQ(3u, reply_request_count_);
        break;
      case MemoryStrategy::SHARED_MEMORY:
        EXPECT_EQ(3u, stream_request_count_);
        break;
      default:
        NOTREACHED();
    }
  }
}

INSTANTIATE_TEST_CASE_P(BlobTransportStrategyTest,
                        BasicTests,
                        testing::Values(MemoryStrategy::NONE_NEEDED,
                                        MemoryStrategy::IPC,
                                        MemoryStrategy::SHARED_MEMORY));

class BasicErrorTests : public BlobTransportStrategyTest,
                        public testing::WithParamInterface<MemoryStrategy> {};

TEST_P(BasicErrorTests, NotEnoughBytesInProvider) {
  BlobDataBuilder builder(kId);

  base::RunLoop loop;
  BlobStatus status = BlobStatus::PENDING_TRANSPORT;
  auto strategy = BlobTransportStrategy::Create(
      GetParam(), &builder,
      base::BindOnce(
          [](BlobStatus* result_out, base::OnceClosure closure,
             BlobStatus result) {
            *result_out = result;
            std::move(closure).Run();
          },
          &status, loop.QuitClosure()),
      limits_);

  std::string data = base::RandBytesAsString(7);
  mojom::DataElementBytes bytes(
      data.size(), base::nullopt,
      CreateBytesProvider(data.substr(0, 4), mock_time_));
  strategy->AddBytesElement(&bytes);

  strategy->BeginTransport(FileInfoVector());
  loop.Run();

  EXPECT_TRUE(BlobStatusIsError(status));
  EXPECT_EQ(GetParam() == MemoryStrategy::SHARED_MEMORY, bad_messages_.empty());
}

TEST_P(BasicErrorTests, TooManyBytesInProvider) {
  BlobDataBuilder builder(kId);

  base::RunLoop loop;
  BlobStatus status = BlobStatus::PENDING_TRANSPORT;
  auto strategy = BlobTransportStrategy::Create(
      GetParam(), &builder,
      base::BindOnce(
          [](BlobStatus* result_out, base::OnceClosure closure,
             BlobStatus result) {
            *result_out = result;
            std::move(closure).Run();
          },
          &status, loop.QuitClosure()),
      limits_);

  std::string data = base::RandBytesAsString(4);
  mojom::DataElementBytes bytes(
      data.size(), base::nullopt,
      CreateBytesProvider(data + "foobar", mock_time_));
  strategy->AddBytesElement(&bytes);

  strategy->BeginTransport(FileInfoVector());
  loop.Run();

  if (GetParam() == MemoryStrategy::SHARED_MEMORY) {
    EXPECT_TRUE(status == BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS ||
                status == BlobStatus::DONE);
  } else {
    EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS, status);
    EXPECT_FALSE(bad_messages_.empty());
  }
}

INSTANTIATE_TEST_CASE_P(BlobTransportStrategyTest,
                        BasicErrorTests,
                        testing::Values(MemoryStrategy::IPC,
                                        MemoryStrategy::SHARED_MEMORY));

TEST_F(BlobTransportStrategyTest, DataStreamChunksData) {
  BlobDataBuilder builder(kId);
  BlobDataBuilder expected(kId);

  base::RunLoop loop;
  BlobStatus status = BlobStatus::PENDING_TRANSPORT;
  auto strategy = BlobTransportStrategy::Create(
      MemoryStrategy::SHARED_MEMORY, &builder,
      base::BindOnce(
          [](BlobStatus* result_out, base::OnceClosure closure,
             BlobStatus result) {
            *result_out = result;
            std::move(closure).Run();
          },
          &status, loop.QuitClosure()),
      limits_);

  std::string data =
      base::RandBytesAsString(kTestBlobStorageMaxSharedMemoryBytes * 3 + 13);
  mojom::DataElementBytes bytes(data.size(), base::nullopt,
                                CreateBytesProvider(data, mock_time_));
  strategy->AddBytesElement(&bytes);

  size_t offset = 0;
  while (offset < data.size()) {
    expected.AppendData(
        data.substr(offset, kTestBlobStorageMaxBytesDataItemSize));
    offset += kTestBlobStorageMaxBytesDataItemSize;
  }

  strategy->BeginTransport(FileInfoVector());
  loop.Run();

  EXPECT_EQ(BlobStatus::DONE, status);
  EXPECT_EQ(expected, builder);

  EXPECT_EQ(0u, reply_request_count_);
  EXPECT_EQ(1u, stream_request_count_);
  EXPECT_EQ(0u, file_request_count_);
}

TEST_F(BlobTransportStrategyTest, Files_NoBytes) {
  BlobDataBuilder builder(kId);
  BlobDataBuilder expected(kId);

  base::RunLoop loop;
  BlobStatus status = BlobStatus::PENDING_TRANSPORT;
  auto strategy = BlobTransportStrategy::Create(
      MemoryStrategy::FILE, &builder,
      base::BindOnce(
          [](BlobStatus* result_out, base::OnceClosure closure,
             BlobStatus result) {
            *result_out = result;
            std::move(closure).Run();
          },
          &status, loop.QuitClosure()),
      limits_);

  strategy->BeginTransport(FileInfoVector());
  loop.Run();

  EXPECT_EQ(BlobStatus::DONE, status);
  EXPECT_EQ(expected, builder);
  EXPECT_EQ(0u,
            reply_request_count_ + stream_request_count_ + file_request_count_);
  EXPECT_TRUE(bad_messages_.empty());
}

TEST_F(BlobTransportStrategyTest, Files_WriteFailed) {
  BlobDataBuilder builder(kId);

  base::RunLoop loop;
  BlobStatus status = BlobStatus::PENDING_TRANSPORT;
  auto strategy = BlobTransportStrategy::Create(
      MemoryStrategy::FILE, &builder,
      base::BindOnce(
          [](BlobStatus* result_out, base::OnceClosure closure,
             BlobStatus result) {
            *result_out = result;
            std::move(closure).Run();
          },
          &status, loop.QuitClosure()),
      limits_);

  std::string data = base::RandBytesAsString(kTestBlobStorageMaxFileSizeBytes);
  mojom::DataElementBytes bytes(data.size(), base::nullopt,
                                CreateBytesProvider(data, base::nullopt));
  strategy->AddBytesElement(&bytes);

  FileInfoVector files(1);
  {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    base::FilePath path;
    ASSERT_TRUE(base::CreateTemporaryFileInDir(data_dir_.GetPath(), &path));
    files[0].file =
        base::File(path, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
    files[0].file_deletion_runner = base::ThreadTaskRunnerHandle::Get();
    files[0].file_reference = ShareableFileReference::GetOrCreate(
        path, ShareableFileReference::DELETE_ON_FINAL_RELEASE,
        bytes_provider_runner_.get());
  }

  strategy->BeginTransport(std::move(files));
  loop.Run();

  EXPECT_EQ(BlobStatus::ERR_FILE_WRITE_FAILED, status);
}

TEST_F(BlobTransportStrategyTest, Files_ValidBytesOneElement) {
  BlobDataBuilder builder(kId);
  BlobDataBuilder expected(kId);

  base::RunLoop loop;
  BlobStatus status = BlobStatus::PENDING_TRANSPORT;
  auto strategy = BlobTransportStrategy::Create(
      MemoryStrategy::FILE, &builder,
      base::BindOnce(
          [](BlobStatus* result_out, base::OnceClosure closure,
             BlobStatus result) {
            *result_out = result;
            std::move(closure).Run();
          },
          &status, loop.QuitClosure()),
      limits_);

  std::string data =
      base::RandBytesAsString(kTestBlobStorageMaxBlobMemorySize + 42);
  mojom::DataElementBytes bytes(data.size(), base::nullopt,
                                CreateBytesProvider(data, mock_time_));
  strategy->AddBytesElement(&bytes);

  size_t expected_file_count =
      1 + data.size() / kTestBlobStorageMaxFileSizeBytes;
  FileInfoVector files(expected_file_count);
  for (size_t i = 0; i < expected_file_count; ++i) {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    base::FilePath path;
    ASSERT_TRUE(base::CreateTemporaryFileInDir(data_dir_.GetPath(), &path));
    files[i].file =
        base::File(path, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
    files[i].file_deletion_runner = base::ThreadTaskRunnerHandle::Get();
    files[i].file_reference = ShareableFileReference::GetOrCreate(
        path, ShareableFileReference::DELETE_ON_FINAL_RELEASE,
        bytes_provider_runner_.get());
    size_t offset = i * kTestBlobStorageMaxFileSizeBytes;
    size_t length = std::min<uint64_t>(kTestBlobStorageMaxFileSizeBytes,
                                       data.size() - offset);
    expected.AppendFile(path, 0, length, mock_time_);
  }

  strategy->BeginTransport(std::move(files));
  loop.Run();

  EXPECT_EQ(BlobStatus::DONE, status);
  EXPECT_EQ(expected, builder);
  EXPECT_TRUE(bad_messages_.empty());
  EXPECT_EQ(0u, reply_request_count_);
  EXPECT_EQ(0u, stream_request_count_);
  EXPECT_EQ(expected_file_count, file_request_count_);
}

TEST_F(BlobTransportStrategyTest, Files_ValidBytesMultipleElements) {
  BlobDataBuilder builder(kId);
  BlobDataBuilder expected(kId);

  base::RunLoop loop;
  BlobStatus status = BlobStatus::PENDING_TRANSPORT;
  auto strategy = BlobTransportStrategy::Create(
      MemoryStrategy::FILE, &builder,
      base::BindOnce(
          [](BlobStatus* result_out, base::OnceClosure closure,
             BlobStatus result) {
            *result_out = result;
            std::move(closure).Run();
          },
          &status, loop.QuitClosure()),
      limits_);

  std::string data =
      base::RandBytesAsString(kTestBlobStorageMaxBlobMemorySize / 3);

  mojom::DataElementBytes bytes1(data.size(), base::nullopt,
                                 CreateBytesProvider(data, mock_time_));
  strategy->AddBytesElement(&bytes1);
  mojom::DataElementBytes bytes2(data.size(), base::nullopt,
                                 CreateBytesProvider(data, mock_time_));
  strategy->AddBytesElement(&bytes2);
  mojom::DataElementBytes bytes3(data.size(), base::nullopt,
                                 CreateBytesProvider(data, mock_time_));
  strategy->AddBytesElement(&bytes3);
  mojom::DataElementBytes bytes4(data.size(), base::nullopt,
                                 CreateBytesProvider(data, mock_time_));
  strategy->AddBytesElement(&bytes4);

  size_t expected_file_count =
      1 + 4 * data.size() / kTestBlobStorageMaxFileSizeBytes;
  FileInfoVector files(expected_file_count);
  for (size_t i = 0; i < expected_file_count; ++i) {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    base::FilePath path;
    ASSERT_TRUE(base::CreateTemporaryFileInDir(data_dir_.GetPath(), &path));
    files[i].file =
        base::File(path, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
    files[i].path = path;
    files[i].file_deletion_runner = base::ThreadTaskRunnerHandle::Get();
    files[i].file_reference = ShareableFileReference::GetOrCreate(
        path, ShareableFileReference::DELETE_ON_FINAL_RELEASE,
        bytes_provider_runner_.get());
  }

  size_t file_offset = 0;
  size_t file_index = 0;
  size_t expected_request_count = 0;
  for (size_t i = 0; i < 4; ++i) {
    size_t remaining_size = data.size();
    while (remaining_size > 0) {
      size_t block_size = std::min<uint64_t>(
          kTestBlobStorageMaxFileSizeBytes - file_offset, remaining_size);
      expected.AppendFile(files[file_index].path, file_offset, block_size,
                          mock_time_);
      expected_request_count++;
      remaining_size -= block_size;
      file_offset += block_size;
      if (file_offset >= kTestBlobStorageMaxFileSizeBytes) {
        file_offset = 0;
        file_index++;
      }
    }
  }

  strategy->BeginTransport(std::move(files));
  loop.Run();

  EXPECT_EQ(BlobStatus::DONE, status);
  EXPECT_EQ(expected, builder);
  EXPECT_TRUE(bad_messages_.empty());
  EXPECT_EQ(0u, reply_request_count_);
  EXPECT_EQ(0u, stream_request_count_);
  EXPECT_EQ(expected_request_count, file_request_count_);
}

}  // namespace

}  // namespace storage
