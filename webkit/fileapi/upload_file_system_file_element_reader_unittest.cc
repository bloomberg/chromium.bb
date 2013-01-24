// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/upload_file_system_file_element_reader.h"

#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/mock_file_system_options.h"

namespace fileapi {

namespace {

const char kFileSystemURLOrigin[] = "http://remote";
const fileapi::FileSystemType kFileSystemType =
    fileapi::kFileSystemTypeTemporary;

}  // namespace

class UploadFileSystemFileElementReaderTest : public testing::Test {
 public:
  UploadFileSystemFileElementReaderTest()
      : message_loop_(MessageLoop::TYPE_IO) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    file_system_context_ = new fileapi::FileSystemContext(
        fileapi::FileSystemTaskRunners::CreateMockTaskRunners(),
        NULL,
        NULL,
        temp_dir_.path(),
        fileapi::CreateDisallowFileAccessOptions());

    file_system_context_->OpenFileSystem(
        GURL(kFileSystemURLOrigin),
        kFileSystemType,
        true,  // create
        base::Bind(&UploadFileSystemFileElementReaderTest::OnValidateFileSystem,
                   base::Unretained(this)));
    MessageLoop::current()->RunUntilIdle();
    ASSERT_TRUE(file_system_root_url_.is_valid());

    // Prepare a file on file system.
    const char kTestData[] = "abcdefghijklmnop0123456789";
    file_data_.assign(kTestData, kTestData + arraysize(kTestData) - 1);
    const char kFilename[] = "File.dat";
    file_url_ = GetFileSystemURL(kFilename);
    WriteFileSystemFile(kFilename, &file_data_[0], file_data_.size(),
                        &file_modification_time_);

    // Create and initialize a reader.
    reader_.reset(new UploadFileSystemFileElementReader(
        file_system_context_, file_url_, 0, kuint64max,
        file_modification_time_));
    net::TestCompletionCallback callback;
    ASSERT_EQ(net::ERR_IO_PENDING, reader_->Init(callback.callback()));
    EXPECT_EQ(net::OK, callback.WaitForResult());
    EXPECT_EQ(file_data_.size(), reader_->GetContentLength());
    EXPECT_EQ(file_data_.size(), reader_->BytesRemaining());
    EXPECT_FALSE(reader_->IsInMemory());
  }

 protected:
  GURL GetFileSystemURL(const std::string& filename) {
    return GURL(file_system_root_url_.spec() + filename);
  }

  void WriteFileSystemFile(const std::string& filename,
                           const char* buf,
                           int buf_size,
                           base::Time* modification_time) {
    fileapi::FileSystemURL url =
        file_system_context_->CreateCrackedFileSystemURL(
            GURL(kFileSystemURLOrigin),
            kFileSystemType,
            FilePath().AppendASCII(filename));

    fileapi::FileSystemFileUtil* file_util =
        file_system_context_->GetFileUtil(kFileSystemType);

    fileapi::FileSystemOperationContext context(file_system_context_);
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
    *modification_time = file_info.last_modified;
  }

  void OnValidateFileSystem(base::PlatformFileError result,
                            const std::string& name,
                            const GURL& root) {
    ASSERT_EQ(base::PLATFORM_FILE_OK, result);
    ASSERT_TRUE(root.is_valid());
    file_system_root_url_ = root;
  }

  MessageLoop message_loop_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<FileSystemContext> file_system_context_;
  GURL file_system_root_url_;
  std::vector<char> file_data_;
  GURL file_url_;
  base::Time file_modification_time_;
  scoped_ptr<UploadFileSystemFileElementReader> reader_;
};

TEST_F(UploadFileSystemFileElementReaderTest, ReadAll) {
  scoped_refptr<net::IOBufferWithSize> buf =
      new net::IOBufferWithSize(file_data_.size());
  net::TestCompletionCallback read_callback;
  ASSERT_EQ(net::ERR_IO_PENDING, reader_->Read(buf, buf->size(),
                                               read_callback.callback()));
  EXPECT_EQ(buf->size(), read_callback.WaitForResult());
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_TRUE(std::equal(file_data_.begin(), file_data_.end(), buf->data()));
  // Try to read again.
  EXPECT_EQ(0, reader_->Read(buf, buf->size(), read_callback.callback()));
}

