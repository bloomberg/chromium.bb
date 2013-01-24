// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_file_stream_reader.h"

#include <limits>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/mock_file_system_options.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/quota/mock_special_storage_policy.h"

namespace fileapi {

namespace {

const char kURLOrigin[] = "http://remote/";
const char kTestFileName[] = "test.dat";
const char kTestData[] = "0123456789";
const int kTestDataSize = arraysize(kTestData) - 1;

void ReadFromReader(FileSystemFileStreamReader* reader,
                    std::string* data,
                    size_t size,
                    int* result) {
  ASSERT_TRUE(reader != NULL);
  ASSERT_TRUE(result != NULL);
  *result = net::OK;
  net::TestCompletionCallback callback;
  size_t total_bytes_read = 0;
  while (total_bytes_read < size) {
    scoped_refptr<net::IOBufferWithSize> buf(
        new net::IOBufferWithSize(size - total_bytes_read));
    int rv = reader->Read(buf, buf->size(), callback.callback());
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    if (rv < 0)
      *result = rv;
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data->append(buf->data(), rv);
  }
}

void NeverCalled(int unused) { ADD_FAILURE(); }

}  // namespace

class FileSystemFileStreamReaderTest : public testing::Test {
 public:
  FileSystemFileStreamReaderTest()
      : message_loop_(MessageLoop::TYPE_IO) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    special_storage_policy_ = new quota::MockSpecialStoragePolicy;
    file_system_context_ =
        new FileSystemContext(
            FileSystemTaskRunners::CreateMockTaskRunners(),
            special_storage_policy_,
            NULL,
            temp_dir_.path(),
            CreateDisallowFileAccessOptions());

    file_system_context_->sandbox_provider()->ValidateFileSystemRoot(
        GURL(kURLOrigin), kFileSystemTypeTemporary, true,  // create
        base::Bind(&OnValidateFileSystem));
    MessageLoop::current()->RunUntilIdle();

    WriteFile(kTestFileName, kTestData, kTestDataSize,
              &test_file_modification_time_);
  }

  virtual void TearDown() OVERRIDE {
    MessageLoop::current()->RunUntilIdle();
  }

 protected:
  FileSystemFileStreamReader* CreateFileReader(
      const std::string& file_name,
      int64 initial_offset,
      const base::Time& expected_modification_time) {
    return new FileSystemFileStreamReader(file_system_context_,
                                          GetFileSystemURL(file_name),
                                          initial_offset,
                                          expected_modification_time);
  }

  base::Time test_file_modification_time() const {
    return test_file_modification_time_;
  }

  void WriteFile(const std::string& file_name,
                 const char* buf,
                 int buf_size,
                 base::Time* modification_time) {
    FileSystemFileUtil* file_util = file_system_context_->
        sandbox_provider()->GetFileUtil(kFileSystemTypeTemporary);
    FileSystemURL url = GetFileSystemURL(file_name);

    FileSystemOperationContext context(file_system_context_);
    context.set_allowed_bytes_growth(1024);

    base::PlatformFile handle = base::kInvalidPlatformFileValue;
    bool created = false;
    ASSERT_EQ(base::PLATFORM_FILE_OK, file_util->CreateOrOpen(
        &context,
        url,
        base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE,
        &handle,
        &created));
    EXPECT_TRUE(created);
    ASSERT_NE(base::kInvalidPlatformFileValue, handle);
    ASSERT_EQ(buf_size,
              base::WritePlatformFile(handle, 0 /* offset */, buf, buf_size));
    base::ClosePlatformFile(handle);

    base::PlatformFileInfo file_info;
    FilePath platform_path;
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              file_util->GetFileInfo(&context, url, &file_info,
                                     &platform_path));
    if (modification_time)
      *modification_time = file_info.last_modified;
  }

 private:
  static void OnValidateFileSystem(base::PlatformFileError result) {
    ASSERT_EQ(base::PLATFORM_FILE_OK, result);
  }

  FileSystemURL GetFileSystemURL(const std::string& file_name) {
    return file_system_context_->CreateCrackedFileSystemURL(
        GURL(kURLOrigin),
        kFileSystemTypeTemporary,
        FilePath().AppendASCII(file_name));
  }

  MessageLoop message_loop_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<quota::MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<FileSystemContext> file_system_context_;
  base::Time test_file_modification_time_;
};

