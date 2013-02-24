// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_TEST_FILE_SET_H_
#define WEBKIT_FILEAPI_TEST_FILE_SET_H_

#include <set>

#include "base/files/file_path.h"

// Common test data structures and test cases.

namespace fileapi {

class FileSystemFileUtil;

namespace test {

struct TestCaseRecord {
  bool is_directory;
  const base::FilePath::CharType path[64];
  int64 data_file_size;
};

extern const TestCaseRecord kRegularTestCases[];
extern const size_t kRegularTestCaseSize;

size_t GetRegularTestCaseSize();

// Creates one file or directory specified by |record|.
void SetUpOneTestCase(const base::FilePath& root_path, const TestCaseRecord& record);

// Creates the files and directories specified in kRegularTestCases.
void SetUpRegularTestCases(const base::FilePath& root_path);

}  // namespace test

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_TEST_FILE_SET_H_
