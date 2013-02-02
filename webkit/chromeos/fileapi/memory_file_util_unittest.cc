// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/chromeos/fileapi/memory_file_util.h"

namespace {
const base::FilePath::CharType kRootPath[] = "/mnt/memory";
const char kTestString[] = "A test string. A test string.";
const char kTestStringLength = arraysize(kTestString) - 1;
}  // namespace

namespace fileapi {

// This test is actually testing MemoryFileUtil — an async in-memory file
// system, based on FileUtilAsync.
class MemoryFileUtilTest : public testing::Test {
 public:
  MemoryFileUtilTest() : max_request_id_(0) {
  }

  ~MemoryFileUtilTest() {
    for (std::map<int, CallbackStatus>::iterator iter = status_map_.begin();
         iter != status_map_.end();
         ++iter) {
      delete iter->second.file_stream;
    }
  }

  void SetUp() {
    file_util_.reset(new MemoryFileUtil(base::FilePath(kRootPath)));
  }

  MemoryFileUtil* file_util() {
    return file_util_.get();
  }

  enum CallbackType {
    CALLBACK_TYPE_ERROR,
    CALLBACK_TYPE_STATUS,
    CALLBACK_TYPE_GET_FILE_INFO,
    CALLBACK_TYPE_OPEN,
    CALLBACK_TYPE_READ_WRITE,
    CALLBACK_TYPE_READ_DIRECTORY
  };

  struct CallbackStatus {
    CallbackStatus()
        : type(CALLBACK_TYPE_ERROR),
          result(base::PLATFORM_FILE_OK),
          file_stream(NULL),
          length(-1),
          completed(false),
          called_after_completed(false),
          called(0) {}

    CallbackType type;
    base::PlatformFileError result;
    base::PlatformFileInfo file_info;
    // This object should be deleted in the test code.
    AsyncFileStream* file_stream;
    int64 length;

    // Following 4 fields only for ReadDirectory.
    FileUtilAsync::FileList entries;
    bool completed;
    bool called_after_completed;
    int called;
  };

  FileUtilAsync::StatusCallback GetStatusCallback(int request_id) {
    return base::Bind(&MemoryFileUtilTest::StatusCallbackImpl,
                      base::Unretained(this),
                      request_id);
  }

  FileUtilAsync::GetFileInfoCallback GetGetFileInfoCallback(
      int request_id) {
    return base::Bind(&MemoryFileUtilTest::GetFileInfoCallback,
                      base::Unretained(this),
                      request_id);
  }

  FileUtilAsync::OpenCallback GetOpenCallback(int request_id) {
    return base::Bind(&MemoryFileUtilTest::OpenCallback,
                      base::Unretained(this),
                      request_id);
  }

  AsyncFileStream::ReadWriteCallback GetReadWriteCallback(int request_id) {
    return base::Bind(&MemoryFileUtilTest::ReadWriteCallbackImpl,
                      base::Unretained(this),
                      request_id);
  }

  FileUtilAsync::ReadDirectoryCallback GetReadDirectoryCallback(
      int request_id) {
    return base::Bind(&MemoryFileUtilTest::ReadDirectoryCallback,
                      base::Unretained(this),
                      request_id);
  }

  int CreateEmptyFile(const base::FilePath& file_path) {
    int request_id = GetNextRequestId();
    file_util_->Create(file_path, GetStatusCallback(request_id));
    return request_id;
  }

  int CreateNonEmptyFile(const base::FilePath& file_path,
                         const char* data,
                         int length) {
    int request_id = GetNextRequestId();
    file_util_->Open(
        file_path,
        base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE,
        base::Bind(&MemoryFileUtilTest::WriteToOpenedFile,
                   base::Unretained(this),
                   request_id, data, length));
    return request_id;
  }

  CallbackType GetStatusType(int request_id) {
    if (status_map_.find(request_id) == status_map_.end())
      return CALLBACK_TYPE_ERROR;
    return status_map_[request_id].type;
  }

  // Return the operation status.
  CallbackStatus& GetStatus(int request_id) {
    return status_map_[request_id];
  }