TEST_F(UploadFileSystemFileElementReaderTest, ReadPartially) {
  const size_t kHalfSize = file_data_.size() / 2;
  ASSERT_EQ(file_data_.size(), kHalfSize * 2);

  scoped_refptr<net::IOBufferWithSize> buf =
      new net::IOBufferWithSize(kHalfSize);

  net::TestCompletionCallback read_callback1;
  ASSERT_EQ(net::ERR_IO_PENDING, reader_->Read(buf, buf->size(),
                                               read_callback1.callback()));
  EXPECT_EQ(buf->size(), read_callback1.WaitForResult());
  EXPECT_EQ(file_data_.size() - buf->size(), reader_->BytesRemaining());
  EXPECT_TRUE(std::equal(file_data_.begin(), file_data_.begin() + kHalfSize,
                         buf->data()));

  net::TestCompletionCallback read_callback2;
  EXPECT_EQ(net::ERR_IO_PENDING, reader_->Read(buf, buf->size(),
                                               read_callback2.callback()));
  EXPECT_EQ(buf->size(), read_callback2.WaitForResult());
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_TRUE(std::equal(file_data_.begin() + kHalfSize, file_data_.end(),
                         buf->data()));
}

TEST_F(UploadFileSystemFileElementReaderTest, ReadTooMuch) {
  const size_t kTooLargeSize = file_data_.size() * 2;
  scoped_refptr<net::IOBufferWithSize> buf =
      new net::IOBufferWithSize(kTooLargeSize);
  net::TestCompletionCallback read_callback;
  ASSERT_EQ(net::ERR_IO_PENDING, reader_->Read(buf, buf->size(),
                                               read_callback.callback()));
  EXPECT_EQ(static_cast<int>(file_data_.size()), read_callback.WaitForResult());
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_TRUE(std::equal(file_data_.begin(), file_data_.end(), buf->data()));
}

TEST_F(UploadFileSystemFileElementReaderTest, MultipleInit) {
  scoped_refptr<net::IOBufferWithSize> buf =
      new net::IOBufferWithSize(file_data_.size());

  // Read all.
  net::TestCompletionCallback read_callback1;
  ASSERT_EQ(net::ERR_IO_PENDING, reader_->Read(buf, buf->size(),
                                               read_callback1.callback()));
  EXPECT_EQ(buf->size(), read_callback1.WaitForResult());
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_TRUE(std::equal(file_data_.begin(), file_data_.end(), buf->data()));

  // Call Init() again to reset the state.
  net::TestCompletionCallback init_callback;
  ASSERT_EQ(net::ERR_IO_PENDING, reader_->Init(init_callback.callback()));
  EXPECT_EQ(net::OK, init_callback.WaitForResult());
  EXPECT_EQ(file_data_.size(), reader_->GetContentLength());
  EXPECT_EQ(file_data_.size(), reader_->BytesRemaining());

  // Read again.
  net::TestCompletionCallback read_callback2;
  ASSERT_EQ(net::ERR_IO_PENDING, reader_->Read(buf, buf->size(),
                                               read_callback2.callback()));
  EXPECT_EQ(buf->size(), read_callback2.WaitForResult());
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_TRUE(std::equal(file_data_.begin(), file_data_.end(), buf->data()));
}

