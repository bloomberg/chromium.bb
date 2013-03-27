// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_file_stream_reader.h"

#include "base/files/file_util_proxy.h"
#include "base/platform_file.h"
#include "base/single_thread_task_runner.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "webkit/blob/local_file_stream_reader.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_task_runners.h"

using webkit_blob::LocalFileStreamReader;

namespace fileapi {

namespace {

void ReadAdapter(base::WeakPtr<FileSystemFileStreamReader> reader,
                 net::IOBuffer* buf, int buf_len,
                 const net::CompletionCallback& callback) {
  if (!reader.get())
    return;
  int rv = reader->Read(buf, buf_len, callback);
  if (rv != net::ERR_IO_PENDING)
    callback.Run(rv);
}

void GetLengthAdapter(base::WeakPtr<FileSystemFileStreamReader> reader,
                      const net::Int64CompletionCallback& callback) {
  if (!reader.get())
    return;
  int rv = reader->GetLength(callback);
  if (rv != net::ERR_IO_PENDING)
    callback.Run(rv);
}

void Int64CallbackAdapter(const net::Int64CompletionCallback& callback,
                          int value) {
  callback.Run(value);
}

}  // namespace

FileSystemFileStreamReader::FileSystemFileStreamReader(
    FileSystemContext* file_system_context,
    const FileSystemURL& url,
    int64 initial_offset,
    const base::Time& expected_modification_time)
    : file_system_context_(file_system_context),
      url_(url),
      initial_offset_(initial_offset),
      expected_modification_time_(expected_modification_time),
      has_pending_create_snapshot_(false),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

FileSystemFileStreamReader::~FileSystemFileStreamReader() {
}

int FileSystemFileStreamReader::Read(
    net::IOBuffer* buf, int buf_len,
    const net::CompletionCallback& callback) {
  if (local_file_reader_.get())
    return local_file_reader_->Read(buf, buf_len, callback);
  return CreateSnapshot(
      base::Bind(&ReadAdapter, weak_factory_.GetWeakPtr(),
                 make_scoped_refptr(buf), buf_len, callback),
      callback);
}

int64 FileSystemFileStreamReader::GetLength(
    const net::Int64CompletionCallback& callback) {
  if (local_file_reader_.get())
    return local_file_reader_->GetLength(callback);
  return CreateSnapshot(
      base::Bind(&GetLengthAdapter, weak_factory_.GetWeakPtr(), callback),
      base::Bind(&Int64CallbackAdapter, callback));
}

int FileSystemFileStreamReader::CreateSnapshot(
    const base::Closure& callback,
    const net::CompletionCallback& error_callback) {
  DCHECK(!has_pending_create_snapshot_);
  base::PlatformFileError error_code;
  FileSystemOperation* operation =
      file_system_context_->CreateFileSystemOperation(url_, &error_code);
  if (error_code != base::PLATFORM_FILE_OK)
    return net::PlatformFileErrorToNetError(error_code);
  has_pending_create_snapshot_ = true;
  operation->CreateSnapshotFile(
      url_,
      base::Bind(&FileSystemFileStreamReader::DidCreateSnapshot,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 error_callback));
  return net::ERR_IO_PENDING;
}

void FileSystemFileStreamReader::DidCreateSnapshot(
    const base::Closure& callback,
    const net::CompletionCallback& error_callback,
    base::PlatformFileError file_error,
    const base::PlatformFileInfo& file_info,
    const base::FilePath& platform_path,
    const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref) {
  DCHECK(has_pending_create_snapshot_);
  DCHECK(!local_file_reader_.get());
  has_pending_create_snapshot_ = false;

  if (file_error != base::PLATFORM_FILE_OK) {
    error_callback.Run(net::PlatformFileErrorToNetError(file_error));
    return;
  }

  // Keep the reference (if it's non-null) so that the file won't go away.
  snapshot_ref_ = file_ref;

  local_file_reader_.reset(
      new LocalFileStreamReader(
          file_system_context_->task_runners()->file_task_runner(),
          platform_path, initial_offset_, expected_modification_time_));

  callback.Run();
}

}  // namespace fileapi
