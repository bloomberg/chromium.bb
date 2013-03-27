// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/blob/local_file_stream_reader.h"

#include "base/file_util.h"
#include "base/files/file_util_proxy.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "base/task_runner.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace webkit_blob {

namespace {

const int kOpenFlagsForRead = base::PLATFORM_FILE_OPEN |
                              base::PLATFORM_FILE_READ |
                              base::PLATFORM_FILE_ASYNC;

// Verify if the underlying file has not been modified.
bool VerifySnapshotTime(const base::Time& expected_modification_time,
                        const base::PlatformFileInfo& file_info) {
  return expected_modification_time.is_null() ||
         expected_modification_time.ToTimeT() ==
             file_info.last_modified.ToTimeT();
}

}  // namespace

LocalFileStreamReader::LocalFileStreamReader(
    base::TaskRunner* task_runner,
    const base::FilePath& file_path,
    int64 initial_offset,
    const base::Time& expected_modification_time)
    : task_runner_(task_runner),
      file_path_(file_path),
      initial_offset_(initial_offset),
      expected_modification_time_(expected_modification_time),
      has_pending_open_(false),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

LocalFileStreamReader::~LocalFileStreamReader() {
}

int LocalFileStreamReader::Read(net::IOBuffer* buf, int buf_len,
                          const net::CompletionCallback& callback) {
  DCHECK(!has_pending_open_);
  if (stream_impl_.get())
    return stream_impl_->Read(buf, buf_len, callback);
  return Open(base::Bind(&LocalFileStreamReader::DidOpenForRead,
                         weak_factory_.GetWeakPtr(),
                         make_scoped_refptr(buf), buf_len, callback));
}

int64 LocalFileStreamReader::GetLength(
    const net::Int64CompletionCallback& callback) {
  const bool posted = base::FileUtilProxy::GetFileInfo(
      task_runner_, file_path_,
      base::Bind(&LocalFileStreamReader::DidGetFileInfoForGetLength,
                 weak_factory_.GetWeakPtr(), callback));
  DCHECK(posted);
  return net::ERR_IO_PENDING;
}

int LocalFileStreamReader::Open(const net::CompletionCallback& callback) {
  DCHECK(!has_pending_open_);
  DCHECK(!stream_impl_.get());
  has_pending_open_ = true;

  // Call GetLength first to make it perform last-modified-time verification,
  // and then call DidVerifyForOpen for do the rest.
  return GetLength(base::Bind(&LocalFileStreamReader::DidVerifyForOpen,
                              weak_factory_.GetWeakPtr(), callback));
}

void LocalFileStreamReader::DidVerifyForOpen(
    const net::CompletionCallback& callback,
    int64 get_length_result) {
  if (get_length_result < 0) {
    callback.Run(static_cast<int>(get_length_result));
    return;
  }

  stream_impl_.reset(new net::FileStream(NULL));
  const int result = stream_impl_->Open(
      file_path_, kOpenFlagsForRead,
      base::Bind(&LocalFileStreamReader::DidOpenFileStream,
                 weak_factory_.GetWeakPtr(),
                 callback));
  if (result != net::ERR_IO_PENDING)
    callback.Run(result);
}

void LocalFileStreamReader::DidOpenFileStream(
    const net::CompletionCallback& callback,
    int result) {
  if (result != net::OK) {
    callback.Run(result);
    return;
  }
  result = stream_impl_->Seek(
      net::FROM_BEGIN, initial_offset_,
      base::Bind(&LocalFileStreamReader::DidSeekFileStream,
                 weak_factory_.GetWeakPtr(),
                 callback));
  if (result != net::ERR_IO_PENDING) {
    callback.Run(result);
  }
}

void LocalFileStreamReader::DidSeekFileStream(
    const net::CompletionCallback& callback,
    int64 seek_result) {
  if (seek_result < 0) {
    callback.Run(static_cast<int>(seek_result));
    return;
  }
  if (seek_result != initial_offset_) {
    callback.Run(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    return;
  }
  callback.Run(net::OK);
}

void LocalFileStreamReader::DidOpenForRead(
    net::IOBuffer* buf,
    int buf_len,
    const net::CompletionCallback& callback,
    int open_result) {
  DCHECK(has_pending_open_);
  has_pending_open_ = false;
  if (open_result != net::OK) {
    stream_impl_.reset();
    callback.Run(open_result);
    return;
  }
  DCHECK(stream_impl_.get());
  const int read_result = stream_impl_->Read(buf, buf_len, callback);
  if (read_result != net::ERR_IO_PENDING)
    callback.Run(read_result);
}

void LocalFileStreamReader::DidGetFileInfoForGetLength(
    const net::Int64CompletionCallback& callback,
    base::PlatformFileError error,
    const base::PlatformFileInfo& file_info) {
  if (file_info.is_directory) {
    callback.Run(net::ERR_FILE_NOT_FOUND);
    return;
  }
  if (error != base::PLATFORM_FILE_OK) {
    callback.Run(net::PlatformFileErrorToNetError(error));
    return;
  }
  if (!VerifySnapshotTime(expected_modification_time_, file_info)) {
    callback.Run(net::ERR_UPLOAD_FILE_CHANGED);
    return;
  }
  callback.Run(file_info.size);
}

}  // namespace webkit_blob