  int StatusQueueSize() {
    return status_map_.size();
  }

  int GetNextRequestId() {
    return ++max_request_id_;
  }

  void set_read_directory_buffer_size(int size) {
    file_util_->set_read_directory_buffer_size(size);
  }

 private:
  void StatusCallbackImpl(int request_id, PlatformFileError result) {
    CallbackStatus status;
    status.type = CALLBACK_TYPE_STATUS;
    status.result = result;
    status_map_[request_id] = status;
  }

  void GetFileInfoCallback(int request_id,
                           PlatformFileError result,
                           const base::PlatformFileInfo& file_info) {
    CallbackStatus status;
    status.type = CALLBACK_TYPE_GET_FILE_INFO;
    status.result = result;
    status.file_info = file_info;
    status_map_[request_id] = status;
  }

  void OpenCallback(int request_id,
                    PlatformFileError result,
                    AsyncFileStream* file_stream) {
    DCHECK(status_map_.find(request_id) == status_map_.end());
    CallbackStatus status;
    status.type = CALLBACK_TYPE_OPEN;
    status.result = result;
    status.file_stream = file_stream;
    status_map_[request_id] = status;
  }

  void ReadWriteCallbackImpl(int request_id,
                             PlatformFileError result,
                             int64 length) {
    CallbackStatus status;
    status.type = CALLBACK_TYPE_READ_WRITE;
    status.result = result;
    status.length = length;
    status_map_[request_id] = status;
  }

  void ReadDirectoryCallback(int request_id,
                             PlatformFileError result,
                             const FileUtilAsync::FileList& entries,
                             bool completed) {
    if (status_map_.find(request_id) == status_map_.end()) {
      CallbackStatus status;
      status.type = CALLBACK_TYPE_READ_DIRECTORY;
      status.called_after_completed = false;
      status.result = result;
      status.completed = completed;
      status.called = 1;
      status.entries = entries;
      status_map_[request_id] = status;
    } else {
      CallbackStatus& status = status_map_[request_id];
      if (status.completed)
        status.called_after_completed = true;
      status.result = result;
      status.completed = completed;
      ++status.called;
      status.entries.insert(status.entries.begin(), entries.begin(),
                            entries.end());
    }
  }

  void WriteToOpenedFile(int request_id,
                         const char* data,
                         int length,
                         PlatformFileError result,
                         AsyncFileStream* stream) {
    DCHECK(status_map_.find(request_id) == status_map_.end());
    CallbackStatus status;
    status.type = CALLBACK_TYPE_OPEN;
    status.result = result;
    status.file_stream = stream;
    status_map_[request_id] = status;
    stream->Write(data, length, GetReadWriteCallback(GetNextRequestId()));
  }

  scoped_ptr<MemoryFileUtil> file_util_;
  std::map<int, CallbackStatus> status_map_;
  int max_request_id_;

  DISALLOW_COPY_AND_ASSIGN(MemoryFileUtilTest);
};

TEST_F(MemoryFileUtilTest, TestCreateGetFileInfo) {
  const int request_id1 = GetNextRequestId();
  file_util()->GetFileInfo(base::FilePath("/mnt/memory/test.txt"),
                           GetGetFileInfoCallback(request_id1));

  // In case the file system is truely asynchronous, RunAllPending is not
  // enough to wait for answer. In that case the thread should be blocked
  // until the callback is called (ex. use Run() instead here, and call
  // Quit() from callback).
  MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(CALLBACK_TYPE_GET_FILE_INFO, GetStatusType(request_id1));
  CallbackStatus status = GetStatus(request_id1);
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status.result);

  base::Time start_create = base::Time::Now();

  const int request_id2 = GetNextRequestId();
  file_util()->Create(base::FilePath("/mnt/memory/test.txt"),
                      GetStatusCallback(request_id2));
  MessageLoop::current()->RunUntilIdle();
  ASSERT_EQ(CALLBACK_TYPE_STATUS, GetStatusType(request_id2));
  status = GetStatus(request_id2);
  ASSERT_EQ(base::PLATFORM_FILE_OK, status.result);

