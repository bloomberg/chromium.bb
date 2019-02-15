// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/fileapi/memory_file_stream_reader.h"

#include <memory>

#include "base/memory/ptr_util.h"

namespace storage {

std::unique_ptr<FileStreamReader> FileStreamReader::CreateForMemoryFile(
    base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util,
    const base::FilePath& file_path,
    int64_t initial_offset,
    const base::Time& expected_modification_time) {
  return base::WrapUnique(
      new MemoryFileStreamReader(std::move(memory_file_util), file_path,
                                 initial_offset, expected_modification_time));
}

MemoryFileStreamReader::~MemoryFileStreamReader() = default;

int MemoryFileStreamReader::Read(net::IOBuffer* buf,
                                 int buf_len,
                                 net::CompletionOnceCallback callback) {
  // TODO(https://crbug.com/93417): Implement!
  NOTIMPLEMENTED();
  return 0;
}

int64_t MemoryFileStreamReader::GetLength(
    net::Int64CompletionOnceCallback callback) {
  NOTIMPLEMENTED();
  return 0;
}

MemoryFileStreamReader::MemoryFileStreamReader(
    base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util,
    const base::FilePath& file_path,
    int64_t initial_offset,
    const base::Time& expected_modification_time)
    : memory_file_util_(std::move(memory_file_util)) {
  DCHECK(memory_file_util_);
}

}  // namespace storage
