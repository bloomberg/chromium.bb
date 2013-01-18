// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/chromeos/fileapi/remote_file_stream_writer.h"

#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "webkit/blob/local_file_stream_reader.h"
#include "webkit/blob/shareable_file_reference.h"
#include "webkit/fileapi/local_file_stream_writer.h"
#include "webkit/fileapi/remote_file_system_proxy.h"

namespace fileapi {

RemoteFileStreamWriter::RemoteFileStreamWriter(
    const scoped_refptr<RemoteFileSystemProxyInterface>& remote_filesystem,
    const FileSystemURL& url,
    int64 offset)
    : remote_filesystem_(remote_filesystem),
      url_(url),
      initial_offset_(offset),
      has_pending_create_snapshot_(false),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

RemoteFileStreamWriter::~RemoteFileStreamWriter() {
}

int RemoteFileStreamWriter::Write(net::IOBuffer* buf,
                                  int buf_len,
                                  const net::CompletionCallback& callback) {
  DCHECK(!has_pending_create_snapshot_);
  DCHECK(pending_cancel_callback_.is_null());

  if (!local_file_writer_.get()) {
    has_pending_create_snapshot_ = true;
    // In this RemoteFileStreamWriter, we only create snapshot file and don't
    // have explicit close operation. This is ok, because close is automatically
    // triggered by a refcounted |file_ref_| passed to OnFileOpened, from the
    // destructor of RemoteFileStreamWriter.
    remote_filesystem_->CreateWritableSnapshotFile(
        url_,
        base::Bind(&RemoteFileStreamWriter::OnFileOpened,
                   weak_factory_.GetWeakPtr(),
                   make_scoped_refptr(buf),
                   buf_len,
                   callback));
    return net::ERR_IO_PENDING;
  }
  return local_file_writer_->Write(buf, buf_len, callback);
}

void RemoteFileStreamWriter::OnFileOpened(
    net::IOBuffer* buf,
    int buf_len,
    const net::CompletionCallback& callback,
    base::PlatformFileError open_result,
    const FilePath& local_path,
    const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref) {
  has_pending_create_snapshot_ = false;
  if (!pending_cancel_callback_.is_null()) {
    InvokePendingCancelCallback(net::OK);
    return;
  }

  if (open_result != base::PLATFORM_FILE_OK) {
    callback.Run(net::PlatformFileErrorToNetError(open_result));
    return;
  }

  // Hold the reference to the file. Releasing the reference notifies the file
  // system about to close file.
  file_ref_ = file_ref;

  DCHECK(!local_file_writer_.get());
  local_file_writer_.reset(new fileapi::LocalFileStreamWriter(local_path,
                                                              initial_offset_));
  int result = local_file_writer_->Write(buf, buf_len, callback);
  if (result != net::ERR_IO_PENDING)
    callback.Run(result);
}

int RemoteFileStreamWriter::Cancel(const net::CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(pending_cancel_callback_.is_null());

  // If file open operation is in-flight, wait for its completion and cancel
  // further write operation in OnFileOpened.
  if (has_pending_create_snapshot_) {
    pending_cancel_callback_ = callback;
    return net::ERR_IO_PENDING;
  }

  // If LocalFileWriter is already created, just delegate the cancel to it.
  if (local_file_writer_.get()) {
    pending_cancel_callback_ = callback;
    return local_file_writer_->Cancel(
        base::Bind(&RemoteFileStreamWriter::InvokePendingCancelCallback,
                   weak_factory_.GetWeakPtr()));
  }

  // Write() is not called yet.
  return net::ERR_UNEXPECTED;
}

int RemoteFileStreamWriter::Flush(const net::CompletionCallback& callback) {
  // For remote file writer, Flush() is a no-op. Synchronization to the remote
  // server is not done until the file is closed.
  return net::OK;
}

void RemoteFileStreamWriter::InvokePendingCancelCallback(int result) {
  net::CompletionCallback callback = pending_cancel_callback_;
  pending_cancel_callback_.Reset();
  callback.Run(result);
}

}  // namespace gdata
