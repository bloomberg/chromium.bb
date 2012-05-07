// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_file_reader.h"

#include "base/file_util_proxy.h"
#include "base/platform_file.h"
#include "base/sequenced_task_runner.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "webkit/blob/local_file_reader.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_interface.h"

using webkit_blob::LocalFileReader;

namespace fileapi {

namespace {

void ReadAdapter(base::WeakPtr<FileSystemFileReader> reader,
                 net::IOBuffer* buf, int buf_len,
                 const net::CompletionCallback& callback) {
  if (!reader.get())
    return;
  int rv = reader->Read(buf, buf_len, callback);
  if (rv != net::ERR_IO_PENDING)
    callback.Run(rv);
}

}

FileSystemFileReader::FileSystemFileReader(
    FileSystemContext* file_system_context,
    const GURL& url,
    int64 initial_offset)
    : file_system_context_(file_system_context),
      url_(url),
      initial_offset_(initial_offset),
      has_pending_create_snapshot_(false),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

FileSystemFileReader::~FileSystemFileReader() {
}

int FileSystemFileReader::Read(
    net::IOBuffer* buf, int buf_len,
    const net::CompletionCallback& callback) {
  if (local_file_reader_.get())
    return local_file_reader_->Read(buf, buf_len, callback);
  DCHECK(!has_pending_create_snapshot_);
  FileSystemOperationInterface* operation =
      file_system_context_->CreateFileSystemOperation(url_);
  if (!operation)
    return net::ERR_INVALID_URL;
  has_pending_create_snapshot_ = true;
  operation->CreateSnapshotFile(
      url_,
      base::Bind(&FileSystemFileReader::DidCreateSnapshot,
                 weak_factory_.GetWeakPtr(),
                 base::Bind(&ReadAdapter, weak_factory_.GetWeakPtr(),
                            make_scoped_refptr(buf), buf_len, callback),
                 callback));
  return net::ERR_IO_PENDING;
}

void FileSystemFileReader::DidCreateSnapshot(
    const base::Closure& read_closure,
    const net::CompletionCallback& callback,
    base::PlatformFileError file_error,
    const base::PlatformFileInfo& file_info,
    const FilePath& platform_path,
    const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref) {
  DCHECK(has_pending_create_snapshot_);
  DCHECK(!local_file_reader_.get());
  has_pending_create_snapshot_ = false;

  if (file_error != base::PLATFORM_FILE_OK) {
    callback.Run(LocalFileReader::PlatformFileErrorToNetError(file_error));
    return;
  }

  // Keep the reference (if it's non-null) so that the file won't go away.
  snapshot_ref_ = file_ref;

  local_file_reader_.reset(
      new LocalFileReader(file_system_context_->file_task_runner(),
                          platform_path,
                          initial_offset_,
                          base::Time()));

  read_closure.Run();
}

}  // namespace fileapi
