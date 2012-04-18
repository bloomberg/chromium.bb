// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/blob/local_file_reader.h"

#include "base/file_util.h"
#include "base/file_util_proxy.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
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

void DidGetFileInfoForGetLength(const net::Int64CompletionCallback& callback,
                                const base::Time& expected_modification_time,
                                base::PlatformFileError error,
                                const base::PlatformFileInfo& file_info) {
  if (file_info.is_directory) {
    callback.Run(net::ERR_FILE_NOT_FOUND);
    return;
  }
  if (error != base::PLATFORM_FILE_OK) {
    callback.Run(LocalFileReader::PlatformFileErrorToNetError(error));
    return;
  }
  if (!VerifySnapshotTime(expected_modification_time, file_info)) {
    callback.Run(net::ERR_UPLOAD_FILE_CHANGED);
    return;
  }
  callback.Run(file_info.size);
}

void DidSeekFile(const LocalFileReader::OpenFileStreamCallback& callback,
                 scoped_ptr<net::FileStream> stream_impl,
                 int64 initial_offset,
                 int64 new_offset) {
  int result = net::OK;
  if (new_offset < 0)
    result = static_cast<int>(new_offset);
  else if (new_offset != initial_offset)
    result = net::ERR_REQUEST_RANGE_NOT_SATISFIABLE;
  callback.Run(result, stream_impl.Pass());
}

void EmptyCompletionCallback(int) {}

}  // namespace

// static
int LocalFileReader::PlatformFileErrorToNetError(
    base::PlatformFileError file_error) {
  switch (file_error) {
    case base::PLATFORM_FILE_OK:
      return net::OK;
    case base::PLATFORM_FILE_ERROR_NOT_FOUND:
      return net::ERR_FILE_NOT_FOUND;
    case base::PLATFORM_FILE_ERROR_ACCESS_DENIED:
      return net::ERR_ACCESS_DENIED;
    default:
      return net::ERR_FAILED;
  }
}

// A helper class to open, verify and seek a file stream for a given path.
class LocalFileReader::OpenFileStreamHelper {
 public:
  OpenFileStreamHelper(base::MessageLoopProxy* file_thread_proxy)
      : file_thread_proxy_(file_thread_proxy),
        file_handle_(base::kInvalidPlatformFileValue),
        result_(net::OK) {}
  ~OpenFileStreamHelper() {
    if (file_handle_ != base::kInvalidPlatformFileValue) {
      file_thread_proxy_->PostTask(
          FROM_HERE,
          base::Bind(base::IgnoreResult(&base::ClosePlatformFile),
                     file_handle_));
    }
  }

  void OpenAndVerifyOnFileThread(const FilePath& file_path,
                                 const base::Time& expected_modification_time) {
    base::PlatformFileError file_error = base::PLATFORM_FILE_OK;
    file_handle_ = base::CreatePlatformFile(
        file_path, kOpenFlagsForRead, NULL, &file_error);
    if (file_error != base::PLATFORM_FILE_OK) {
      result_ = PlatformFileErrorToNetError(file_error);
      return;
    }
    DCHECK_NE(base::kInvalidPlatformFileValue, file_handle_);
    base::PlatformFileInfo file_info;
    if (!base::GetPlatformFileInfo(file_handle_, &file_info)) {
      result_ = net::ERR_FAILED;
      return;
    }
    if (!VerifySnapshotTime(expected_modification_time, file_info)) {
      result_ = net::ERR_UPLOAD_FILE_CHANGED;
      return;
    }
    result_ = net::OK;
  }

  void OpenStreamOnCallingThread(int64 initial_offset,
                                 const OpenFileStreamCallback& callback) {
    DCHECK(!callback.is_null());
    scoped_ptr<net::FileStream> stream_impl;
    if (result_ != net::OK) {
      callback.Run(result_, stream_impl.Pass());
      return;
    }
    stream_impl.reset(
        new net::FileStream(file_handle_, kOpenFlagsForRead, NULL));
    file_handle_ = base::kInvalidPlatformFileValue;
    result_ = stream_impl->Seek(net::FROM_BEGIN, initial_offset,
                                base::Bind(&DidSeekFile, callback,
                                           base::Passed(&stream_impl),
                                           initial_offset));
    if (result_ != net::ERR_IO_PENDING)
      callback.Run(result_, stream_impl.Pass());
  }

 private:
  scoped_refptr<base::MessageLoopProxy> file_thread_proxy_;
  base::PlatformFile file_handle_;
  int result_;
  DISALLOW_COPY_AND_ASSIGN(OpenFileStreamHelper);
};

LocalFileReader::LocalFileReader(
    base::MessageLoopProxy* file_thread_proxy,
    const FilePath& file_path,
    int64 initial_offset,
    const base::Time& expected_modification_time)
    : file_thread_proxy_(file_thread_proxy),
      file_path_(file_path),
      initial_offset_(initial_offset),
      expected_modification_time_(expected_modification_time),
      has_pending_open_(false),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

LocalFileReader::~LocalFileReader() {
  if (!stream_impl_.get())
    return;
  stream_impl_->Close(base::Bind(&EmptyCompletionCallback));
}

int LocalFileReader::Read(net::IOBuffer* buf, int buf_len,
                          const net::CompletionCallback& callback) {
  DCHECK(!has_pending_open_);
  if (stream_impl_.get())
    return stream_impl_->Read(buf, buf_len, callback);
  return Open(base::Bind(&LocalFileReader::DidOpen, weak_factory_.GetWeakPtr(),
                         make_scoped_refptr(buf), buf_len, callback));
}

int LocalFileReader::GetLength(const net::Int64CompletionCallback& callback) {
  const bool posted = base::FileUtilProxy::GetFileInfo(
      file_thread_proxy_, file_path_,
      base::Bind(&DidGetFileInfoForGetLength, callback,
                 expected_modification_time_));
  DCHECK(posted);
  return net::ERR_IO_PENDING;
}

int LocalFileReader::Open(const OpenFileStreamCallback& callback) {
  DCHECK(!has_pending_open_);
  DCHECK(!stream_impl_.get());
  has_pending_open_ = true;
  OpenFileStreamHelper* helper = new OpenFileStreamHelper(file_thread_proxy_);
  const bool posted = file_thread_proxy_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&OpenFileStreamHelper::OpenAndVerifyOnFileThread,
                 base::Unretained(helper), file_path_,
                 expected_modification_time_),
      base::Bind(&OpenFileStreamHelper::OpenStreamOnCallingThread,
                 base::Owned(helper), initial_offset_, callback));
  DCHECK(posted);
  return net::ERR_IO_PENDING;
}

void LocalFileReader::DidOpen(net::IOBuffer* buf,
                              int buf_len,
                              const net::CompletionCallback& callback,
                              int open_error,
                              scoped_ptr<net::FileStream> stream_impl) {
  DCHECK(has_pending_open_);
  DCHECK(!stream_impl_.get());
  has_pending_open_ = false;
  if (open_error != net::OK) {
    callback.Run(open_error);
    return;
  }
  DCHECK(stream_impl.get());
  stream_impl_ = stream_impl.Pass();
  const int read_error = stream_impl_->Read(buf, buf_len, callback);
  if (read_error != net::ERR_IO_PENDING)
    callback.Run(read_error);
}

}  // namespace webkit_blob
