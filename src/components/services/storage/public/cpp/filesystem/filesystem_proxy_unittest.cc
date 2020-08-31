// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/filesystem/filesystem_proxy.h"

#include <memory>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "components/services/storage/public/cpp/filesystem/file_error_or.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_impl.h"
#include "components/services/storage/public/mojom/filesystem/directory.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

namespace {

constexpr char kFile1Contents[] = "Hello, world!";

using ::testing::UnorderedElementsAre;

std::string ReadFileContents(base::File* file) {
  std::vector<uint8_t> buffer(file->GetLength());
  CHECK(file->ReadAndCheck(0, buffer));
  return std::string(buffer.begin(), buffer.end());
}

}  // namespace

class FilesystemProxyTest : public testing::TestWithParam<bool> {
 public:
  const base::FilePath kFile1{FILE_PATH_LITERAL("file1")};
  const base::FilePath kFile2{FILE_PATH_LITERAL("file2")};
  const base::FilePath kDir1{FILE_PATH_LITERAL("dir1")};
  const base::FilePath kDir1File1{FILE_PATH_LITERAL("dir1file1")};
  const base::FilePath kDir1File2{FILE_PATH_LITERAL("dir1file2")};
  const base::FilePath kDir1Dir1{FILE_PATH_LITERAL("dir1dir1")};
  const base::FilePath kDir2{FILE_PATH_LITERAL("dir2")};
  const base::FilePath kDir2File1{FILE_PATH_LITERAL("dir2file1")};

  FilesystemProxyTest() = default;

  void SetUp() override {
    CHECK(temp_dir_.CreateUniqueTempDir());
    const base::FilePath root = temp_dir_.GetPath();

    // Populate the temporary root with some files and subdirectories.
    CHECK(base::CreateDirectory(root.Append(kDir1)));
    CHECK(base::CreateDirectory(root.Append(kDir1).Append(kDir1Dir1)));
    CHECK(base::CreateDirectory(root.Append(kDir2)));
    CHECK(base::WriteFile(root.Append(kFile1), kFile1Contents,
                          base::size(kFile1Contents) - 1));
    CHECK(base::WriteFile(root.Append(kFile2), " ", 1));
    CHECK(base::WriteFile(root.Append(kDir1).Append(kDir1File1), " ", 1));
    CHECK(base::WriteFile(root.Append(kDir1).Append(kDir1File2), " ", 1));
    CHECK(base::WriteFile(root.Append(kDir2).Append(kDir1File2), " ", 1));

    if (UseRestrictedFilesystem()) {
      // Run a remote FilesystemImpl on a background thread to exercise
      // restricted FilesystemProxy behavior.
      mojo::PendingRemote<mojom::Directory> remote;
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
          ->PostTask(FROM_HERE,
                     base::BindOnce(
                         [](const base::FilePath& root,
                            mojo::PendingReceiver<mojom::Directory> receiver) {
                           mojo::MakeSelfOwnedReceiver(
                               std::make_unique<FilesystemImpl>(root),
                               std::move(receiver));
                         },
                         root, remote.InitWithNewPipeAndPassReceiver()));
      proxy_ = std::make_unique<FilesystemProxy>(
          FilesystemProxy::RESTRICTED, root, std::move(remote),
          base::ThreadPool::CreateSequencedTaskRunner({}));
    } else {
      proxy_ = std::make_unique<FilesystemProxy>(FilesystemProxy::UNRESTRICTED,
                                                 root);
    }
  }

  void TearDown() override {
    proxy_.reset();
    CHECK(temp_dir_.Delete());
  }

  base::FilePath GetTestRoot() { return temp_dir_.GetPath(); }

  FilesystemProxy& proxy() { return *proxy_; }

  base::FilePath MakeAbsolute(const base::FilePath& path) {
    DCHECK(!path.IsAbsolute());
    return GetTestRoot().Append(path);
  }

  std::string ReadFileContentsAtPath(const base::FilePath& path) {
    FileErrorOr<base::File> result =
        proxy().OpenFile(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    return ReadFileContents(&result.value());
  }

 private:
  bool UseRestrictedFilesystem() { return GetParam(); }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<FilesystemProxy> proxy_;
};

TEST_P(FilesystemProxyTest, PathExists) {
  EXPECT_TRUE(proxy().PathExists(kFile1));
  EXPECT_TRUE(proxy().PathExists(kDir1));
  EXPECT_TRUE(proxy().PathExists(kDir1.Append(kDir1File1)));
  EXPECT_FALSE(proxy().PathExists(kDir2.Append(kFile2)));
}

TEST_P(FilesystemProxyTest, GetDirectoryEntries) {
  FileErrorOr<std::vector<base::FilePath>> result = proxy().GetDirectoryEntries(
      base::FilePath(), FilesystemProxy::DirectoryEntryType::kFilesOnly);
  ASSERT_FALSE(result.is_error());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(MakeAbsolute(kFile1), MakeAbsolute(kFile2)));

