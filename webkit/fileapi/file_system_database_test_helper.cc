// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_database_test_helper.h"

#include <algorithm>
#include <functional>
#include <vector>

#include "base/file_util.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_util.h"

namespace fileapi {

void CorruptDatabase(const base::FilePath& db_path,
                     leveldb::FileType type,
                     ptrdiff_t offset,
                     size_t size) {
  file_util::FileEnumerator file_enum(db_path, false /* not recursive */,
      file_util::FileEnumerator::DIRECTORIES |
      file_util::FileEnumerator::FILES);
  base::FilePath file_path;
  base::FilePath picked_file_path;
  uint64 picked_file_number = kuint64max;

  while (!(file_path = file_enum.Next()).empty()) {
    uint64 number = kuint64max;
    leveldb::FileType file_type;
    EXPECT_TRUE(leveldb::ParseFileName(FilePathToString(file_path.BaseName()),
                                       &number, &file_type));
    if (file_type == type &&
        (picked_file_number == kuint64max || picked_file_number < number)) {
      picked_file_path = file_path;
      picked_file_number = number;
    }
  }

  EXPECT_FALSE(picked_file_path.empty());
  EXPECT_NE(kuint64max, picked_file_number);

  bool created = true;
  base::PlatformFileError error = base::PLATFORM_FILE_ERROR_FAILED;
  base::PlatformFile file =
      CreatePlatformFile(picked_file_path,
                         base::PLATFORM_FILE_OPEN |
                         base::PLATFORM_FILE_READ |
                         base::PLATFORM_FILE_WRITE,
                         &created, &error);
  EXPECT_EQ(base::PLATFORM_FILE_OK, error);
  EXPECT_FALSE(created);

  base::PlatformFileInfo file_info;
  EXPECT_TRUE(base::GetPlatformFileInfo(file, &file_info));
  if (offset < 0)
    offset += file_info.size;
  EXPECT_GE(offset, 0);
  EXPECT_LE(offset, file_info.size);

  size = std::min(size, static_cast<size_t>(file_info.size - offset));

  std::vector<char> buf(size);
  int read_size = base::ReadPlatformFile(file, offset,
                                         vector_as_array(&buf), buf.size());
  EXPECT_LT(0, read_size);
  EXPECT_GE(buf.size(), static_cast<size_t>(read_size));
  buf.resize(read_size);

  std::transform(buf.begin(), buf.end(), buf.begin(),
                 std::logical_not<char>());

  int written_size = base::WritePlatformFile(file, offset,
                                             vector_as_array(&buf), buf.size());
  EXPECT_GT(written_size, 0);
  EXPECT_EQ(buf.size(), static_cast<size_t>(written_size));

  base::ClosePlatformFile(file);
}

}  // namespace fileapi
