// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHROMEOS_FILEAPI_REMOTE_FILE_STREAM_WRITER_H_
#define WEBKIT_CHROMEOS_FILEAPI_REMOTE_FILE_STREAM_WRITER_H_

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "webkit/fileapi/file_stream_writer.h"
#include "webkit/fileapi/file_system_url.h"

namespace net {
class IOBuffer;
}

namespace webkit_blob {
class ShareableFileReference;
}

namespace fileapi {

class RemoteFileSystemProxyInterface;

// FileStreamWriter interface for writing to a file on remote file system.
class RemoteFileStreamWriter : public fileapi::FileStreamWriter {
 public:
  // Creates a writer for a file on |remote_filesystem| with path url |url|
  // (like "filesystem:chrome-extension://id/external/special/drive/...") that
  // starts writing from |offset|. When invalid parameters are set, the first
  // call to Write() method fails.
  RemoteFileStreamWriter(
      const scoped_refptr<RemoteFileSystemProxyInterface>& remote_filesystem,
      const FileSystemURL& url,
      int64 offset);
  virtual ~RemoteFileStreamWriter();

  // FileWriter override.
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback) OVERRIDE;
  virtual int Cancel(const net::CompletionCallback& callback) OVERRIDE;
  virtual int Flush(const net::CompletionCallback& callback) OVERRIDE;

 private:
  // Callback function to do the continuation of the work of the first Write()
  // call, which tries to open the local copy of the file before writing.
  void OnFileOpened(
      net::IOBuffer* buf,
      int buf_len,
      const net::CompletionCallback& callback,
      base::PlatformFileError open_result,
      const base::FilePath& local_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref);
  // Calls |pending_cancel_callback_|, assuming it is non-null.
  void InvokePendingCancelCallback(int result);

  scoped_refptr<RemoteFileSystemProxyInterface> remote_filesystem_;
  const FileSystemURL url_;
  const int64 initial_offset_;
  scoped_ptr<fileapi::FileStreamWriter> local_file_writer_;
  scoped_refptr<webkit_blob::ShareableFileReference> file_ref_;
  bool has_pending_create_snapshot_;
  net::CompletionCallback pending_cancel_callback_;

  base::WeakPtrFactory<RemoteFileStreamWriter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoteFileStreamWriter);
};

}  // namespace fileapi

#endif  // WEBKIT_CHROMEOS_FILEAPI_REMOTE_FILE_STREAM_WRITER_H_