  result = proxy().GetDirectoryEntries(
      base::FilePath(),
      FilesystemProxy::DirectoryEntryType::kFilesAndDirectories);
  ASSERT_FALSE(result.is_error());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(MakeAbsolute(kFile1), MakeAbsolute(kFile2),
                                   MakeAbsolute(kDir1), MakeAbsolute(kDir2)));

  result = proxy().GetDirectoryEntries(
      kDir1, FilesystemProxy::DirectoryEntryType::kFilesOnly);
  ASSERT_FALSE(result.is_error());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(MakeAbsolute(kDir1.Append(kDir1File1)),
                                   MakeAbsolute(kDir1.Append(kDir1File2))));

  result = proxy().GetDirectoryEntries(
      kDir1, FilesystemProxy::DirectoryEntryType::kFilesAndDirectories);
  ASSERT_FALSE(result.is_error());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(MakeAbsolute(kDir1.Append(kDir1File1)),
                                   MakeAbsolute(kDir1.Append(kDir1File2)),
                                   MakeAbsolute(kDir1.Append(kDir1Dir1))));

  EXPECT_EQ(
      base::File::FILE_ERROR_NOT_FOUND,
      proxy()
          .GetDirectoryEntries(base::FilePath(FILE_PATH_LITERAL("nope")),
                               FilesystemProxy::DirectoryEntryType::kFilesOnly)
          .error());
}

TEST_P(FilesystemProxyTest, OpenFileOpenIfExists) {
  const base::FilePath kNewFilename{FILE_PATH_LITERAL("new_file")};
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            proxy()
                .OpenFile(kNewFilename, base::File::FLAG_OPEN |
                                            base::File::FLAG_READ |
                                            base::File::FLAG_WRITE)
                .error());

  FileErrorOr<base::File> file1 =
      proxy().OpenFile(kFile1, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                   base::File::FLAG_WRITE);
  EXPECT_FALSE(file1.is_error());
  EXPECT_EQ(kFile1Contents, ReadFileContents(&file1.value()));
}

TEST_P(FilesystemProxyTest, OpenFileCreateAndOpenOnlyIfNotExists) {
  EXPECT_EQ(
      base::File::FILE_ERROR_EXISTS,
      proxy()
          .OpenFile(kFile1, base::File::FLAG_CREATE | base::File::FLAG_READ |
                                base::File::FLAG_WRITE)
          .error());

  const base::FilePath kNewFilename{FILE_PATH_LITERAL("new_file")};
  FileErrorOr<base::File> new_file = proxy().OpenFile(
      kNewFilename,
      base::File::FLAG_CREATE | base::File::FLAG_READ | base::File::FLAG_WRITE);
  ASSERT_FALSE(new_file.is_error());
  EXPECT_EQ("", ReadFileContents(&new_file.value()));

  const std::string kData = "yeet";
  EXPECT_TRUE(
      new_file->WriteAndCheck(0, base::as_bytes(base::make_span(kData))));
  EXPECT_EQ(kData, ReadFileContents(&new_file.value()));
}

TEST_P(FilesystemProxyTest, OpenFileAlwaysOpen) {
  FileErrorOr<base::File> file1 = proxy().OpenFile(
      kFile1, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
                  base::File::FLAG_WRITE);
  ASSERT_FALSE(file1.is_error());
  EXPECT_TRUE(file1->IsValid());
  EXPECT_EQ(kFile1Contents, ReadFileContents(&file1.value()));

  const base::FilePath kNewFilename{FILE_PATH_LITERAL("new_file")};
  FileErrorOr<base::File> new_file = proxy().OpenFile(
      kNewFilename, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
                        base::File::FLAG_WRITE);
  ASSERT_FALSE(new_file.is_error());
  EXPECT_TRUE(new_file->IsValid());
  EXPECT_EQ("", ReadFileContents(&new_file.value()));
}

TEST_P(FilesystemProxyTest, OpenFileAlwaysCreate) {
  FileErrorOr<base::File> file1 = proxy().OpenFile(
      kFile1, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                  base::File::FLAG_WRITE);
  ASSERT_FALSE(file1.is_error());
  EXPECT_TRUE(file1->IsValid());
  EXPECT_EQ("", ReadFileContents(&file1.value()));

  const base::FilePath kNewFilename{FILE_PATH_LITERAL("new_file")};
  FileErrorOr<base::File> new_file = proxy().OpenFile(
      kNewFilename, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                        base::File::FLAG_WRITE);
  ASSERT_FALSE(new_file.is_error());
  EXPECT_TRUE(new_file->IsValid());
  EXPECT_EQ("", ReadFileContents(&new_file.value()));
}