TEST_F(UploadFileSystemFileElementReaderTest, InitDuringAsyncOperation) {
  scoped_refptr<net::IOBufferWithSize> buf =
      new net::IOBufferWithSize(file_data_.size());

  // Start reading all.
  net::TestCompletionCallback read_callback1;
  EXPECT_EQ(net::ERR_IO_PENDING, reader_->Read(buf, buf->size(),
                                               read_callback1.callback()));

  // Call Init to cancel the previous read.
  net::TestCompletionCallback init_callback1;
  EXPECT_EQ(net::ERR_IO_PENDING, reader_->Init(init_callback1.callback()));

  // Call Init again to cancel the previous init.
  net::TestCompletionCallback init_callback2;
  EXPECT_EQ(net::ERR_IO_PENDING, reader_->Init(init_callback2.callback()));
  EXPECT_EQ(net::OK, init_callback2.WaitForResult());
  EXPECT_EQ(file_data_.size(), reader_->GetContentLength());
  EXPECT_EQ(file_data_.size(), reader_->BytesRemaining());

  // Read half.
  scoped_refptr<net::IOBufferWithSize> buf2 =
      new net::IOBufferWithSize(file_data_.size() / 2);
  net::TestCompletionCallback read_callback2;
  EXPECT_EQ(net::ERR_IO_PENDING, reader_->Read(buf2, buf2->size(),
                                               read_callback2.callback()));
  EXPECT_EQ(buf2->size(), read_callback2.WaitForResult());
  EXPECT_EQ(file_data_.size() - buf2->size(), reader_->BytesRemaining());
  EXPECT_TRUE(std::equal(file_data_.begin(), file_data_.begin() + buf2->size(),
                         buf2->data()));

  // Make sure callbacks are not called for cancelled operations.
  EXPECT_FALSE(read_callback1.have_result());
  EXPECT_FALSE(init_callback1.have_result());
}

TEST_F(UploadFileSystemFileElementReaderTest, Range) {
  const int kOffset = 2;
  const int kLength = file_data_.size() - kOffset * 3;
  reader_.reset(new UploadFileSystemFileElementReader(
      file_system_context_, file_url_, kOffset, kLength, base::Time()));
  net::TestCompletionCallback init_callback;
  ASSERT_EQ(net::ERR_IO_PENDING, reader_->Init(init_callback.callback()));
  EXPECT_EQ(net::OK, init_callback.WaitForResult());
  EXPECT_EQ(static_cast<uint64>(kLength), reader_->GetContentLength());
  EXPECT_EQ(static_cast<uint64>(kLength), reader_->BytesRemaining());
  scoped_refptr<net::IOBufferWithSize> buf = new net::IOBufferWithSize(kLength);
  net::TestCompletionCallback read_callback;
  ASSERT_EQ(net::ERR_IO_PENDING, reader_->Read(buf, buf->size(),
                                               read_callback.callback()));
  EXPECT_EQ(kLength, read_callback.WaitForResult());
  EXPECT_TRUE(std::equal(file_data_.begin() + kOffset,
                         file_data_.begin() + kOffset + kLength,
                         buf->data()));
}

TEST_F(UploadFileSystemFileElementReaderTest, FileChanged) {
  // Expect one second before the actual modification time to simulate change.
  const base::Time expected_modification_time =
      file_modification_time_ - base::TimeDelta::FromSeconds(1);
  reader_.reset(new UploadFileSystemFileElementReader(
      file_system_context_, file_url_, 0, kuint64max,
      expected_modification_time));
  net::TestCompletionCallback init_callback;
  ASSERT_EQ(net::ERR_IO_PENDING, reader_->Init(init_callback.callback()));
  EXPECT_EQ(net::ERR_UPLOAD_FILE_CHANGED, init_callback.WaitForResult());
}

TEST_F(UploadFileSystemFileElementReaderTest, WrongURL) {
  const GURL wrong_url = GetFileSystemURL("wrong_file_name.dat");
  reader_.reset(new UploadFileSystemFileElementReader(
      file_system_context_, wrong_url, 0, kuint64max, base::Time()));
  net::TestCompletionCallback init_callback;
  ASSERT_EQ(net::ERR_IO_PENDING, reader_->Init(init_callback.callback()));
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, init_callback.WaitForResult());
}

}  // namespace fileapi
