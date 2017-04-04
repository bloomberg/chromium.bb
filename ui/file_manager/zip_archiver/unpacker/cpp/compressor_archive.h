// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPRESSSOR_ARCHIVE_H_
#define COMPRESSSOR_ARCHIVE_H_

#include "compressor_io_javascript_stream.h"

// Defines a wrapper for packing operations executed on an archive. API is not
// meant to be thread safe and its methods shouldn't be called in parallel.
class CompressorArchive {
 public:
  explicit CompressorArchive(CompressorStream* compressor_stream)
    : compressor_stream_(compressor_stream) {}

  virtual ~CompressorArchive() {}

  // Creates an archive object. This method does not call CustomArchiveWrite, so
  // this is synchronous.
  virtual void CreateArchive() = 0;

  // Releases all resources obtained by libarchive.
  // This method also writes metadata about the archive itself onto the end of
  // the archive file before releasing resources if hasError is false. Since
  // writing data onto the archive is asynchronous, this function must not be
  // called in the main thread if hasError is false.
  virtual void CloseArchive(bool has_error) = 0;

  // Adds an entry to the archive. It writes the header of the entry onto the
  // archive first, and then if it is a file(not a directory), requests
  // JavaScript for file chunks, compresses and writes them onto the archive
  // until all chunks of the entry are written onto the archive. This method
  // calls IO operations, so this function must not be called in the main thread.
  virtual void AddToArchive(const std::string& filename,
                            int64_t file_size,
                            time_t modification_time,
                            bool is_directory) = 0;

  // A getter function for archive_.
  struct archive* archive() const { return archive_; }

  // A getter function for compressor_stream_.
  CompressorStream* compressor_stream() const { return compressor_stream_; }

 private:
  // The libarchive correspondent archive object.
  struct archive* archive_;

  // An instance that takes care of all IO operations.
  CompressorStream* compressor_stream_;
};

#endif  // COMPRESSSOR_ARCHIVE_H_