  const int request_id3 = GetNextRequestId();
  file_util()->GetFileInfo(base::FilePath("/mnt/memory/test.txt"),
                           GetGetFileInfoCallback(request_id3));
  MessageLoop::current()->RunUntilIdle();

  base::Time end_create = base::Time::Now();

  ASSERT_EQ(CALLBACK_TYPE_GET_FILE_INFO, GetStatusType(request_id3));
  status = GetStatus(request_id3);
  ASSERT_EQ(base::PLATFORM_FILE_OK, status.result);
  ASSERT_EQ(0, status.file_info.size);
  ASSERT_FALSE(status.file_info.is_directory);
  ASSERT_FALSE(status.file_info.is_symbolic_link);
  ASSERT_GE(status.file_info.last_modified, start_create);
  ASSERT_LE(status.file_info.last_modified, end_create);
  ASSERT_GE(status.file_info.creation_time, start_create);
  ASSERT_LE(status.file_info.creation_time, end_create);
}

TEST_F(MemoryFileUtilTest, TestReadWrite) {
  // Check that the file does not exist.

  const int request_id1 = GetNextRequestId();
  file_util()->GetFileInfo(base::FilePath("/mnt/memory/test1.txt"),
                           GetGetFileInfoCallback(request_id1));

  MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(CALLBACK_TYPE_GET_FILE_INFO, GetStatusType(request_id1));
  CallbackStatus status = GetStatus(request_id1);
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status.result);

  // Create & open file for writing.

  base::Time start_create = base::Time::Now();
  const int request_id2 = GetNextRequestId();
  file_util()->Open(base::FilePath("/mnt/memory/test1.txt"),
                    base::PLATFORM_FILE_CREATE_ALWAYS |
                    base::PLATFORM_FILE_WRITE,
                    GetOpenCallback(request_id2));
  MessageLoop::current()->RunUntilIdle();

  base::Time end_create = base::Time::Now();

  ASSERT_EQ(CALLBACK_TYPE_OPEN, GetStatusType(request_id2));
  status = GetStatus(request_id2);
  ASSERT_EQ(base::PLATFORM_FILE_OK, status.result);

  AsyncFileStream* write_file_stream = status.file_stream;

  // Check that file was created and has 0 size.

  const int request_id3 = GetNextRequestId();
  file_util()->GetFileInfo(base::FilePath("/mnt/memory/test1.txt"),
                           GetGetFileInfoCallback(request_id3));
  MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(CALLBACK_TYPE_GET_FILE_INFO, GetStatusType(request_id3));
  status = GetStatus(request_id3);
  ASSERT_EQ(base::PLATFORM_FILE_OK, status.result);
  ASSERT_EQ(0, status.file_info.size);
  ASSERT_FALSE(status.file_info.is_directory);
  ASSERT_FALSE(status.file_info.is_symbolic_link);
  ASSERT_GE(status.file_info.last_modified, start_create);
  ASSERT_LE(status.file_info.last_modified, end_create);

  // Write 10 bytes to file.

  const int request_id4 = GetNextRequestId();
  base::Time start_write = base::Time::Now();
  write_file_stream->Write(kTestString, 10,
                           GetReadWriteCallback(request_id4));
  MessageLoop::current()->RunUntilIdle();
  base::Time end_write = base::Time::Now();

  ASSERT_EQ(CALLBACK_TYPE_READ_WRITE, GetStatusType(request_id4));
  status = GetStatus(request_id4);
  ASSERT_EQ(base::PLATFORM_FILE_OK, status.result);
  ASSERT_EQ(10, status.length);

  // Check that the file has now size 10 and correct modification time.

  const int request_id5 = GetNextRequestId();
  file_util()->GetFileInfo(base::FilePath("/mnt/memory/test1.txt"),
                           GetGetFileInfoCallback(request_id5));
  MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(CALLBACK_TYPE_GET_FILE_INFO, GetStatusType(request_id5));
  status = GetStatus(request_id5);
  ASSERT_EQ(base::PLATFORM_FILE_OK, status.result);
  ASSERT_EQ(10, status.file_info.size);
  ASSERT_GE(status.file_info.last_modified, start_write);
  ASSERT_LE(status.file_info.last_modified, end_write);

  // Write the rest of the string to file.

  const int request_id6 = GetNextRequestId();
  start_write = base::Time::Now();
  write_file_stream->Write(kTestString + 10,
                           kTestStringLength - 10,
                           GetReadWriteCallback(request_id6));
  MessageLoop::current()->RunUntilIdle();
  end_write = base::Time::Now();

  ASSERT_EQ(CALLBACK_TYPE_READ_WRITE, GetStatusType(request_id6));
  status = GetStatus(request_id6);
  ASSERT_EQ(base::PLATFORM_FILE_OK, status.result);
  ASSERT_EQ(static_cast<int64>(kTestStringLength) - 10, status.length);

  // Check the file size & modification time.

  const int request_id7 = GetNextRequestId();
  file_util()->GetFileInfo(base::FilePath("/mnt/memory/test1.txt"),
                           GetGetFileInfoCallback(request_id7));
  MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(CALLBACK_TYPE_GET_FILE_INFO, GetStatusType(request_id7));
  status = GetStatus(request_id7);
  ASSERT_EQ(base::PLATFORM_FILE_OK, status.result);
  ASSERT_EQ(static_cast<int64>(kTestStringLength), status.file_info.size);
  ASSERT_GE(status.file_info.last_modified, start_write);
  ASSERT_LE(status.file_info.last_modified, end_write);

  // Open file for reading.

  const int request_id8 = GetNextRequestId();
  file_util()->Open(base::FilePath("/mnt/memory/test1.txt"),
                    base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
                    GetOpenCallback(request_id8));
  MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(CALLBACK_TYPE_OPEN, GetStatusType(request_id8));
  status = GetStatus(request_id8);
  ASSERT_EQ(base::PLATFORM_FILE_OK, status.result);
  ASSERT_EQ(base::PLATFORM_FILE_OK, status.result);

  AsyncFileStream* read_file_stream = status.file_stream;

  // Read the whole file
  char buffer[1024];
  const int request_id9 = GetNextRequestId();
  read_file_stream->Read(buffer, 1023, GetReadWriteCallback(request_id9));
  MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(CALLBACK_TYPE_READ_WRITE, GetStatusType(request_id9));
  status = GetStatus(request_id9);
  ASSERT_EQ(base::PLATFORM_FILE_OK, status.result);
  ASSERT_EQ(static_cast<int64>(kTestStringLength), status.length);

  buffer[status.length] = '\0';
  std::string result_string(buffer);
  ASSERT_EQ(kTestString, result_string);

  // Check that size & modification time have not changed.

  const int request_id10 = GetNextRequestId();
  file_util()->GetFileInfo(base::FilePath("/mnt/memory/test1.txt"),
                           GetGetFileInfoCallback(request_id10));
  MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(CALLBACK_TYPE_GET_FILE_INFO, GetStatusType(request_id10));
  status = GetStatus(request_id10);
  ASSERT_EQ(base::PLATFORM_FILE_OK, status.result);
  ASSERT_EQ(static_cast<int64>(kTestStringLength), status.file_info.size);
  ASSERT_GE(status.file_info.last_modified, start_write);
  ASSERT_LE(status.file_info.last_modified, end_write);

  // Open once more for writing.

  const int request_id11 = GetNextRequestId();
  file_util()->Open(base::FilePath("/mnt/memory/test1.txt"),
                    base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE,
                    GetOpenCallback(request_id11));
  MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(CALLBACK_TYPE_OPEN, GetStatusType(request_id11));
  status = GetStatus(request_id11);
  ASSERT_EQ(base::PLATFORM_FILE_OK, status.result);

  AsyncFileStream* write_file_stream2 = status.file_stream;

  // Check that the size has not changed.

  const int request_id12 = GetNextRequestId();
  file_util()->GetFileInfo(base::FilePath("/mnt/memory/test1.txt"),
                           GetGetFileInfoCallback(request_id12));
  MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(CALLBACK_TYPE_GET_FILE_INFO, GetStatusType(request_id12));
  status = GetStatus(request_id12);
  ASSERT_EQ(base::PLATFORM_FILE_OK, status.result);
  ASSERT_EQ(static_cast<int64>(kTestStringLength), status.file_info.size);

  // Seek beyond the end of file. Should return error.

  const int request_id13 = GetNextRequestId();
  write_file_stream2->Seek(1000, GetStatusCallback(request_id13));
  MessageLoop::current()->RunUntilIdle();
  ASSERT_EQ(CALLBACK_TYPE_STATUS, GetStatusType(request_id13));
  status = GetStatus(request_id13);
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status.result);

  // Try to write to read-only stream.

  const int request_id14 = GetNextRequestId();
  read_file_stream->Write(kTestString,
                          kTestStringLength,
                          GetReadWriteCallback(request_id14));
  MessageLoop::current()->RunUntilIdle();
  status = GetStatus(request_id14);
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION, status.result);

  // Write data overlapping with already written.


  const int request_id15 = GetNextRequestId();
  write_file_stream2->Seek(10, GetStatusCallback(request_id15));
  MessageLoop::current()->RunUntilIdle();
  ASSERT_EQ(base::PLATFORM_FILE_OK, GetStatus(request_id15).result);

  const int request_id16 = GetNextRequestId();
  write_file_stream2->Write(kTestString,
                            kTestStringLength,
                            GetReadWriteCallback(request_id16));
  MessageLoop::current()->RunUntilIdle();
  status = GetStatus(request_id16);
  ASSERT_EQ(base::PLATFORM_FILE_OK, status.result);
  ASSERT_EQ(static_cast<int64>(kTestStringLength), status.length);

  // Check size.

  const int request_id17 = GetNextRequestId();
  file_util()->GetFileInfo(base::FilePath("/mnt/memory/test1.txt"),
                           GetGetFileInfoCallback(request_id17));
  MessageLoop::current()->RunUntilIdle();
  status = GetStatus(request_id17);
  ASSERT_EQ(base::PLATFORM_FILE_OK, status.result);
  ASSERT_EQ(static_cast<int64>(kTestStringLength) + 10,
            status.file_info.size);

  // Read from 10th byte.
  const int request_id18 = GetNextRequestId();
  read_file_stream->Seek(10, GetStatusCallback(request_id18));
  MessageLoop::current()->RunUntilIdle();
  ASSERT_EQ(base::PLATFORM_FILE_OK, GetStatus(request_id18).result);

  const int request_id19 = GetNextRequestId();
  read_file_stream->Read(buffer, 1023, GetReadWriteCallback(request_id19));
  MessageLoop::current()->RunUntilIdle();
  status = GetStatus(request_id19);
  ASSERT_EQ(base::PLATFORM_FILE_OK, status.result);
  ASSERT_EQ(static_cast<int64>(kTestStringLength), status.length);
  buffer[status.length] = '\0';
  std::string result_string2(buffer);
  ASSERT_EQ(kTestString, result_string2);
}

