// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BLOB_LOCAL_FILE_READER_H_
#define WEBKIT_BLOB_LOCAL_FILE_READER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "net/base/completion_callback.h"
#include "webkit/blob/blob_export.h"

namespace base {
class MessageLoopProxy;
}

namespace net {
class FileStream;
class IOBuffer;
}

namespace webkit_blob {

// A thin wrapper of net::FileStream with range support for sliced file
// handling.
class BLOB_EXPORT LocalFileReader {
 public:
  typedef base::Callback<void(int error, scoped_ptr<net::FileStream> stream)>
      OpenFileStreamCallback;

  // Creates a new FileReader for a local file |file_path|.
  // |initial_offset| specifies the offset in the file where the first read
  // should start.  If the given offset is out of the file range any
  // read operation may error out with net::ERR_REQUEST_RANGE_NOT_SATISFIABLE.
  //
  // |expected_modification_time| specifies the expected last modification
  // If the value is non-null, the reader will check the underlying file's
  // actual modification time to see if the file has been modified, and if
  // it does any succeeding read operations should fail with
  // ERR_UPLOAD_FILE_CHANGED error.
  LocalFileReader(base::MessageLoopProxy* file_thread_proxy,
                  const FilePath& file_path,
                  int64 initial_offset,
                  const base::Time& expected_modification_time);

  ~LocalFileReader();

  // Reads from the current cursor position asynchronously.
  // This works mostly same as how net::FileStream::Read() works except that
  // it internally opens (and seeks) the file if it is not opened yet.
  // It is invalid to call Read while there is an in-flight Read operation.
  int Read(net::IOBuffer* buf, int buf_len,
           const net::CompletionCallback& callback);

  // Returns the number of bytes available to read from the beginning of
  // the file (or initial_offset) until the end of the file (rv >= 0 cases).
  // Otherwise, a negative error code is returned (rv < 0 cases).
  int GetLength(const net::Int64CompletionCallback& callback);

 private:
  class OpenFileStreamHelper;

  int Open(const OpenFileStreamCallback& callback);
  void DidOpen(net::IOBuffer* buf,
               int buf_len,
               const net::CompletionCallback& callback,
               int open_error,
               scoped_ptr<net::FileStream> stream);

  scoped_refptr<base::MessageLoopProxy> file_thread_proxy_;
  scoped_ptr<net::FileStream> stream_impl_;
  const FilePath file_path_;
  const int64 initial_offset_;
  const base::Time expected_modification_time_;
  bool has_pending_open_;
  base::WeakPtrFactory<LocalFileReader> weak_factory_;
};

}  // namespace webkit_blob

#endif  // WEBKIT_BLOB_LOCAL_FILE_READER_H_
