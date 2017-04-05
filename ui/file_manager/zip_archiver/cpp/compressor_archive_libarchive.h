// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPRESSOR_ARCHIVE_LIBARCHIVE_H_
#define COMPRESSOR_ARCHIVE_LIBARCHIVE_H_

#include <string>

#include "archive.h"

#include "compressor_archive.h"
#include "compressor_stream.h"

// A namespace with constants used by CompressorArchiveLibarchive.
namespace compressor_archive_constants {
const int64_t kMaximumDataChunkSize = 512 * 1024;
const int kFilePermission = 640;
const int kDirectoryPermission = 760;
}  // namespace compressor_archive_constants

class CompressorArchiveLibarchive : public CompressorArchive {
 public:
  explicit CompressorArchiveLibarchive(CompressorStream* compressor_stream);

  virtual ~CompressorArchiveLibarchive();

  // Creates an archive object.
  virtual void CreateArchive();

  // Releases all resources obtained by libarchive.
  virtual void CloseArchive(bool has_error);

  // Adds an entry to the archive.
  virtual void AddToArchive(const std::string& filename,
                            int64_t file_size,
                            time_t modification_time,
                            bool is_directory);

  // A getter function for archive_.
  struct archive* archive() const { return archive_; }

  // A getter function for compressor_stream.
  CompressorStream* compressor_stream() const { return compressor_stream_; }

 private:
  // An instance that takes care of all IO operations.
  CompressorStream* compressor_stream_;

  // The libarchive correspondent archive object.
  struct archive* archive_;

  // The libarchive correspondent archive entry object that is currently
  // processed.
  struct archive_entry* entry;

  // The buffer used to store the data read from JavaScript.
  char* destination_buffer_;
};

#endif  // COMPRESSOR_ARCHIVE_LIBARCHIVE_H_
