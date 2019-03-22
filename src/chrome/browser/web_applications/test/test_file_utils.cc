// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/test/test_file_utils.h"

namespace web_app {

std::unique_ptr<FileUtilsWrapper> TestFileUtils::Clone() {
  auto clone = std::make_unique<TestFileUtils>();
  clone->remaining_disk_space_ = remaining_disk_space_;
  return clone;
}

void TestFileUtils::SetRemainingDiskSpaceSize(int remaining_disk_space) {
  remaining_disk_space_ = remaining_disk_space;
}

int TestFileUtils::WriteFile(const base::FilePath& filename,
                             const char* data,
                             int size) {
  if (remaining_disk_space_ != kNoLimit) {
    if (size > remaining_disk_space_) {
      // Disk full:
      const int size_written = remaining_disk_space_;
      if (size_written > 0)
        FileUtilsWrapper::WriteFile(filename, data, size_written);
      remaining_disk_space_ = 0;
      return size_written;
    }

    remaining_disk_space_ -= size;
  }

  return FileUtilsWrapper::WriteFile(filename, data, size);
}

}  // namespace web_app
