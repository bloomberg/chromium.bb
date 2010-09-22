// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_operation.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_operation.h"

const int kInvalidRequestId = -1;
const int kFileOperationStatusNotSet = 0;
const int kFileOperationSucceeded = 1;

static int last_request_id = -1;

bool FileExists(FilePath path) {
  return file_util::PathExists(path) && !file_util::DirectoryExists(path);
}

class MockDispatcher : public fileapi::FileSystemCallbackDispatcher {
 public:
  MockDispatcher(int request_id)
      : status_(kFileOperationStatusNotSet),
        request_id_(request_id) {
  }

  virtual void DidFail(base::PlatformFileError status) {
    status_ = status;
  }

  virtual void DidSucceed() {
    status_ = kFileOperationSucceeded;
  }

  virtual void DidReadMetadata(const base::PlatformFileInfo& info) {
    info_ = info;
    status_ = kFileOperationSucceeded;
  }

  virtual void DidReadDirectory(
      const std::vector<base::file_util_proxy::Entry>& entries,
      bool /* has_more */) {
    entries_ = entries;
  }

  virtual void DidOpenFileSystem(const string16&, const FilePath&) {
    NOTREACHED();
  }

  // Helpers for testing.
  int status() const { return status_; }
  int request_id() const { return request_id_; }
  const base::PlatformFileInfo& info() const { return info_; }
  const std::vector<base::file_util_proxy::Entry>& entries() const {
    return entries_;
  }

 private:
  int status_;
  int request_id_;
  base::PlatformFileInfo info_;
  std::vector<base::file_util_proxy::Entry> entries_;
};

class FileSystemOperationTest : public testing::Test {
 public:
  FileSystemOperationTest()
      : request_id_(kInvalidRequestId),
        operation_(NULL) {
    base_.CreateUniqueTempDir();
    EXPECT_TRUE(base_.IsValid());
  }

  fileapi::FileSystemOperation* operation() {
    request_id_ = ++last_request_id;
    mock_dispatcher_ = new MockDispatcher(request_id_);
    operation_.reset(new fileapi::FileSystemOperation(
        mock_dispatcher_, base::MessageLoopProxy::CreateForCurrentThread()));
    return operation_.get();
  }

 protected:
  // Common temp base for all non-existing file/dir path test cases.
  // This is in case a dir on test machine exists. It's better to
  // create temp and then create non-existing file paths under it.
  ScopedTempDir base_;

  int request_id_;
  scoped_ptr<fileapi::FileSystemOperation> operation_;

  // Owned by |operation_|.
  MockDispatcher* mock_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemOperationTest);
};

TEST_F(FileSystemOperationTest, TestMoveFailureSrcDoesntExist) {
  FilePath src(base_.path().Append(FILE_PATH_LITERAL("a")));
  FilePath dest(base_.path().Append(FILE_PATH_LITERAL("b")));
  operation()->Move(src, dest);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}


