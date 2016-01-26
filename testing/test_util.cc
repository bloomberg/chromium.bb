// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "testing/test_util.h"

#include <sys/stat.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <memory>
#include <string>

#include "common/libwebm_utils.h"

namespace libwebm {
namespace test {

std::string GetTestDataDir() {
  return std::getenv("LIBWEBM_TEST_DATA_PATH");
}

std::string GetTestFilePath(const std::string& name) {
  const std::string libwebm_testdata_dir = GetTestDataDir();
  return libwebm_testdata_dir + "/" + name;
}

bool CompareFiles(const std::string& file1, const std::string& file2) {
  const std::size_t kBlockSize = 4096;
  std::uint8_t buf1[kBlockSize] = {0};
  std::uint8_t buf2[kBlockSize] = {0};

  FilePtr f1 = FilePtr(std::fopen(file1.c_str(), "rb"), FILEDeleter());
  FilePtr f2 = FilePtr(std::fopen(file2.c_str(), "rb"), FILEDeleter());

  if (!f1.get() || !f2.get()) {
    // Files cannot match if one or both couldn't be opened.
    return false;
  }

  do {
    const std::size_t r1 = std::fread(buf1, 1, kBlockSize, f1.get());
    const std::size_t r2 = std::fread(buf2, 1, kBlockSize, f2.get());

    if (r1 != r2 || std::memcmp(buf1, buf2, r1)) {
      return 0;  // Files are not equal
    }
  } while (!std::feof(f1.get()) && !std::feof(f2.get()));

  return std::feof(f1.get()) && std::feof(f2.get());
}

std::string GetTempFileName() {
  // TODO(tomfinegan): This is only test code, but it would be nice to avoid
  // using std::tmpnam().
  return std::tmpnam(nullptr);
}

std::uint64_t GetFileSize(const std::string& file_name) {
  std::uint64_t file_size = 0;
#ifndef _MSC_VER
  struct stat st;
  st.st_size = 0;
  if (stat(file_name.c_str(), &st) == 0) {
#else
  struct _stat st;
  st.st_size = 0;
  if (_stat(file_name.c_str(), &st) == 0) {
#endif
    file_size = st.st_size;
  }
  return file_size;
}

TempFileDeleter::TempFileDeleter() {
  file_name_ = GetTempFileName();
}

TempFileDeleter::~TempFileDeleter() {
  std::ifstream file(file_name_);
  if (file.good()) {
    file.close();
    std::remove(file_name_.c_str());
  }
}

}  // namespace test
}  // namespace libwebm