TEST_P(FilesystemProxyTest, OpenFileOpenIfExistsAndTruncate) {
  const base::FilePath kNewFilename{FILE_PATH_LITERAL("new_file")};
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            proxy()
                .OpenFile(kNewFilename, base::File::FLAG_OPEN_TRUNCATED |
                                            base::File::FLAG_READ |
                                            base::File::FLAG_WRITE)
                .error());

  FileErrorOr<base::File> file1 = proxy().OpenFile(
      kFile1, base::File::FLAG_OPEN_TRUNCATED | base::File::FLAG_READ |
                  base::File::FLAG_WRITE);
  ASSERT_FALSE(file1.is_error());
  EXPECT_TRUE(file1->IsValid());
  EXPECT_EQ("", ReadFileContents(&file1.value()));
}

TEST_P(FilesystemProxyTest, OpenFileReadOnly) {
  FileErrorOr<base::File> file =
      proxy().OpenFile(kFile1, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_FALSE(file.is_error());
  EXPECT_TRUE(file->IsValid());

  // Writes should fail.
  EXPECT_FALSE(file->WriteAtCurrentPosAndCheck(
      base::as_bytes(base::make_span("doesn't matter"))));
  EXPECT_EQ(kFile1Contents, ReadFileContents(&file.value()));
}

TEST_P(FilesystemProxyTest, OpenFileWriteOnly) {
  FileErrorOr<base::File> file = proxy().OpenFile(
      kFile2, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_FALSE(file.is_error());
  EXPECT_TRUE(file->IsValid());

  const std::string kData{"files can have a little data, as a treat"};
  EXPECT_TRUE(file->WriteAndCheck(0, base::as_bytes(base::make_span(kData))));

  // Reading from this handle should fail.
  std::vector<uint8_t> data;
  EXPECT_FALSE(file->ReadAndCheck(0, data));
  file->Close();

  // But the file contents should still have been written.
  EXPECT_EQ(kData, ReadFileContentsAtPath(kFile2));
}

TEST_P(FilesystemProxyTest, OpenFileAppendOnly) {
  const base::FilePath kFile3{FILE_PATH_LITERAL("file3")};
  FileErrorOr<base::File> file = proxy().OpenFile(
      kFile3, base::File::FLAG_CREATE | base::File::FLAG_APPEND);
  ASSERT_FALSE(file.is_error());
  EXPECT_TRUE(file->IsValid());

  const std::string kData{"files can have a little data, as a treat"};
  EXPECT_TRUE(
      file->WriteAtCurrentPosAndCheck(base::as_bytes(base::make_span(kData))));

  // Attempt to write somewhere other than the end of the file. The offset
  // should be ignored and the data should be appended instead.
  const std::string kMoreData{"!"};
  EXPECT_TRUE(
      file->WriteAndCheck(0, base::as_bytes(base::make_span(kMoreData))));

  // Reading should still fail.
  std::vector<uint8_t> data;
  EXPECT_FALSE(file->ReadAndCheck(0, data));
  file->Close();

  // But we should have all the appended data in the file.
  EXPECT_EQ(kData + kMoreData, ReadFileContentsAtPath(kFile3));
}

TEST_P(FilesystemProxyTest, RemoveFile) {
  FileErrorOr<base::File> file =
      proxy().OpenFile(kFile1, base::File::FLAG_OPEN | base ::File::FLAG_READ);
  ASSERT_FALSE(file.is_error());
  EXPECT_TRUE(file->IsValid());
  file->Close();

  EXPECT_TRUE(proxy().RemoveFile(kFile1));
  file =
      proxy().OpenFile(kFile1, base::File::FLAG_OPEN | base ::File::FLAG_READ);
  EXPECT_TRUE(file.is_error());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, file.error());
}

TEST_P(FilesystemProxyTest, CreateAndRemoveDirectory) {
  const base::FilePath kNewDirectoryName{FILE_PATH_LITERAL("new_dir")};
  EXPECT_EQ(base::File::FILE_OK, proxy().CreateDirectory(kNewDirectoryName));
  EXPECT_TRUE(proxy().RemoveDirectory(kNewDirectoryName));
  EXPECT_FALSE(proxy().RemoveDirectory(kNewDirectoryName));
}

TEST_P(FilesystemProxyTest, GetFileInfo) {
  base::Optional<base::File::Info> file1_info = proxy().GetFileInfo(kFile1);
  ASSERT_TRUE(file1_info.has_value());
  EXPECT_FALSE(file1_info->is_directory);
  EXPECT_EQ(static_cast<int>(base::size(kFile1Contents) - 1), file1_info->size);

  base::Optional<base::File::Info> dir1_info = proxy().GetFileInfo(kDir1);
  ASSERT_TRUE(dir1_info.has_value());
  EXPECT_TRUE(dir1_info->is_directory);

  base::Optional<base::File::Info> dir1_file1_info =
      proxy().GetFileInfo(kDir1.Append(kDir1File1));
  ASSERT_TRUE(dir1_file1_info.has_value());
  EXPECT_FALSE(dir1_file1_info->is_directory);
  EXPECT_EQ(1, dir1_file1_info->size);

  const base::FilePath kBadFilename{FILE_PATH_LITERAL("bad_file")};
  EXPECT_FALSE(proxy().GetFileInfo(kBadFilename).has_value());
}

TEST_P(FilesystemProxyTest, RenameFile) {
  const base::FilePath kNewFilename{FILE_PATH_LITERAL("new_file")};
  EXPECT_EQ(base::File::FILE_OK, proxy().RenameFile(kFile1, kNewFilename));

  EXPECT_EQ(
      base::File::FILE_ERROR_NOT_FOUND,
      proxy()
          .OpenFile(kFile1, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                base::File::FLAG_WRITE)
          .error());

  FileErrorOr<base::File> new_file = proxy().OpenFile(
      kNewFilename,
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);
  ASSERT_FALSE(new_file.is_error());
  EXPECT_TRUE(new_file->IsValid());
  EXPECT_EQ(kFile1Contents, ReadFileContents(&new_file.value()));
}

TEST_P(FilesystemProxyTest, RenameNonExistentFile) {
  const base::FilePath kBadFilename{FILE_PATH_LITERAL("bad_file")};
  const base::FilePath kNewFilename{FILE_PATH_LITERAL("new_file")};
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            proxy().RenameFile(kBadFilename, kNewFilename));
}

