// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_flash_file.h"

#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/flash_file.h"
#include "ppapi/tests/testing_instance.h"
#include "ppapi/tests/test_utils.h"

#if defined(PPAPI_OS_WIN)
#include <windows.h>
#else
#include <errno.h>
#include <unistd.h>
#endif

using pp::flash::FileModuleLocal;

namespace {

void CloseFileHandle(PP_FileHandle file_handle) {
#if defined(PPAPI_OS_WIN)
  CloseHandle(file_handle);
#else
  close(file_handle);
#endif
}

bool WriteFile(PP_FileHandle file_handle, const std::string& contents) {
#if defined(PPAPI_OS_WIN)
  DWORD bytes_written = 0;
  BOOL result = ::WriteFile(file_handle, contents.c_str(), contents.size(),
                            &bytes_written, NULL);
  return result && bytes_written == static_cast<DWORD>(contents.size());
#else
  ssize_t bytes_written = 0;
  do {
    bytes_written = write(file_handle, contents.c_str(), contents.size());
  } while (bytes_written == -1 && errno == EINTR);
  return bytes_written == static_cast<ssize_t>(contents.size());
#endif
}

bool ReadFile(PP_FileHandle file_handle, std::string* contents) {
  static const size_t kBufferSize = 1024;
  char* buffer = new char[kBufferSize];
  bool result = false;
  contents->clear();

#if defined(PPAPI_OS_WIN)
  SetFilePointer(file_handle, 0, NULL, FILE_BEGIN);
  DWORD bytes_read = 0;
  do {
    result = !!::ReadFile(file_handle, buffer, kBufferSize, &bytes_read, NULL);
    if (result && bytes_read > 0)
      contents->append(buffer, bytes_read);
  } while (result && bytes_read > 0);
#else
  lseek(file_handle, 0, SEEK_SET);
  ssize_t bytes_read = 0;
  do {
    do {
      bytes_read = read(file_handle, buffer, kBufferSize);
    } while (bytes_read == -1 && errno == EINTR);
    result = bytes_read != -1;
    if (bytes_read > 0)
      contents->append(buffer, bytes_read);
  } while (bytes_read > 0);
#endif

  delete[] buffer;
  return result;
}

}  // namespace

REGISTER_TEST_CASE(FlashFile);

TestFlashFile::TestFlashFile(TestingInstance* instance)
    : TestCase(instance) {
}

TestFlashFile::~TestFlashFile() {
}

bool TestFlashFile::Init() {
  return FileModuleLocal::IsAvailable();
}

void TestFlashFile::RunTests(const std::string& filter) {
  RUN_TEST(CreateTemporaryFile, filter);
}

std::string TestFlashFile::TestCreateTemporaryFile() {
  // Make sure that the root directory exists.
  FileModuleLocal::CreateDir(instance_, std::string());

  size_t before_create = 0;
  ASSERT_SUBTEST_SUCCESS(GetItemCountUnderModuleLocalRoot(&before_create));

  PP_FileHandle file_handle = FileModuleLocal::CreateTemporaryFile(instance_);
  ASSERT_NE(PP_kInvalidFileHandle, file_handle);

  std::string contents = "This is a temp file.";
  ASSERT_TRUE(WriteFile(file_handle, contents));
  std::string read_contents;
  ASSERT_TRUE(ReadFile(file_handle, &read_contents));
  ASSERT_EQ(contents, read_contents);

  CloseFileHandle(file_handle);

  size_t after_close = 0;
  ASSERT_SUBTEST_SUCCESS(GetItemCountUnderModuleLocalRoot(&after_close));
  ASSERT_EQ(before_create, after_close);

  PASS();
}

std::string TestFlashFile::GetItemCountUnderModuleLocalRoot(
    size_t* item_count) {
  std::vector<FileModuleLocal::DirEntry> contents;
  ASSERT_TRUE(FileModuleLocal::GetDirContents(instance_, "", &contents));
  *item_count = contents.size();
  PASS();
}