TEST_F(FileSystemFileStreamReaderTest, NonExistent) {
  const char kFileName[] = "nonexistent";
  scoped_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kFileName, 0, base::Time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, 10, &result);
  ASSERT_EQ(net::ERR_FILE_NOT_FOUND, result);
  ASSERT_EQ(0U, data.size());
}

TEST_F(FileSystemFileStreamReaderTest, Empty) {
  const char kFileName[] = "empty";
  WriteFile(kFileName, NULL, 0, NULL);

  scoped_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kFileName, 0, base::Time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, 10, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(0U, data.size());

  net::TestInt64CompletionCallback callback;
  result = reader->GetLength(callback.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback.WaitForResult();
  ASSERT_EQ(0, result);
}

TEST_F(FileSystemFileStreamReaderTest, GetLengthNormal) {
  scoped_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 0, test_file_modification_time()));
  net::TestInt64CompletionCallback callback;
  int result = reader->GetLength(callback.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback.WaitForResult();
  ASSERT_EQ(kTestDataSize, result);
}

TEST_F(FileSystemFileStreamReaderTest, GetLengthAfterModified) {
  // Pass a fake expected modifictaion time so that the expectation fails.
  base::Time fake_expected_modification_time =
      test_file_modification_time() - base::TimeDelta::FromSeconds(10);

  scoped_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 0, fake_expected_modification_time));
  net::TestInt64CompletionCallback callback;
  int result = reader->GetLength(callback.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback.WaitForResult();
  ASSERT_EQ(net::ERR_UPLOAD_FILE_CHANGED, result);

  // With NULL expected modification time this should work.
  reader.reset(CreateFileReader(kTestFileName, 0, base::Time()));
  result = reader->GetLength(callback.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback.WaitForResult();
  ASSERT_EQ(kTestDataSize, result);
}

TEST_F(FileSystemFileStreamReaderTest, GetLengthWithOffset) {
  scoped_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 3, base::Time()));
  net::TestInt64CompletionCallback callback;
  int result = reader->GetLength(callback.callback());
  if (result == net::ERR_IO_PENDING)
    result = callback.WaitForResult();
  // Initial offset does not affect the result of GetLength.
  ASSERT_EQ(kTestDataSize, result);
}

TEST_F(FileSystemFileStreamReaderTest, ReadNormal) {
  scoped_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 0, test_file_modification_time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(kTestData, data);
}

TEST_F(FileSystemFileStreamReaderTest, ReadAfterModified) {
  // Pass a fake expected modifictaion time so that the expectation fails.
  base::Time fake_expected_modification_time =
      test_file_modification_time() - base::TimeDelta::FromSeconds(10);

  scoped_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 0, fake_expected_modification_time));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  ASSERT_EQ(net::ERR_UPLOAD_FILE_CHANGED, result);
  ASSERT_EQ(0U, data.size());

  // With NULL expected modification time this should work.
  data.clear();
  reader.reset(CreateFileReader(kTestFileName, 0, base::Time()));
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(kTestData, data);
}

TEST_F(FileSystemFileStreamReaderTest, ReadWithOffset) {
  scoped_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 3, base::Time()));
  int result = 0;
  std::string data;
  ReadFromReader(reader.get(), &data, kTestDataSize, &result);
  ASSERT_EQ(net::OK, result);
  ASSERT_EQ(&kTestData[3], data);
}

TEST_F(FileSystemFileStreamReaderTest, DeleteWithUnfinishedRead) {
  scoped_ptr<FileSystemFileStreamReader> reader(
      CreateFileReader(kTestFileName, 0, base::Time()));

  net::TestCompletionCallback callback;
  scoped_refptr<net::IOBufferWithSize> buf(
      new net::IOBufferWithSize(kTestDataSize));
  int rv = reader->Read(buf, buf->size(), base::Bind(&NeverCalled));
  ASSERT_TRUE(rv == net::ERR_IO_PENDING || rv >= 0);

  // Delete immediately.
  // Should not crash; nor should NeverCalled be callback.
  reader.reset();
}

}  // namespace fileapi