TEST_F(FileSystemOperationTest, TestMoveFailureContainsPath) {
  ScopedTempDir dir_under_base;
  dir_under_base.CreateUniqueTempDirUnderPath(base_.path());
  operation()->Move(base_.path(), dir_under_base.path());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION,
            mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestMoveFailureSrcDirExistsDestFile) {
  // Src exists and is dir. Dest is a file.
  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());
  FilePath dest_file;
  file_util::CreateTemporaryFileInDir(dest_dir.path(), &dest_file);

  operation()->Move(base_.path(), dest_file);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestMoveFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());
  FilePath nonexisting_file = base_.path().Append(
      FILE_PATH_LITERAL("NonexistingDir")).Append(
          FILE_PATH_LITERAL("NonexistingFile"));;

  operation()->Move(src_dir.path(), nonexisting_file);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestMoveSuccessSrcFileAndOverwrite) {
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());
  FilePath src_file;
  file_util::CreateTemporaryFileInDir(src_dir.path(), &src_file);

  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());
  FilePath dest_file;
  file_util::CreateTemporaryFileInDir(dest_dir.path(), &dest_file);

  operation()->Move(src_file, dest_file);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_dispatcher_->status());
  EXPECT_TRUE(FileExists(dest_file));
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestMoveSuccessSrcFileAndNew) {
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());
  FilePath src_file;
  file_util::CreateTemporaryFileInDir(src_dir.path(), &src_file);

  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());
  FilePath dest_file(dest_dir.path().Append(FILE_PATH_LITERAL("NewFile")));

  operation()->Move(src_file, dest_file);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_dispatcher_->status());
  EXPECT_TRUE(FileExists(dest_file));
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestCopyFailureSrcDoesntExist) {
  FilePath src(base_.path().Append(FILE_PATH_LITERAL("a")));
  FilePath dest(base_.path().Append(FILE_PATH_LITERAL("b")));
  operation()->Copy(src, dest);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestCopyFailureContainsPath) {
  FilePath file_under_base = base_.path().Append(FILE_PATH_LITERAL("b"));
  operation()->Copy(base_.path(), file_under_base);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_FAILED, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestCopyFailureSrcDirExistsDestFile) {
  // Src exists and is dir. Dest is a file.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath dest_file;
  file_util::CreateTemporaryFileInDir(dir.path(), &dest_file);

  operation()->Copy(base_.path(), dest_file);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY,
            mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestCopyFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath src_dir = dir.path();

  FilePath nonexisting(base_.path().Append(FILE_PATH_LITERAL("DontExistDir")));
  file_util::EnsureEndsWithSeparator(&nonexisting);
  FilePath nonexisting_file = nonexisting.Append(
      FILE_PATH_LITERAL("DontExistFile"));

  operation()->Copy(src_dir, nonexisting_file);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestCopySuccessSrcFileAndOverwrite) {
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());
  FilePath src_file;
  file_util::CreateTemporaryFileInDir(src_dir.path(), &src_file);

  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());
  FilePath dest_file;
  file_util::CreateTemporaryFileInDir(dest_dir.path(), &dest_file);

  operation()->Copy(src_file, dest_file);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_dispatcher_->status());
  EXPECT_TRUE(FileExists(dest_file));
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestCopySuccessSrcFileAndNew) {
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());
  FilePath src_file;
  file_util::CreateTemporaryFileInDir(src_dir.path(), &src_file);

  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());
  FilePath dest_file(dest_dir.path().Append(FILE_PATH_LITERAL("NewFile")));

  operation()->Copy(src_file, dest_file);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_dispatcher_->status());
  EXPECT_TRUE(FileExists(dest_file));
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestCopySuccessSrcDir) {
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());

  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());

  operation()->Copy(src_dir.path(), dest_dir.path());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestCopyDestParentExistsSuccess) {
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());
  FilePath src_file;
  file_util::CreateTemporaryFileInDir(src_dir.path(), &src_file);

  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());

  operation()->Copy(src_file, dest_dir.path());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());

  FilePath src_filename(src_file.BaseName());
  EXPECT_TRUE(FileExists(dest_dir.path().Append(src_filename)));
}

