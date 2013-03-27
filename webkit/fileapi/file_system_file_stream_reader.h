// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_FILE_STREAM_READER_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_FILE_STREAM_READER_H_

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "base/time.h"
#include "webkit/blob/file_stream_reader.h"
#include "webkit/blob/shareable_file_reference.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/storage/webkit_storage_export.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace webkit_blob {
class LocalFileStreamReader;
}

namespace fileapi {

class FileSystemContext;

// TODO(kinaba,satorux): This generic implementation would work for any
// filesystems but remote filesystem should implement its own reader
// rather than relying on FileSystemOperation::GetSnapshotFile() which
// may force downloading the entire contents for remote files.
class WEBKIT_STORAGE_EXPORT_PRIVATE FileSystemFileStreamReader
    : public webkit_blob::FileStreamReader {
 public:
  // Creates a new FileReader for a filesystem URL |url| form |initial_offset|.
  // |expected_modification_time| specifies the expected last modification if
  // the value is non-null, the reader will check the underlying file's actual
  // modification time to see if the file has been modified, and if it does any
  // succeeding read operations should fail with ERR_UPLOAD_FILE_CHANGED error.
  FileSystemFileStreamReader(FileSystemContext* file_system_context,
                             const FileSystemURL& url,
                             int64 initial_offset,
                             const base::Time& expected_modification_time);
  virtual ~FileSystemFileStreamReader();

  // FileStreamReader overrides.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) OVERRIDE;
  virtual int64 GetLength(
      const net::Int64CompletionCallback& callback) OVERRIDE;

 private:
  int CreateSnapshot(const base::Closure& callback,
                     const net::CompletionCallback& error_callback);
  void DidCreateSnapshot(
      const base::Closure& callback,
      const net::CompletionCallback& error_callback,
      base::PlatformFileError file_error,
      const base::PlatformFileInfo& file_info,
      const base::FilePath& platform_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref);

  scoped_refptr<FileSystemContext> file_system_context_;
  FileSystemURL url_;
  const int64 initial_offset_;
  const base::Time expected_modification_time_;
  scoped_ptr<webkit_blob::LocalFileStreamReader> local_file_reader_;
  scoped_refptr<webkit_blob::ShareableFileReference> snapshot_ref_;
  bool has_pending_create_snapshot_;
  base::WeakPtrFactory<FileSystemFileStreamReader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemFileStreamReader);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_FILE_STREAM_READER_H_