// The directory structure we'll be testing on:
// path                    size
//
// /mnt/memory/a           0
// /mnt/memory/b/
// /mnt/memory/b/c         kTestStringLength
// /mnt/memory/b/d/
// /mnt/memory/b/e         0
// /mnt/memory/b/f         kTestStringLength
// /mnt/memory/b/g/
// /mnt/memory/b/g/h       0
// /mnt/memory/b/i         kTestStringLength
// /mnt/memory/c/
// /mnt/memory/longer_file_name.txt kTestStringLength
TEST_F(MemoryFileUtilTest, TestDirectoryOperations) {
  // Check the directory is empty.
  const int request_id0 = GetNextRequestId();
  file_util()->ReadDirectory(base::FilePath("/mnt/memory/"),
                             GetReadDirectoryCallback(request_id0));

  MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(CALLBACK_TYPE_READ_DIRECTORY, GetStatusType(request_id0));
  CallbackStatus& status = GetStatus(request_id0);
  ASSERT_TRUE(status.completed);
  ASSERT_FALSE(status.called_after_completed);
  ASSERT_EQ(1, status.called);
  ASSERT_EQ(0u, status.entries.size());

  // Create /mnt/memory/a, /mnt/memory/b/, /mnt/memory/longer_file_name.txt,
  // /mnt/memory/c/ asyncronously (i.e. we do not wait for each operation to
  // complete before starting the next one.

  base::Time start_create = base::Time::Now();
  CreateEmptyFile(base::FilePath("/mnt/memory/a"));

  int request_id1 = GetNextRequestId();
  file_util()->CreateDirectory(base::FilePath("/mnt/memory/b"),
                               GetStatusCallback(request_id1));

  CreateNonEmptyFile(base::FilePath("/mnt/memory/longer_file_name.txt"),
                     kTestString,
                     kTestStringLength);

  int request_id2 = GetNextRequestId();
  file_util()->CreateDirectory(base::FilePath("/mnt/memory/c"),
                               GetStatusCallback(request_id2));

  MessageLoop::current()->RunUntilIdle();
  base::Time end_create = base::Time::Now();

  ASSERT_EQ(base::PLATFORM_FILE_OK, GetStatus(request_id1).result);
  ASSERT_EQ(base::PLATFORM_FILE_OK, GetStatus(request_id2).result);

  // ReadDirectory /mnt/memory, /mnt/memory/a (not a dir), /mnt/memory/b/,
  // /mnt/memory/d (not found)

  set_read_directory_buffer_size(5);  // Should complete in one go.

  request_id1 = GetNextRequestId();
  file_util()->ReadDirectory(base::FilePath("/mnt/memory"),
                             GetReadDirectoryCallback(request_id1));
  request_id2 = GetNextRequestId();
  file_util()->ReadDirectory(base::FilePath("/mnt/memory/a"),
                             GetReadDirectoryCallback(request_id2));
  const int request_id3 = GetNextRequestId();
  file_util()->ReadDirectory(base::FilePath("/mnt/memory/b/"),
                             GetReadDirectoryCallback(request_id3));
  const int request_id4 = GetNextRequestId();
  file_util()->ReadDirectory(base::FilePath("/mnt/memory/d/"),
                             GetReadDirectoryCallback(request_id4));

  MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(base::PLATFORM_FILE_OK, GetStatus(request_id1).result);
  status = GetStatus(request_id1);
  ASSERT_TRUE(status.completed);
  ASSERT_FALSE(status.called_after_completed);
  ASSERT_EQ(1, status.called);  // Because the number of entries < 5.
  ASSERT_EQ(4u, status.entries.size());

  std::set<base::FilePath::StringType> seen;

  for (FileUtilAsync::FileList::const_iterator it = status.entries.begin();
       it != status.entries.end();
       ++it) {
    ASSERT_LE(start_create, it->last_modified_time);
    ASSERT_GE(end_create, it->last_modified_time);

    ASSERT_EQ(seen.end(), seen.find(it->name));
    seen.insert(it->name);

    if (it->name == "a") {
      ASSERT_FALSE(it->is_directory);
      ASSERT_EQ(0, it->size);
    } else if (it->name == "b") {
      ASSERT_TRUE(it->is_directory);
    } else if (it->name == "c") {
      ASSERT_TRUE(it->is_directory);
    } else if (it->name == "longer_file_name.txt") {
      ASSERT_FALSE(it->is_directory);
      ASSERT_EQ(static_cast<int64>(kTestStringLength), it->size);
    } else {
      LOG(ERROR) << "Unexpected file: " << it->name;
      ASSERT_TRUE(false);
    }
  }

  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY,
            GetStatus(request_id2).result);

  status = GetStatus(request_id3);
  ASSERT_TRUE(status.completed);
  ASSERT_FALSE(status.called_after_completed);
  ASSERT_EQ(1, status.called);
  ASSERT_EQ(0u, status.entries.size());

  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, GetStatus(request_id4).result);

  // Create files & dirs inside b/:
  // /mnt/memory/b/c         kTestStringLength
  // /mnt/memory/b/d/
  // /mnt/memory/b/e         0
  // /mnt/memory/b/f         kTestStringLength
  // /mnt/memory/b/g/
  // /mnt/memory/b/i         kTestStringLength
  //
  // /mnt/memory/b/g/h       0

  start_create = base::Time::Now();
  CreateNonEmptyFile(base::FilePath("/mnt/memory/b/c"),
                     kTestString,
                     kTestStringLength);
  request_id1 = GetNextRequestId();
  file_util()->CreateDirectory(base::FilePath("/mnt/memory/b/d"),
                               GetStatusCallback(request_id1));
  CreateEmptyFile(base::FilePath("/mnt/memory/b/e"));
  CreateNonEmptyFile(base::FilePath("/mnt/memory/b/f"),
                     kTestString,
                     kTestStringLength);
  request_id2 = GetNextRequestId();
  file_util()->CreateDirectory(base::FilePath("/mnt/memory/b/g"),
                               GetStatusCallback(request_id1));
  CreateNonEmptyFile(base::FilePath("/mnt/memory/b/i"),
                     kTestString,
                     kTestStringLength);

  MessageLoop::current()->RunUntilIdle();

  CreateEmptyFile(base::FilePath("/mnt/memory/b/g/h"));

  MessageLoop::current()->RunUntilIdle();
  end_create = base::Time::Now();

  // Read /mnt/memory and check that the number of entries is unchanged.

  request_id1 = GetNextRequestId();
  file_util()->ReadDirectory(base::FilePath("/mnt/memory"),
                             GetReadDirectoryCallback(request_id1));

  MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(base::PLATFORM_FILE_OK, GetStatus(request_id1).result);
  status = GetStatus(request_id1);
  ASSERT_TRUE(status.completed);
  ASSERT_FALSE(status.called_after_completed);
  ASSERT_EQ(1, status.called);  // Because the number of entries < 5.
  ASSERT_EQ(4u, status.entries.size());

  // Read /mnt/memory/b

  request_id1 = GetNextRequestId();
  file_util()->ReadDirectory(base::FilePath("/mnt/memory/b"),
                             GetReadDirectoryCallback(request_id1));

  MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(base::PLATFORM_FILE_OK, GetStatus(request_id1).result);
  status = GetStatus(request_id1);
  ASSERT_TRUE(status.completed);
  ASSERT_FALSE(status.called_after_completed);
  ASSERT_EQ(2, status.called);  // Because the number of entries > 5.
  ASSERT_EQ(6u, status.entries.size());

  seen.clear();

  for (FileUtilAsync::FileList::const_iterator it = status.entries.begin();
       it != status.entries.end();
       ++it) {
    ASSERT_LE(start_create, it->last_modified_time);
    ASSERT_GE(end_create, it->last_modified_time);

    ASSERT_EQ(seen.end(), seen.find(it->name));
    seen.insert(it->name);

    // /mnt/memory/b/c         kTestStringLength
    // /mnt/memory/b/d/
    // /mnt/memory/b/e         0
    // /mnt/memory/b/f         kTestStringLength
    // /mnt/memory/b/g/
    // /mnt/memory/b/i         kTestStringLength

    if (it->name == "c") {
      ASSERT_FALSE(it->is_directory);
      ASSERT_EQ(static_cast<int64>(kTestStringLength), it->size);
    } else if (it->name == "d") {
      ASSERT_TRUE(it->is_directory);
    } else if (it->name == "e") {
      ASSERT_FALSE(it->is_directory);
      ASSERT_EQ(0, it->size);
    } else if (it->name == "f") {
      ASSERT_FALSE(it->is_directory);
      ASSERT_EQ(static_cast<int64>(kTestStringLength), it->size);
    } else if (it->name == "g") {
      ASSERT_TRUE(it->is_directory);
    } else if (it->name == "i") {
      ASSERT_FALSE(it->is_directory);
      ASSERT_EQ(static_cast<int64>(kTestStringLength), it->size);
    } else {
      LOG(ERROR) << "Unexpected file: " << it->name;
      ASSERT_TRUE(false);
    }
  }

  // Remove single file: /mnt/memory/b/f

  request_id1 = GetNextRequestId();
  file_util()->Remove(base::FilePath("/mnt/memory/b/f"), false /* recursive */,
                      GetStatusCallback(request_id1));
  MessageLoop::current()->RunUntilIdle();
  ASSERT_EQ(base::PLATFORM_FILE_OK, GetStatus(request_id1).result);

  // Check the number of files in b/

  request_id1 = GetNextRequestId();
  file_util()->ReadDirectory(base::FilePath("/mnt/memory/b"),
                             GetReadDirectoryCallback(request_id1));

  MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(base::PLATFORM_FILE_OK, GetStatus(request_id1).result);
  status = GetStatus(request_id1);
  ASSERT_TRUE(status.completed);
  ASSERT_FALSE(status.called_after_completed);
  ASSERT_EQ(5u, status.entries.size());

  // Try remove /mnt/memory/b non-recursively (error)

  request_id1 = GetNextRequestId();
  file_util()->Remove(base::FilePath("/mnt/memory/b"), false /* recursive */,
                      GetStatusCallback(request_id1));
  MessageLoop::current()->RunUntilIdle();
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_FILE,
            GetStatus(request_id1).result);

  // Non-recursively remove empty directory.

  request_id1 = GetNextRequestId();
  file_util()->Remove(base::FilePath("/mnt/memory/b/d"), false /* recursive */,
                      GetStatusCallback(request_id1));
  MessageLoop::current()->RunUntilIdle();
  ASSERT_EQ(base::PLATFORM_FILE_OK, GetStatus(request_id1).result);

  request_id1 = GetNextRequestId();
  file_util()->GetFileInfo(base::FilePath("/mnt/memory/b/d"),
                           GetGetFileInfoCallback(request_id1));
  MessageLoop::current()->RunUntilIdle();
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, GetStatus(request_id1).result);

  // Remove /mnt/memory/b recursively.

  request_id1 = GetNextRequestId();
  file_util()->Remove(base::FilePath("/mnt/memory/b"), true /* recursive */,
                      GetStatusCallback(request_id1));
  MessageLoop::current()->RunUntilIdle();
  ASSERT_EQ(base::PLATFORM_FILE_OK, GetStatus(request_id1).result);

  // ReadDirectory /mnt/memory/b -> not found

  request_id1 = GetNextRequestId();
  file_util()->ReadDirectory(base::FilePath("/mnt/memory/b"),
                             GetReadDirectoryCallback(request_id1));
  MessageLoop::current()->RunUntilIdle();
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, GetStatus(request_id1).result);

  // ReadDirectory /mnt/memory

  request_id1 = GetNextRequestId();
  file_util()->ReadDirectory(base::FilePath("/mnt/memory"),
                             GetReadDirectoryCallback(request_id1));

  MessageLoop::current()->RunUntilIdle();

  ASSERT_EQ(base::PLATFORM_FILE_OK, GetStatus(request_id1).result);
  status = GetStatus(request_id1);
  ASSERT_TRUE(status.completed);
  ASSERT_FALSE(status.called_after_completed);
  ASSERT_EQ(3u, status.entries.size());

  seen.clear();

  for (FileUtilAsync::FileList::const_iterator it = status.entries.begin();
       it != status.entries.end();
       ++it) {
    ASSERT_EQ(seen.end(), seen.find(it->name));
    seen.insert(it->name);

    if (it->name == "a") {
      ASSERT_FALSE(it->is_directory);
      ASSERT_EQ(0, it->size);
    } else if (it->name == "c") {
      ASSERT_TRUE(it->is_directory);
    } else if (it->name == "longer_file_name.txt") {
      ASSERT_FALSE(it->is_directory);
      ASSERT_EQ(static_cast<int64>(kTestStringLength), it->size);
    } else {
      LOG(ERROR) << "Unexpected file: " << it->name;
      ASSERT_TRUE(false);
    }
  }
}

}  // namespace fileapi
