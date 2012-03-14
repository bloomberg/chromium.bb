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

int PlatformFileErrorToNetError(base::PlatformFileError file_error) {
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

// Verify if the underlying file has not been modified.
bool VerifySnapshotTime(const base::Time& expected_modification_time,
                        const base::PlatformFileInfo& file_info) {
  return expected_modification_time.is_null() ||
         expected_modification_time.ToTimeT() ==
             file_info.last_modified.ToTimeT();
}

void DidGetFileInfoForGetLength(const net::CompletionCallback& callback,
                                const base::Time& expected_modification_time,
                                int64 initial_offset,
                                base::PlatformFileError error,
                                const base::PlatformFileInfo& file_info) {
  if (file_info.is_directory) {
    callback.Run(net::ERR_FILE_NOT_FOUND);
    return;
  }
  if (error != base::PLATFORM_FILE_OK) {
    callback.Run(PlatformFileErrorToNetError(error));
    return;
  }
  if (!VerifySnapshotTime(expected_modification_time, file_info)) {
    callback.Run(net::ERR_UPLOAD_FILE_CHANGED);
    return;
  }
  callback.Run(file_info.size - initial_offset);
}

}  // namespace

class LocalFileReader::OpenFileStreamHelper {
 public:
  OpenFileStreamHelper() : result_(net::OK) {}

  void OpenAndSeekOnFileThread(const FilePath& file_path,
                               int64 initial_offset,
                               const base::Time& expected_modification_time) {
    base::PlatformFileInfo file_info;
    const bool result = file_util::GetFileInfo(file_path, &file_info);
    if (!result) {
      result_ = net::ERR_FAILED;
      return;
    }

    if (!VerifySnapshotTime(expected_modification_time, file_info)) {
      result_ = net::ERR_UPLOAD_FILE_CHANGED;
      return;
    }

    stream_impl_.reset(new net::FileStream(NULL));
    result_ = stream_impl_->OpenSync(file_path, kOpenFlagsForRead);
    if (result_ != net::OK) {
      stream_impl_.reset();
      return;
    }

    const int64 returned_offset = stream_impl_->Seek(
        net::FROM_BEGIN, initial_offset);
    if (returned_offset != initial_offset) {
      result_ = net::ERR_FAILED;
      return;
    }

    result_ = net::OK;
  }

  void Reply(const OpenFileStreamCallback& callback) {
    callback.Run(result_, stream_impl_.Pass());
  }

 private:
  base::WeakPtr<LocalFileReader> stream_;
  scoped_ptr<net::FileStream> stream_impl_;
  int result_;
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

LocalFileReader::~LocalFileReader() {}

int LocalFileReader::Read(net::IOBuffer* buf, int buf_len,
                          const net::CompletionCallback& callback) {
  DCHECK(!has_pending_open_);
  if (stream_impl_.get())
    return stream_impl_->Read(buf, buf_len, callback);
  return Open(base::Bind(&LocalFileReader::DidOpen, weak_factory_.GetWeakPtr(),
                         make_scoped_refptr(buf), buf_len, callback));
}

int LocalFileReader::GetLength(const net::CompletionCallback& callback) {
  const bool posted = base::FileUtilProxy::GetFileInfo(
      file_thread_proxy_, file_path_,
      base::Bind(&DidGetFileInfoForGetLength, callback,
                 expected_modification_time_, initial_offset_));
  DCHECK(posted);
  return net::ERR_IO_PENDING;
}

int LocalFileReader::Open(const OpenFileStreamCallback& callback) {
  DCHECK(!has_pending_open_);
  DCHECK(!stream_impl_.get());
  has_pending_open_ = true;
  OpenFileStreamHelper* helper = new OpenFileStreamHelper;
  const bool posted = file_thread_proxy_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&OpenFileStreamHelper::OpenAndSeekOnFileThread,
                 base::Unretained(helper), file_path_, initial_offset_,
                 expected_modification_time_),
      base::Bind(&OpenFileStreamHelper::Reply, base::Owned(helper), callback));
  DCHECK(posted);
  return net::ERR_IO_PENDING;
}

void LocalFileReader::DidOpen(net::IOBuffer* buf,
                              int buf_len,
                              const net::CompletionCallback& callback,
                              int error,
                              scoped_ptr<net::FileStream> stream_impl) {
  DCHECK(has_pending_open_);
  DCHECK(!stream_impl_.get());
  stream_impl_ = stream_impl.Pass();
  has_pending_open_ = false;
  error = stream_impl_->Read(buf, buf_len, callback);
  if (error != net::ERR_IO_PENDING)
    callback.Run(error);
}

}  // namespace webkit_blob
