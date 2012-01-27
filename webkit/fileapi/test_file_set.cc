// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/test_file_set.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fileapi {

namespace test {

const TestCaseRecord kRegularTestCases[] = {
  {true, FILE_PATH_LITERAL("dir a"), 0},
  {true, FILE_PATH_LITERAL("dir a/dir a"), 0},
  {true, FILE_PATH_LITERAL("dir a/dir d"), 0},
  {true, FILE_PATH_LITERAL("dir a/dir d/dir e"), 0},
  {true, FILE_PATH_LITERAL("dir a/dir d/dir e/dir f"), 0},
  {true, FILE_PATH_LITERAL("dir a/dir d/dir e/dir g"), 0},
  {true, FILE_PATH_LITERAL("dir a/dir d/dir e/dir h"), 0},
  {true, FILE_PATH_LITERAL("dir b"), 0},
  {true, FILE_PATH_LITERAL("dir b/dir a"), 0},
  {true, FILE_PATH_LITERAL("dir c"), 0},
  {false, FILE_PATH_LITERAL("file 0"), 38},
  {false, FILE_PATH_LITERAL("file 2"), 60},
  {false, FILE_PATH_LITERAL("file 3"), 0},
  {false, FILE_PATH_LITERAL("dir a/file 0"), 39},
  {false, FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 0"), 40},
  {false, FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 1"), 41},
  {false, FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 2"), 42},
  {false, FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 3"), 50},
};

const size_t kRegularTestCaseSize = arraysize(kRegularTestCases);

void SetUpOneTestCase(const FilePath& root_path,
                      const TestCaseRecord& test_case) {
  FilePath path = root_path.Append(test_case.path);
  if (test_case.is_directory) {
    ASSERT_TRUE(file_util::CreateDirectory(path));
  } else {
    base::PlatformFileError error_code;
    bool created = false;
    int file_flags = base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE;
    base::PlatformFile file_handle =
        base::CreatePlatformFile(path, file_flags, &created, &error_code);
    EXPECT_TRUE(created);
    ASSERT_NE(base::kInvalidPlatformFileValue, file_handle);
    ASSERT_EQ(base::PLATFORM_FILE_OK, error_code);
    ASSERT_TRUE(
        base::TruncatePlatformFile(file_handle, test_case.data_file_size));
    EXPECT_TRUE(base::ClosePlatformFile(file_handle));
  }
}


void SetUpRegularTestCases(const FilePath& root_path) {
  for (size_t i = 0; i < arraysize(kRegularTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Creating kRegularTestCases " << i);
    SetUpOneTestCase(root_path, kRegularTestCases[i]);
  }
}

}  // namespace test

}  // namespace fileapi
