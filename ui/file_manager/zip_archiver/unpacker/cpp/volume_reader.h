// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VOLUME_READER_H_
#define VOLUME_READER_H_

#include <string>

#include "archive.h"

// Defines a reader for archive volumes. This class is used by libarchive
// for custom reads: https://github.com/libarchive/libarchive/wiki/Examples
class VolumeReader {
 public:
  virtual ~VolumeReader() {}

  // Tries to read bytes_to_read from the archive. The result will be stored at
  // *destination_buffer, which is the address of a buffer handled by
  // VolumeReaderJavaScriptStream. *destination_buffer must be available until
  // the next VolumeReader:Read call or until VolumeReader is destructed.
  //
  // The operation must be synchronous (libarchive requirement), so it
  // should NOT be done on the main thread. bytes_to_read should be > 0.
  //
  // Returns the actual number of read bytes or ARCHIVE_FATAL in case of
  // failure.
  virtual int64_t Read(int64_t bytes_to_read,
                       const void** destination_buffer) = 0;

  // Tries to skip bytes_to_skip number of bytes. Returns the actual number of
  // skipped bytes or 0 if none were skipped. In case of failure
  // VolumeReader::Skip returns 0 bytes and VolumeReader::Read can be used
  // to skip those bytes by discarding them.
  virtual int64_t Skip(int64_t bytes_to_skip) = 0;

  // Tries to seek to offset from whence. Returns the resulting offset location
  // or ARCHIVE_FATAL in case of errors. Similar to
  // http://www.cplusplus.com/reference/cstdio/fseek/
  virtual int64_t Seek(int64_t offset, int whence) = 0;

  // Fetches a passphrase for reading. If the passphrase is not available it
  // returns NULL.
  virtual const char* Passphrase() = 0;
};

#endif  // VOLUME_READER_H_
