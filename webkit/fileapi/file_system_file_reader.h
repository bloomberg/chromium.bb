// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_FILE_READER_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_FILE_READER_H_
#pragma once

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "googleurl/src/gurl.h"
#include "webkit/blob/file_reader.h"

class FilePath;

namespace base {
class SequencedTaskRunner;
}

namespace webkit_blob {
class LocalFileReader;
class ShareableFileReference;
}

namespace fileapi {

class FileSystemContext;

// TODO(kinaba,satorux): This generic implementation would work for any
// filesystems but remote filesystem should implement its own reader
// rather than relying on FileSystemOperation::GetSnapshotFile() which
// may force downloading the entire contents for remote files.
class FileSystemFileReader : public webkit_blob::FileReader {
 public:
  // Creates a new FileReader for a filesystem URL |url| form |initial_offset|.
  FileSystemFileReader(FileSystemContext* file_system_context,
                       const GURL& url,
                       int64 initial_offset);
  virtual ~FileSystemFileReader();

  // FileReader override.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) OVERRIDE;

 private:
  void DidCreateSnapshot(
      const base::Closure& read_closure,
      const net::CompletionCallback& callback,
      base::PlatformFileError file_error,
      const base::PlatformFileInfo& file_info,
      const FilePath& platform_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref);

  scoped_refptr<FileSystemContext> file_system_context_;
  const GURL url_;
  const int64 initial_offset_;
  scoped_ptr<webkit_blob::LocalFileReader> local_file_reader_;
  scoped_refptr<webkit_blob::ShareableFileReference> snapshot_ref_;
  bool has_pending_create_snapshot_;
  base::WeakPtrFactory<FileSystemFileReader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemFileReader);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_FILE_READER_H_
