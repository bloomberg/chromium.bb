// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_file_handle_impl.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/browser/file_system_access/fixed_file_system_access_permission_grant.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using blink::mojom::PermissionStatus;
using storage::FileSystemURL;

class FileSystemAccessFileHandleImplTest : public testing::Test {
 public:
  FileSystemAccessFileHandleImplTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());

    file_system_context_ = storage::CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, dir_.GetPath());

    test_file_url_ = file_system_context_->CreateCrackedFileSystemURL(
        test_src_origin_, storage::kFileSystemTypeTest,
        base::FilePath::FromUTF8Unsafe("test"));

    ASSERT_EQ(base::File::FILE_OK,
              storage::AsyncFileTestHelper::CreateFile(
                  file_system_context_.get(), test_file_url_));

    chrome_blob_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    chrome_blob_context_->InitializeOnIOThread(base::FilePath(),
                                               base::FilePath(), nullptr);

    manager_ = base::MakeRefCounted<FileSystemAccessManagerImpl>(
        file_system_context_, chrome_blob_context_,
        /*permission_context=*/nullptr,
        /*off_the_record=*/false);

    handle_ = std::make_unique<FileSystemAccessFileHandleImpl>(
        manager_.get(),
        FileSystemAccessManagerImpl::BindingContext(
            test_src_origin_, test_src_url_, /*process_id=*/1),
        test_file_url_,
        FileSystemAccessManagerImpl::SharedHandleState(
            allow_grant_, allow_grant_, /*file_system=*/{}));
  }

  void TearDown() override { task_environment_.RunUntilIdle(); }

  std::string ReadFile(const FileSystemURL& url) {
    std::unique_ptr<storage::FileStreamReader> reader =
        file_system_context_->CreateFileStreamReader(
            url, 0, std::numeric_limits<int64_t>::max(), base::Time());
    std::string result;
    while (true) {
      auto buf = base::MakeRefCounted<net::IOBufferWithSize>(4096);
      net::TestCompletionCallback callback;
      int rv = reader->Read(buf.get(), buf->size(), callback.callback());
      if (rv == net::ERR_IO_PENDING)
        rv = callback.WaitForResult();
      EXPECT_GE(rv, 0);
      if (rv < 0)
        return "(read failure)";
      if (rv == 0)
        return result;
      result.append(buf->data(), rv);
    }
  }

 protected:
  const GURL test_src_url_ = GURL("http://example.com/foo");
  const url::Origin test_src_origin_ = url::Origin::Create(test_src_url_);

  BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;
  scoped_refptr<FileSystemAccessManagerImpl> manager_;

  FileSystemURL test_file_url_;

  scoped_refptr<FixedFileSystemAccessPermissionGrant> allow_grant_ =
      base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
          FixedFileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
          base::FilePath());
  std::unique_ptr<FileSystemAccessFileHandleImpl> handle_;
};

TEST_F(FileSystemAccessFileHandleImplTest, CreateFileWriterOverLimitNotOK) {
  int max_files = 5;
  handle_->set_max_swap_files_for_testing(max_files);

  const FileSystemURL base_swap_url =
      file_system_context_->CreateCrackedFileSystemURL(
          test_src_origin_, storage::kFileSystemTypeTest,
          base::FilePath::FromUTF8Unsafe("test.crswap"));

  std::vector<mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
      writers;
  for (int i = 0; i < max_files; i++) {
    FileSystemURL swap_url;
    if (i == 0) {
      swap_url = base_swap_url;
    } else {
      swap_url = file_system_context_->CreateCrackedFileSystemURL(
          test_src_origin_, storage::kFileSystemTypeTest,
          base::FilePath::FromUTF8Unsafe(
              base::StringPrintf("test.%d.crswap", i)));
    }

    base::RunLoop loop;
    handle_->CreateFileWriter(
        /*keep_existing_data=*/false,
        /*auto_close=*/false,
        base::BindLambdaForTesting(
            [&](blink::mojom::FileSystemAccessErrorPtr result,
                mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>
                    writer_remote) {
              EXPECT_EQ(blink::mojom::FileSystemAccessStatus::kOk,
                        result->status);
              EXPECT_EQ("", ReadFile(swap_url));
              writers.push_back(std::move(writer_remote));
              loop.Quit();
            }));
    loop.Run();
  }

  base::RunLoop loop;
  handle_->CreateFileWriter(
      /*keep_existing_data=*/false,
      /*auto_close=*/false,
      base::BindLambdaForTesting(
          [&](blink::mojom::FileSystemAccessErrorPtr result,
              mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>
                  writer_remote) {
            EXPECT_EQ(blink::mojom::FileSystemAccessStatus::kOperationFailed,
                      result->status);
            loop.Quit();
          }));
  loop.Run();
}

}  // namespace content
