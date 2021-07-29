// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/share_info_file_stream_adapter.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace arc {

namespace {
const char kURLOrigin[] = "http://remote/";
constexpr int kTestFileSize = 8 * 1024 * 1024;
constexpr int kDefaultBufSize = 32 * 1024;
}  // namespace

class ShareInfoFileStreamAdapterTest : public testing::Test {
 public:
  ShareInfoFileStreamAdapterTest() = default;
  ShareInfoFileStreamAdapterTest(const ShareInfoFileStreamAdapterTest&) =
      delete;
  ShareInfoFileStreamAdapterTest& operator=(
      const ShareInfoFileStreamAdapterTest&) = delete;
  ~ShareInfoFileStreamAdapterTest() override = default;

  void SetUp() override {
    // Setup a temporary directory.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Setup path and temp file for testing.
    test_file_path_ = temp_dir_.GetPath().AppendASCII("test");
    base::File test_file(test_file_path_,
                         base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(test_file.IsValid() && base::PathExists(test_file_path_));
    test_fd_ = base::ScopedFD(test_file.TakePlatformFile());

    file_system_context_ = storage::CreateFileSystemContextForTesting(
        nullptr /*quota_manager_proxy=*/, temp_dir_.GetPath());

    file_system_context_->OpenFileSystem(
        url::Origin::Create(GURL(kURLOrigin)),
        storage::kFileSystemTypeTemporary,
        storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::BindOnce([](const GURL& root_url, const std::string& name,
                          base::File::Error result) {
          ASSERT_EQ(base::File::FILE_OK, result);
        }));
    base::RunLoop().RunUntilIdle();

    // Setup a test file in the file system with random data.
    url_ = file_system_context_->CreateCrackedFileSystemURL(
        url::Origin::Create(GURL(kURLOrigin)),
        storage::kFileSystemTypeTemporary,
        base::FilePath().AppendASCII("test.dat"));
    test_data_ = base::RandBytesAsString(kTestFileSize);

    ASSERT_EQ(base::File::FILE_OK,
              storage::AsyncFileTestHelper::CreateFileWithData(
                  file_system_context_.get(), url_, test_data_.data(),
                  test_data_.size()));
  }

  void TearDown() override { stream_adapter_.reset(); }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath test_file_path_;
  base::ScopedFD test_fd_;
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<ShareInfoFileStreamAdapter> stream_adapter_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  storage::FileSystemURL url_;
  std::string test_data_;
};

TEST_F(ShareInfoFileStreamAdapterTest, ReadEntireStreamAndWriteFile) {
  constexpr int kOffset = 0;
  const int kSize = test_data_.size();
  base::RunLoop run_loop;
  stream_adapter_ = base::MakeRefCounted<ShareInfoFileStreamAdapter>(
      file_system_context_, url_, kOffset, kSize, kDefaultBufSize,
      std::move(test_fd_), base::BindLambdaForTesting([&](bool result) {
        EXPECT_TRUE(result);
        run_loop.Quit();
      }));
  stream_adapter_->StartRunnerForTesting();
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(test_file_path_, &contents));
  EXPECT_EQ(test_data_, contents);
}

TEST_F(ShareInfoFileStreamAdapterTest,
       ReadEntireStreamAndWriteFileSmallBuffer) {
  constexpr int kOffset = 0;
  const int kSize = test_data_.size();
  constexpr int kBufSize = 16 * 1024;
  base::RunLoop run_loop;
  stream_adapter_ = base::MakeRefCounted<ShareInfoFileStreamAdapter>(
      file_system_context_, url_, kOffset, kSize, kBufSize, std::move(test_fd_),
      base::BindLambdaForTesting([&](bool result) {
        EXPECT_TRUE(result);
        run_loop.Quit();
      }));
  stream_adapter_->StartRunnerForTesting();
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(test_file_path_, &contents));
  EXPECT_EQ(test_data_, contents);
}

TEST_F(ShareInfoFileStreamAdapterTest, ReadEntireStreamAndWriteFileTinyBuffer) {
  constexpr int kOffset = 0;
  const int kSize = test_data_.size();
  constexpr int kBufSize = 8 * 1024;
  base::RunLoop run_loop;
  stream_adapter_ = base::MakeRefCounted<ShareInfoFileStreamAdapter>(
      file_system_context_, url_, kOffset, kSize, kBufSize, std::move(test_fd_),
      base::BindLambdaForTesting([&](bool result) {
        EXPECT_TRUE(result);
        run_loop.Quit();
      }));
  stream_adapter_->StartRunnerForTesting();
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(test_file_path_, &contents));
  EXPECT_EQ(test_data_, contents);
}

TEST_F(ShareInfoFileStreamAdapterTest, ReadMidStreamAndWriteFile) {
  constexpr int kOffset = 1024;
  const int kSize = test_data_.size() - 1024;
  base::RunLoop run_loop;
  stream_adapter_ = base::MakeRefCounted<ShareInfoFileStreamAdapter>(
      file_system_context_, url_, kOffset, kSize, kDefaultBufSize,
      std::move(test_fd_), base::BindLambdaForTesting([&](bool result) {
        EXPECT_TRUE(result);
        run_loop.Quit();
      }));
  stream_adapter_->StartRunnerForTesting();
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(test_file_path_, &contents));
  EXPECT_EQ(std::string(test_data_.begin() + kOffset,
                        test_data_.begin() + kOffset + kSize),
            contents);
}

}  // namespace arc
