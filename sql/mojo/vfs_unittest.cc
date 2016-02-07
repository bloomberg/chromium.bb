// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>
#include <utility>

#include "base/macros.h"
#include "components/filesystem/public/interfaces/file_system.mojom.h"
#include "mojo/shell/public/cpp/application_test_base.h"
#include "mojo/shell/public/cpp/shell.h"
#include "mojo/util/capture_util.h"
#include "sql/mojo/mojo_vfs.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace std {

// This deleter lets us be safe with sqlite3 objects, which aren't really the
// structs, but slabs of new uint8_t[size].
template <>
struct default_delete<sqlite3_file> {
  inline void operator()(sqlite3_file* ptr) const {
    // Why don't we call file->pMethods->xClose() here? Because it's not
    // guaranteed to be valid. sqlite3_file "objects" can be in partially
    // initialized states.
    delete [] reinterpret_cast<uint8_t*>(ptr);
  }
};

}  // namespace std

namespace sql {

const char kFileName[] = "TestingDatabase.db";

class VFSTest : public mojo::test::ApplicationTestBase,
                public filesystem::FileSystemClient {
 public:
  VFSTest() : binding_(this) {}
  ~VFSTest() override {}

  sqlite3_vfs* vfs() {
    return sqlite3_vfs_find(NULL);
  }

  scoped_ptr<sqlite3_file> MakeFile() {
    return scoped_ptr<sqlite3_file>(reinterpret_cast<sqlite3_file*>(
        new uint8_t[vfs()->szOsFile]));
  }

  void SetUp() override {
    mojo::test::ApplicationTestBase::SetUp();

    shell()->ConnectToService("mojo:filesystem", &files_);

    filesystem::FileError error = filesystem::FileError::FAILED;
    filesystem::DirectoryPtr directory;
    files_->OpenFileSystem("temp", GetProxy(&directory),
                           binding_.CreateInterfacePtrAndBind(),
                           mojo::Capture(&error));
    ASSERT_TRUE(files_.WaitForIncomingResponse());
    ASSERT_EQ(filesystem::FileError::OK, error);

    vfs_.reset(new ScopedMojoFilesystemVFS(std::move(directory)));
  }

  void TearDown() override {
    vfs_.reset();
    mojo::test::ApplicationTestBase::TearDown();
  }

  void OnFileSystemShutdown() override {
  }

 private:
  filesystem::FileSystemPtr files_;
  scoped_ptr<ScopedMojoFilesystemVFS> vfs_;
  mojo::Binding<filesystem::FileSystemClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(VFSTest);
};

TEST_F(VFSTest, TestInstalled) {
  EXPECT_EQ("mojo", std::string(vfs()->zName));
}

TEST_F(VFSTest, ExclusiveOpen) {
  // Opening a non-existent file exclusively should work.
  scoped_ptr<sqlite3_file> file(MakeFile());
  int out_flags;
  int rc = vfs()->xOpen(vfs(), kFileName, file.get(),
                        SQLITE_OPEN_EXCLUSIVE | SQLITE_OPEN_CREATE,
                        &out_flags);
  EXPECT_EQ(SQLITE_OK, rc);

  // Opening it a second time exclusively shouldn't.
  scoped_ptr<sqlite3_file> file2(MakeFile());
  rc = vfs()->xOpen(vfs(), kFileName, file2.get(),
                    SQLITE_OPEN_EXCLUSIVE | SQLITE_OPEN_CREATE,
                    &out_flags);
  EXPECT_NE(SQLITE_OK, rc);

  file->pMethods->xClose(file.get());
}

TEST_F(VFSTest, NonexclusiveOpen) {
  // Opening a non-existent file should work.
  scoped_ptr<sqlite3_file> file(MakeFile());
  int out_flags;
  int rc = vfs()->xOpen(vfs(), kFileName, file.get(),
                        SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE,
                        &out_flags);
  EXPECT_EQ(SQLITE_OK, rc);

  // Opening it a second time should work.
  scoped_ptr<sqlite3_file> file2(MakeFile());
  rc = vfs()->xOpen(vfs(), kFileName, file2.get(),
                    SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE,
                    &out_flags);
  EXPECT_EQ(SQLITE_OK, rc);

  file->pMethods->xClose(file.get());
  file->pMethods->xClose(file2.get());
}

TEST_F(VFSTest, NullFilenameOpen) {
  // Opening a file with a null filename should return a valid file object.
  scoped_ptr<sqlite3_file> file(MakeFile());
  int out_flags;
  int rc = vfs()->xOpen(
      vfs(), nullptr, file.get(),
      SQLITE_OPEN_DELETEONCLOSE | SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE,
      &out_flags);
  EXPECT_EQ(SQLITE_OK, rc);

  file->pMethods->xClose(file.get());
}

TEST_F(VFSTest, DeleteOnClose) {
  {
    scoped_ptr<sqlite3_file> file(MakeFile());
    int out_flags;
    int rc = vfs()->xOpen(
        vfs(), kFileName, file.get(),
        SQLITE_OPEN_DELETEONCLOSE | SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE,
        &out_flags);
    EXPECT_EQ(SQLITE_OK, rc);
    file->pMethods->xClose(file.get());
  }

  // The file shouldn't exist now.
  int result = 0;
  vfs()->xAccess(vfs(), kFileName, SQLITE_ACCESS_EXISTS, &result);
  EXPECT_FALSE(result);
}

TEST_F(VFSTest, TestNonExistence) {
  // We shouldn't have a file exist yet in a fresh directory.
  int result = 0;
  vfs()->xAccess(vfs(), kFileName, SQLITE_ACCESS_EXISTS, &result);
  EXPECT_FALSE(result);
}

TEST_F(VFSTest, TestExistence) {
  {
    scoped_ptr<sqlite3_file> file(MakeFile());
    int out_flags;
    int rc = vfs()->xOpen(vfs(), kFileName, file.get(),
                          SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE,
                          &out_flags);
    EXPECT_EQ(SQLITE_OK, rc);

    file->pMethods->xClose(file.get());
  }

  int result = 0;
  vfs()->xAccess(vfs(), kFileName, SQLITE_ACCESS_EXISTS, &result);
  EXPECT_TRUE(result);
}

TEST_F(VFSTest, TestDelete) {
  {
    scoped_ptr<sqlite3_file> file(MakeFile());
    int out_flags;
    int rc = vfs()->xOpen(vfs(), kFileName, file.get(),
                          SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE,
                          &out_flags);
    EXPECT_EQ(SQLITE_OK, rc);

    file->pMethods->xClose(file.get());
  }

  int result = 0;
  vfs()->xAccess(vfs(), kFileName, SQLITE_ACCESS_EXISTS, &result);
  EXPECT_TRUE(result);

  vfs()->xDelete(vfs(), kFileName, 0);

  vfs()->xAccess(vfs(), kFileName, SQLITE_ACCESS_EXISTS, &result);
  EXPECT_FALSE(result);
}

TEST_F(VFSTest, TestWriteAndRead) {
  const char kBuffer[] = "One Two Three Four Five Six Seven";
  const int kBufferSize = arraysize(kBuffer);

  {
    scoped_ptr<sqlite3_file> file(MakeFile());
    int out_flags;
    int rc = vfs()->xOpen(vfs(), kFileName, file.get(),
                          SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE,
                          &out_flags);
    EXPECT_EQ(SQLITE_OK, rc);

    for (int i = 0; i < 10; ++i) {
      rc = file->pMethods->xWrite(file.get(), kBuffer, kBufferSize,
                                  i * kBufferSize);
      EXPECT_EQ(SQLITE_OK, rc);
    }

    file->pMethods->xClose(file.get());
  }

  // Expect that the size of the file is 10 * arraysize(kBuffer);
  {
    scoped_ptr<sqlite3_file> file(MakeFile());
    int out_flags;
    int rc = vfs()->xOpen(vfs(), kFileName, file.get(),
                          SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE,
                          &out_flags);
    EXPECT_EQ(SQLITE_OK, rc);

    sqlite_int64 size;
    rc = file->pMethods->xFileSize(file.get(), &size);
    EXPECT_EQ(SQLITE_OK, rc);
    EXPECT_EQ(10 * kBufferSize, size);

    file->pMethods->xClose(file.get());
  }

  // We should be able to read things back.
  {
    scoped_ptr<sqlite3_file> file(MakeFile());
    int out_flags;
    int rc = vfs()->xOpen(vfs(), kFileName, file.get(),
                          SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE,
                          &out_flags);
    EXPECT_EQ(SQLITE_OK, rc);

    char data_buffer[kBufferSize];
    memset(data_buffer, '8', kBufferSize);
    for (int i = 0; i < 10; ++i) {
      rc = file->pMethods->xRead(file.get(), data_buffer, kBufferSize,
                                 i * kBufferSize);
      EXPECT_EQ(SQLITE_OK, rc);
      EXPECT_TRUE(strncmp(kBuffer, &data_buffer[0], kBufferSize) == 0);
    }

    file->pMethods->xClose(file.get());
  }
}

TEST_F(VFSTest, PartialRead) {
  const char kBuffer[] = "One Two Three Four Five Six Seven";
  const int kBufferSize = arraysize(kBuffer);

  // Write the data once.
  {
    scoped_ptr<sqlite3_file> file(MakeFile());
    int out_flags;
    int rc = vfs()->xOpen(vfs(), kFileName, file.get(),
                          SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE,
                          &out_flags);
    EXPECT_EQ(SQLITE_OK, rc);

    rc = file->pMethods->xWrite(file.get(), kBuffer, kBufferSize, 0);
    EXPECT_EQ(SQLITE_OK, rc);

    file->pMethods->xClose(file.get());
  }

  // Now attempt to read kBufferSize + 5 from a file sized to kBufferSize.
  {
    scoped_ptr<sqlite3_file> file(MakeFile());
    int out_flags;
    int rc = vfs()->xOpen(vfs(), kFileName, file.get(),
                          SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE,
                          &out_flags);
    EXPECT_EQ(SQLITE_OK, rc);

    const char kBufferWithFiveNulls[] =
        "One Two Three Four Five Six Seven\0\0\0\0\0";
    const int kBufferWithFiveNullsSize = arraysize(kBufferWithFiveNulls);

    char data_buffer[kBufferWithFiveNullsSize];
    memset(data_buffer, '8', kBufferWithFiveNullsSize);
    rc = file->pMethods->xRead(file.get(), data_buffer,
                               kBufferWithFiveNullsSize, 0);
    EXPECT_EQ(SQLITE_IOERR_SHORT_READ, rc);

    EXPECT_TRUE(strncmp(kBufferWithFiveNulls, &data_buffer[0],
                        kBufferWithFiveNullsSize) == 0);

    file->pMethods->xClose(file.get());
  }
}

TEST_F(VFSTest, Truncate) {
  const char kBuffer[] = "One Two Three Four Five Six Seven";
  const int kBufferSize = arraysize(kBuffer);
  const int kCharsToThree = 13;

  scoped_ptr<sqlite3_file> file(MakeFile());
  int out_flags;
  int rc = vfs()->xOpen(vfs(), kFileName, file.get(),
                        SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE,
                        &out_flags);
  EXPECT_EQ(SQLITE_OK, rc);

  rc = file->pMethods->xWrite(file.get(), kBuffer, kBufferSize, 0);
  EXPECT_EQ(SQLITE_OK, rc);

  sqlite_int64 size;
  rc = file->pMethods->xFileSize(file.get(), &size);
  EXPECT_EQ(SQLITE_OK, rc);
  EXPECT_EQ(kBufferSize, size);

  rc = file->pMethods->xTruncate(file.get(), kCharsToThree);
  EXPECT_EQ(SQLITE_OK, rc);

  rc = file->pMethods->xFileSize(file.get(), &size);
  EXPECT_EQ(SQLITE_OK, rc);
  EXPECT_EQ(kCharsToThree, size);

  file->pMethods->xClose(file.get());
}

}  // namespace sql
