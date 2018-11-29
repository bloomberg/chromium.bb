// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/local_file_operations.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/queue.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class LocalFileOperationsTest : public testing::Test {
 public:
  LocalFileOperationsTest();

  // testing::Test implementation.
  void SetUp() override;
  void TearDown() override;

 protected:
  const base::FilePath kTestFilename =
      base::FilePath::FromUTF8Unsafe("test-file.txt");
  const base::FilePath kTestFilenameSecondary =
      base::FilePath::FromUTF8Unsafe("test-file (1).txt");
  const std::string kTestDataOne = "this is the first test string";
  const std::string kTestDataTwo = "this is the second test string";
  const std::string kTestDataThree = "this is the third test string";

  base::FilePath TestDir();
  void WriteFile(const base::FilePath& filename,
                 base::queue<std::string> chunks,
                 bool close);
  void OnFileCreated(base::queue<std::string> chunks,
                     bool close,
                     base::Optional<protocol::FileTransfer_Error> error,
                     std::unique_ptr<FileOperations::Writer> writer);
  void OnWriteComplete(base::queue<std::string> remaining_chunks,
                       bool close,
                       base::Optional<protocol::FileTransfer_Error> error);
  void OnCloseComplete(base::Optional<protocol::FileTransfer_Error> error);

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::ScopedPathOverride scoped_path_override_;
  std::unique_ptr<FileOperations> file_operations_;
  std::unique_ptr<FileOperations::Writer> file_writer_;
  bool operation_completed_ = false;
};

LocalFileOperationsTest::LocalFileOperationsTest()
    : scoped_task_environment_(
          base::test::ScopedTaskEnvironment::MainThreadType::DEFAULT,
          base::test::ScopedTaskEnvironment::ExecutionMode::QUEUED),
      // Points DIR_USER_DESKTOP at a scoped temporary directory.
      scoped_path_override_(base::DIR_USER_DESKTOP),
      file_operations_(std::make_unique<LocalFileOperations>()) {}

void LocalFileOperationsTest::SetUp() {}

void LocalFileOperationsTest::TearDown() {}

base::FilePath LocalFileOperationsTest::TestDir() {
  base::FilePath result;
  EXPECT_TRUE(base::PathService::Get(base::DIR_USER_DESKTOP, &result));
  return result;
}

void LocalFileOperationsTest::WriteFile(const base::FilePath& filename,
                                        base::queue<std::string> chunks,
                                        bool close) {
  operation_completed_ = false;
  file_operations_->WriteFile(
      filename,
      base::BindOnce(&LocalFileOperationsTest::OnFileCreated,
                     base::Unretained(this), std::move(chunks), close));
}

void LocalFileOperationsTest::OnFileCreated(
    base::queue<std::string> chunks,
    bool close,
    base::Optional<protocol::FileTransfer_Error> error,
    std::unique_ptr<FileOperations::Writer> writer) {
  file_writer_ = std::move(writer);
  OnWriteComplete(std::move(chunks), close, error);
}

void LocalFileOperationsTest::OnWriteComplete(
    base::queue<std::string> remaining_chunks,
    bool close,
    base::Optional<protocol::FileTransfer_Error> error) {
  ASSERT_FALSE(error);
  if (!remaining_chunks.empty()) {
    std::string next_chunk = std::move(remaining_chunks.front());
    remaining_chunks.pop();
    file_writer_->WriteChunk(
        std::move(next_chunk),
        base::BindOnce(&LocalFileOperationsTest::OnWriteComplete,
                       base::Unretained(this), std::move(remaining_chunks),
                       close));
  } else if (close) {
    file_writer_->Close(base::BindOnce(
        &LocalFileOperationsTest::OnCloseComplete, base::Unretained(this)));
  } else {
    operation_completed_ = true;
  }
}

void LocalFileOperationsTest::OnCloseComplete(
    base::Optional<protocol::FileTransfer_Error> error) {
  ASSERT_FALSE(error);
  operation_completed_ = true;
}

// Verifies that a file consisting of three chunks can be written successfully.
TEST_F(LocalFileOperationsTest, WritesThreeChunks) {
  WriteFile(
      kTestFilename,
      base::queue<std::string>({kTestDataOne, kTestDataTwo, kTestDataThree}),
      true /* close */);
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(operation_completed_);

  std::string actual_file_data;
  ASSERT_TRUE(base::ReadFileToString(TestDir().Append(kTestFilename),
                                     &actual_file_data));
  ASSERT_EQ(kTestDataOne + kTestDataTwo + kTestDataThree, actual_file_data);
}

// Verifies that LocalFileOperations will write to a file named
// "file (1).txt" if "file.txt" already exists.
TEST_F(LocalFileOperationsTest, RenamesFileIfExists) {
  WriteFile(kTestFilename, base::queue<std::string>({kTestDataOne}),
            true /* close */);
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(operation_completed_);

  WriteFile(kTestFilename, base::queue<std::string>({kTestDataTwo}),
            true /* close */);
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(operation_completed_);

  std::string actual_file_data_one;
  ASSERT_TRUE(base::ReadFileToString(TestDir().Append(kTestFilename),
                                     &actual_file_data_one));
  ASSERT_EQ(kTestDataOne, actual_file_data_one);
  std::string actual_file_data_two;
  ASSERT_TRUE(base::ReadFileToString(TestDir().Append(kTestFilenameSecondary),
                                     &actual_file_data_two));
  ASSERT_EQ(kTestDataTwo, actual_file_data_two);
}

// Verifies that calling Cancel() deletes the temporary file.
TEST_F(LocalFileOperationsTest, CancelDeletesTemp) {
  WriteFile(
      kTestFilename,
      base::queue<std::string>({kTestDataOne, kTestDataTwo, kTestDataThree}),
      false /* close */);
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(operation_completed_);

  file_writer_->Cancel();
  scoped_task_environment_.RunUntilIdle();

  ASSERT_TRUE(base::IsDirectoryEmpty(TestDir()));
}

// Verifies that Cancel() can be called while an operation is pending.
TEST_F(LocalFileOperationsTest, CancelsWhileOperationPending) {
  WriteFile(kTestFilename, base::queue<std::string>({kTestDataOne}),
            false /* close */);
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(operation_completed_);

  file_writer_->WriteChunk(kTestDataTwo, base::DoNothing());
  file_writer_->Cancel();
  scoped_task_environment_.RunUntilIdle();

  ASSERT_TRUE(base::IsDirectoryEmpty(TestDir()));
}

// Verifies that dropping an unclosed FileWriter is the same as canceling it.
TEST_F(LocalFileOperationsTest, CancelsWhenDestructed) {
  WriteFile(
      kTestFilename,
      base::queue<std::string>({kTestDataOne, kTestDataTwo, kTestDataThree}),
      false /* close */);
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(operation_completed_);

  file_writer_->WriteChunk(kTestDataTwo, base::DoNothing());
  file_writer_.reset();
  scoped_task_environment_.RunUntilIdle();

  ASSERT_TRUE(base::IsDirectoryEmpty(TestDir()));
}

}  // namespace remoting