TEST_F(FileSystemOperationTest, TestCreateFileFailure) {
  // Already existing file and exclusive true.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath file;

  file_util::CreateTemporaryFileInDir(dir.path(), &file);
  operation()->CreateFile(file, true);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestCreateFileSuccessFileExists) {
  // Already existing file and exclusive false.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath file;
  file_util::CreateTemporaryFileInDir(dir.path(), &file);

  operation()->CreateFile(file, false);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_dispatcher_->status());
  EXPECT_TRUE(FileExists(file));
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestCreateFileSuccessExclusive) {
  // File doesn't exist but exclusive is true.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath file = dir.path().Append(FILE_PATH_LITERAL("FileDoesntExist"));
  operation()->CreateFile(file, true);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_dispatcher_->status());
  EXPECT_TRUE(FileExists(file));
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestCreateFileSuccessFileDoesntExist) {
  // Non existing file.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath file = dir.path().Append(FILE_PATH_LITERAL("FileDoesntExist"));
  operation()->CreateFile(file, false);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest,
       TestCreateDirFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  FilePath nonexisting(base_.path().Append(
      FILE_PATH_LITERAL("DirDoesntExist")));
  file_util::EnsureEndsWithSeparator(&nonexisting);
  FilePath nonexisting_file = nonexisting.Append(
      FILE_PATH_LITERAL("FileDoesntExist"));
  operation()->CreateDirectory(nonexisting_file, false, false);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestCreateDirFailureDirExists) {
  // Exclusive and dir existing at path.
  operation()->CreateDirectory(base_.path(), true, false);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestCreateDirFailureFileExists) {
  // Exclusive true and file existing at path.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath file;
  file_util::CreateTemporaryFileInDir(dir.path(), &file);
  operation()->CreateDirectory(file, true, false);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestCreateDirSuccess) {
  // Dir exists and exclusive is false.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  operation()->CreateDirectory(dir.path(), false, false);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());

  // Dir doesn't exist.
  FilePath nonexisting_dir_path(FILE_PATH_LITERAL("nonexistingdir"));
  operation()->CreateDirectory(nonexisting_dir_path, false, false);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_dispatcher_->status());
  EXPECT_TRUE(file_util::DirectoryExists(nonexisting_dir_path));
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestCreateDirSuccessExclusive) {
  // Dir doesn't exist.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath nonexisting_dir_path(dir.path().Append(
      FILE_PATH_LITERAL("nonexistingdir")));

  operation()->CreateDirectory(nonexisting_dir_path, true, false);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_dispatcher_->status());
  EXPECT_TRUE(file_util::DirectoryExists(nonexisting_dir_path));
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestExistsAndMetadataFailure) {
  FilePath nonexisting_dir_path(base_.path().Append(
      FILE_PATH_LITERAL("nonexistingdir")));
  operation()->GetMetadata(nonexisting_dir_path);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, mock_dispatcher_->status());

  operation()->FileExists(nonexisting_dir_path);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());

  file_util::EnsureEndsWithSeparator(&nonexisting_dir_path);
  operation()->DirectoryExists(nonexisting_dir_path);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestExistsAndMetadataSuccess) {
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());

  operation()->DirectoryExists(dir.path());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());

  operation()->GetMetadata(dir.path());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_dispatcher_->status());
  EXPECT_TRUE(mock_dispatcher_->info().is_directory);
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());

  FilePath file;
  file_util::CreateTemporaryFileInDir(dir.path(), &file);
  operation()->FileExists(file);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());

  operation()->GetMetadata(file);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_dispatcher_->status());
  EXPECT_FALSE(mock_dispatcher_->info().is_directory);
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestReadDirFailure) {
  // Path doesn't exists
    FilePath nonexisting_dir_path(base_.path().Append(
        FILE_PATH_LITERAL("NonExistingDir")));
  file_util::EnsureEndsWithSeparator(&nonexisting_dir_path);
  operation()->ReadDirectory(nonexisting_dir_path);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());

  // File exists.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath file;
  file_util::CreateTemporaryFileInDir(dir.path(), &file);
  operation()->ReadDirectory(file);
  MessageLoop::current()->RunAllPending();
  // TODO(kkanetkar) crbug.com/54309 to change the error code.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestReadDirSuccess) {
  //      parent_dir
  //       |       |
  //  child_dir  child_file
  // Verify reading parent_dir.
  ScopedTempDir parent_dir;
  ASSERT_TRUE(parent_dir.CreateUniqueTempDir());
  FilePath child_file;
  file_util::CreateTemporaryFileInDir(parent_dir.path(), &child_file);
  FilePath child_dir;
  ASSERT_TRUE(file_util::CreateTemporaryDirInDir(
      parent_dir.path(), FILE_PATH_LITERAL("child_dir"), &child_dir));

  operation()->ReadDirectory(parent_dir.path());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kFileOperationStatusNotSet, mock_dispatcher_->status());
  EXPECT_EQ(2u, mock_dispatcher_->entries().size());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());

  for (size_t i = 0; i < mock_dispatcher_->entries().size(); ++i) {
    if (mock_dispatcher_->entries()[i].is_directory) {
      EXPECT_EQ(child_dir.BaseName().value(),
                mock_dispatcher_->entries()[i].name);
    } else {
      EXPECT_EQ(child_file.BaseName().value(),
                mock_dispatcher_->entries()[i].name);
    }
  }
}

TEST_F(FileSystemOperationTest, TestRemoveFailure) {
  // Path doesn't exist.
  FilePath nonexisting(base_.path().Append(
      FILE_PATH_LITERAL("NonExistingDir")));
  file_util::EnsureEndsWithSeparator(&nonexisting);

  operation()->Remove(nonexisting);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());

  // By spec recursive is always false. Non-empty dir should fail.
  //      parent_dir
  //       |       |
  //  child_dir  child_file
  // Verify deleting parent_dir.
  ScopedTempDir parent_dir;
  ASSERT_TRUE(parent_dir.CreateUniqueTempDir());
  FilePath child_file;
  file_util::CreateTemporaryFileInDir(parent_dir.path(), &child_file);
  FilePath child_dir;
  ASSERT_TRUE(file_util::CreateTemporaryDirInDir(
      parent_dir.path(), FILE_PATH_LITERAL("child_dir"), &child_dir));

  operation()->Remove(parent_dir.path());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION,
            mock_dispatcher_->status());
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}

TEST_F(FileSystemOperationTest, TestRemoveSuccess) {
  ScopedTempDir empty_dir;
  ASSERT_TRUE(empty_dir.CreateUniqueTempDir());
  EXPECT_TRUE(file_util::DirectoryExists(empty_dir.path()));

  operation()->Remove(empty_dir.path());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_dispatcher_->status());
  EXPECT_FALSE(file_util::DirectoryExists(empty_dir.path()));
  EXPECT_EQ(request_id_, mock_dispatcher_->request_id());
}