TEST_P(FilesystemProxyTest, LockFile) {
  const base::FilePath kLockFilename{FILE_PATH_LITERAL("lox")};
  FileErrorOr<std::unique_ptr<FilesystemProxy::FileLock>> result =
      proxy().LockFile(kLockFilename);
  ASSERT_FALSE(result.is_error());
  EXPECT_NE(nullptr, result.value());

  FileErrorOr<std::unique_ptr<FilesystemProxy::FileLock>> result2 =
      proxy().LockFile(kLockFilename);
  EXPECT_TRUE(result2.is_error());
  EXPECT_EQ(base::File::FILE_ERROR_IN_USE, result2.error());

  // Synchronously release so we can re-acquire the lock.
  EXPECT_EQ(base::File::Error::FILE_OK, result.value()->Release());

  result2 = proxy().LockFile(kLockFilename);
  ASSERT_FALSE(result2.is_error());
  EXPECT_NE(nullptr, result2.value());

  // Test that destruction also implicitly releases the lock.
  result2 = base::File::FILE_ERROR_FAILED;

  // And once again we should be able to reacquire the lock.
  result = proxy().LockFile(kLockFilename);
  ASSERT_FALSE(result.is_error());
  EXPECT_NE(nullptr, result.value());
}

TEST_P(FilesystemProxyTest, AbsolutePathEqualToRoot) {
  // Verifies that if a delegate is given an absolute path identical to its
  // root path, it is correctly resolved to an empty relative path and can
  // operate correctly.
  FileErrorOr<std::vector<base::FilePath>> result = proxy().GetDirectoryEntries(
      GetTestRoot(), FilesystemProxy::DirectoryEntryType::kFilesAndDirectories);
  ASSERT_FALSE(result.is_error());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(MakeAbsolute(kFile1), MakeAbsolute(kFile2),
                                   MakeAbsolute(kDir1), MakeAbsolute(kDir2)));
}

TEST_P(FilesystemProxyTest, AbsolutePathWithinRoot) {
  // Verifies that if a delegate is given an absolute path which falls within
  // its root path, it is correctly resolved to a relative path suitable for use
  // with the Directory IPC interface.
  FileErrorOr<std::vector<base::FilePath>> result = proxy().GetDirectoryEntries(
      GetTestRoot().Append(kDir1),
      FilesystemProxy::DirectoryEntryType::kFilesAndDirectories);
  ASSERT_FALSE(result.is_error());
  EXPECT_THAT(result.value(),
              UnorderedElementsAre(MakeAbsolute(kDir1.Append(kDir1File1)),
                                   MakeAbsolute(kDir1.Append(kDir1File2)),
                                   MakeAbsolute(kDir1.Append(kDir1Dir1))));
}

INSTANTIATE_TEST_SUITE_P(, FilesystemProxyTest, testing::Bool());

}  // namespace storage
