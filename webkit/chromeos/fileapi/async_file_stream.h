// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHROMEOS_FILEAPI_ASYNC_FILE_STREAM_H_
#define WEBKIT_CHROMEOS_FILEAPI_ASYNC_FILE_STREAM_H_

#include "base/callback.h"
#include "base/platform_file.h"

namespace fileapi {

using base::PlatformFileError;

// This class is used for implementing Open() in FileUtilAsync. This class
// is similar to net::FileStream, but supporting only the asynchronous
// operations and not necessarily relying on PlatformFile.
class AsyncFileStream {
 public:
  // Used for Read() and Write(). |result| is the return code of the
  // operation, and |length| is the length of data read or written.
  typedef base::Callback<void(PlatformFileError result,
                              int64 length)> ReadWriteCallback;

  // Used for Seek(). |result| is the return code of the operation.
  typedef base::Callback<void(PlatformFileError)> SeekCallback;

  virtual ~AsyncFileStream() {};

  // Reads data from the current stream position. Up to |length| bytes
  // will be read from |buffer|. The memory pointed to by |buffer| must
  // remain valid until the callback is called. On success,
  // PLATFORM_FILE_OK is passed to |callback| with the number of bytes
  // read. On failure, an error code is passed instead.
  virtual void Read(char* buffer,
                    int64 length,
                    const ReadWriteCallback& callback) = 0;

  // Writes data at the current stream position. Up to |length| bytes will
  // be written from |buffer|. The memory pointed to by |buffer| must
  // remain valid until the callback is called. On success,
  // PLATFORM_FILE_OK is passed to |callback| with the number of bytes
  // written. On failure, an error code is passed instead.
  virtual void Write(const char* buffer,
                     int64 length,
                     const ReadWriteCallback& callback) = 0;

  // Moves the stream position. On success, PLATFORM_FILE_OK is passed to
  // |callback|. On error, an error code is passed instead.
  virtual void Seek(int64 offset,
                    const SeekCallback& callback) = 0;
};

}  // namespace fileapi

#endif  // WEBKIT_CHROMEOS_FILEAPI_ASYNC_FILE_STREAM_H_
