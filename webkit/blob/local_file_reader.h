// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BLOB_LOCAL_FILE_READER_H_
#define WEBKIT_BLOB_LOCAL_FILE_READER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/platform_file.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "webkit/blob/blob_export.h"
#include "webkit/blob/file_reader.h"

namespace base {
class TaskRunner;
}

namespace webkit_blob {

// A thin wrapper of net::FileStream with range support for sliced file
// handling.
class BLOB_EXPORT LocalFileReader : public FileReader {
 public:
  // A convenient method to translate platform file error to net error code.
  static int PlatformFileErrorToNetError(base::PlatformFileError file_error);

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
  LocalFileReader(base::TaskRunner* task_runner,
                  const FilePath& file_path,
                  int64 initial_offset,
                  const base::Time& expected_modification_time);
  virtual ~LocalFileReader();

  // FileReader overrides.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) OVERRIDE;

  // Returns the length of the file if it could successfully retrieve the
  // file info *and* its last modification time equals to
  // expected_modification_time_ (rv >= 0 cases).
  // Otherwise, a negative error code is returned (rv < 0 cases).
  // If the stream is deleted while it has an in-flight GetLength operation
  // |callback| will not be called.
  int GetLength(const net::Int64CompletionCallback& callback);

 private:
  int Open(const net::CompletionCallback& callback);

  // Callbacks that are chained from Open for Read.
  void DidVerifyForOpen(const net::CompletionCallback& callback,
                        int64 get_length_result);
  void DidOpenFileStream(const net::CompletionCallback& callback,
                         int result);
  void DidSeekFileStream(const net::CompletionCallback& callback,
                         int64 seek_result);
  void DidOpenForRead(net::IOBuffer* buf,
                      int buf_len,
                      const net::CompletionCallback& callback,
                      int open_result);

  void DidGetFileInfoForGetLength(const net::Int64CompletionCallback& callback,
                                  base::PlatformFileError error,
                                  const base::PlatformFileInfo& file_info);

  scoped_refptr<base::TaskRunner> task_runner_;
  scoped_ptr<net::FileStream> stream_impl_;
  const FilePath file_path_;
  const int64 initial_offset_;
  const base::Time expected_modification_time_;
  bool has_pending_open_;
  base::WeakPtrFactory<LocalFileReader> weak_factory_;
};

}  // namespace webkit_blob

#endif  // WEBKIT_BLOB_LOCAL_FILE_READER_H_
