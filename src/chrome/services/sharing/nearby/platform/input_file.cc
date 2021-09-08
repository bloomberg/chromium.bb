// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/input_file.h"

#include "base/logging.h"

namespace location {
namespace nearby {
namespace chrome {

InputFile::InputFile(base::File file) : file_(std::move(file)) {}

InputFile::~InputFile() = default;

std::string InputFile::GetFilePath() const {
  // File path is not supported.
  return std::string();
}

std::int64_t InputFile::GetTotalSize() const {
  if (!file_.IsValid())
    return -1;

  return file_.GetLength();
}

ExceptionOr<ByteArray> InputFile::Read(std::int64_t size) {
  if (!file_.IsValid())
    return Exception::kIo;

  if (size == 0) {
    return ExceptionOr<ByteArray>(ByteArray());
  }

  char buf[size];
  int num_bytes_read = file_.ReadAtCurrentPos(buf, size);

  if (num_bytes_read < 0 || num_bytes_read > GetTotalSize())
    return Exception::kIo;

  return ExceptionOr<ByteArray>(ByteArray(buf, num_bytes_read));
}

Exception InputFile::Close() {
  if (!file_.IsValid())
    return {Exception::kIo};

  file_.Close();
  return {Exception::kSuccess};
}

base::File InputFile::ExtractUnderlyingFile() {
  return std::move(file_);
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
