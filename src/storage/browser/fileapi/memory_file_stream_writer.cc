// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/fileapi/memory_file_stream_writer.h"

#include <memory>

#include "base/memory/ptr_util.h"

namespace storage {

std::unique_ptr<FileStreamWriter> FileStreamWriter::CreateForMemoryFile(
    base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util,
    const base::FilePath& file_path,
    int64_t initial_offset,
    OpenOrCreate open_or_create) {
  return base::WrapUnique(new MemoryFileStreamWriter(
      std::move(memory_file_util), file_path, initial_offset, open_or_create));
}

MemoryFileStreamWriter::~MemoryFileStreamWriter() = default;

int MemoryFileStreamWriter::Write(net::IOBuffer* buf,
                                  int buf_len,
                                  net::CompletionOnceCallback callback) {
  // TODO(https://crbug.com/93417): Implement!
  NOTIMPLEMENTED();
  return 0;
}

int MemoryFileStreamWriter::Cancel(net::CompletionOnceCallback callback) {
  NOTIMPLEMENTED();
  return 0;
}

int MemoryFileStreamWriter::Flush(net::CompletionOnceCallback callback) {
  NOTIMPLEMENTED();

  return 0;
}

MemoryFileStreamWriter::MemoryFileStreamWriter(
    base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util,
    const base::FilePath& file_path,
    int64_t initial_offset,
    OpenOrCreate open_or_create)
    : memory_file_util_(std::move(memory_file_util)) {
  DCHECK(memory_file_util_);
}
}  // namespace storage
