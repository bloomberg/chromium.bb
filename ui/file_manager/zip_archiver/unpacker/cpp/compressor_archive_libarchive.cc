// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "compressor_archive_libarchive.h"

#include <cerrno>
#include <cstring>

#include "archive_entry.h"
#include "ppapi/cpp/logging.h"

namespace {
  // Nothing to do here because JavaScript takes care of file open operations.
  int CustomArchiveOpen(archive* archive_object, void* client_data) {
    return ARCHIVE_OK;
  }

  // Called when any data chunk must be written on the archive. It copies data
  // from the given buffer processed by libarchive to an array buffer and passes
  // it to compressor_stream.
  ssize_t CustomArchiveWrite(archive* archive_object, void* client_data,
      const void* buffer, size_t length) {
    CompressorArchiveLibarchive* compressor_libarchive =
        static_cast<CompressorArchiveLibarchive*>(client_data);

    const char* char_buffer = static_cast<const char*>(buffer);

    // Copy the data in buffer to array_buffer.
    PP_DCHECK(length > 0);
    pp::VarArrayBuffer array_buffer(length);
    char* array_buffer_data = static_cast<char*>(array_buffer.Map());
    memcpy(array_buffer_data, char_buffer, length);
    array_buffer.Unmap();

    ssize_t written_bytes =
        compressor_libarchive->compressor_stream()->Write(length, array_buffer);

    // Negative written_bytes represents an error.
    if (written_bytes < 0) {
      // When writing fails, archive_set_error() should be called and -1 should
      // be returned.
      archive_set_error(
          compressor_libarchive->archive(), EIO, "Failed to write a chunk.");
      return -1;
    }
    return written_bytes;
  }

  // Nothing to do here because JavaScript takes care of file close operations.
  int CustomArchiveClose(archive* archive_object, void* client_data) {
    return ARCHIVE_OK;
  }
}

CompressorArchiveLibarchive::CompressorArchiveLibarchive(
    CompressorStream* compressor_stream)
    : CompressorArchive(compressor_stream),
      compressor_stream_(compressor_stream) {
  destination_buffer_ =
      new char[compressor_archive_constants::kMaximumDataChunkSize];
}

CompressorArchiveLibarchive::~CompressorArchiveLibarchive() {
  delete destination_buffer_;
}

void CompressorArchiveLibarchive::CreateArchive() {
  archive_ = archive_write_new();
  archive_write_set_format_zip(archive_);

  // Passing 1 as the second argument causes the final chunk not to be padded.
  archive_write_set_bytes_in_last_block(archive_, 1);
  archive_write_set_bytes_per_block(
     archive_, compressor_archive_constants::kMaximumDataChunkSize);
  archive_write_open(archive_, this, CustomArchiveOpen,
                     CustomArchiveWrite, CustomArchiveClose);
}

void CompressorArchiveLibarchive::AddToArchive(
    const std::string& filename,
    int64_t file_size,
    time_t modification_time,
    bool is_directory) {
  entry = archive_entry_new();

  archive_entry_set_pathname(entry, filename.c_str());
  archive_entry_set_size(entry, file_size);
  archive_entry_set_mtime(entry, modification_time, 0 /* millisecond */);

  if (is_directory) {
    archive_entry_set_filetype(entry, AE_IFDIR);
    archive_entry_set_perm(
        entry, compressor_archive_constants::kDirectoryPermission);
  } else {
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(
        entry, compressor_archive_constants::kFilePermission);
  }
  archive_write_header(archive_, entry);
  // If archive_errno() returns 0, the header was written correctly.
  if (archive_errno(archive_) != 0) {
    CloseArchive(true /* hasError */);
    return;
  }

  if (!is_directory) {
    int64_t remaining_size = file_size;
    while (remaining_size > 0) {
      int64_t chunk_size = std::min(remaining_size,
          compressor_archive_constants::kMaximumDataChunkSize);
      PP_DCHECK(chunk_size > 0);

      int64_t read_bytes =
          compressor_stream_->Read(chunk_size, destination_buffer_);
      // Negative read_bytes indicates an error occurred when reading chunks.
      if (read_bytes < 0) {
        CloseArchive(true /* hasError */);
        break;
      }

      int64_t written_bytes =
          archive_write_data(archive_, destination_buffer_, read_bytes);
      // If archive_errno() returns 0, the buffer was written correctly.
      if (archive_errno(archive_) != 0) {
        CloseArchive(true /* hasError */);
        break;
      }
      PP_DCHECK(written_bytes > 0);

      remaining_size -= written_bytes;
    }
  }

  archive_entry_free(entry);
}

void CompressorArchiveLibarchive::CloseArchive(bool has_error) {
  // If has_error is true, mark the archive object as being unusable and
  // release resources without writing no more data on the archive.
  if (has_error)
    archive_write_fail(archive_);
  if (archive_) {
    archive_write_free(archive_);
    archive_ = NULL;
  }
}
