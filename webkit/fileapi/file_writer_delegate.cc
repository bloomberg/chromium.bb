// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_writer_delegate.h"

#include "base/message_loop.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/net_errors.h"
#include "webkit/fileapi/file_system_operation.h"

namespace fileapi {

static const int kReadBufSize = 32768;

FileWriterDelegate::FileWriterDelegate(
    FileSystemOperation* file_system_operation,
    int64 offset)
    : file_system_operation_(file_system_operation),
      file_(base::kInvalidPlatformFileValue),
      offset_(offset),
      bytes_read_backlog_(0),
      bytes_written_(0),
      bytes_read_(0),
      io_buffer_(new net::IOBufferWithSize(kReadBufSize)),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

FileWriterDelegate::~FileWriterDelegate() {
}

void FileWriterDelegate::Start(base::PlatformFile file,
                               net::URLRequest* request) {
  file_ = file;
  request_ = request;
  file_stream_.reset(
      new net::FileStream(
        file,
        base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE |
            base::PLATFORM_FILE_ASYNC));
  request_->Start();
}

void FileWriterDelegate::OnReceivedRedirect(
    net::URLRequest* request, const GURL& new_url, bool* defer_redirect) {
  NOTREACHED();
  OnError(base::PLATFORM_FILE_ERROR_SECURITY);
}

void FileWriterDelegate::OnAuthRequired(
    net::URLRequest* request, net::AuthChallengeInfo* auth_info) {
  NOTREACHED();
  OnError(base::PLATFORM_FILE_ERROR_SECURITY);
}

void FileWriterDelegate::OnCertificateRequested(
    net::URLRequest* request, net::SSLCertRequestInfo* cert_request_info) {
  NOTREACHED();
  OnError(base::PLATFORM_FILE_ERROR_SECURITY);
}

void FileWriterDelegate::OnSSLCertificateError(
    net::URLRequest* request, int cert_error, net::X509Certificate* cert) {
  NOTREACHED();
  OnError(base::PLATFORM_FILE_ERROR_SECURITY);
}

void FileWriterDelegate::OnResponseStarted(net::URLRequest* request) {
  DCHECK_EQ(request_, request);
  // file_stream_->Seek() blocks the IO thread.
  // See http://crbug.com/75548.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  if (!request->status().is_success()) {
    OnError(base::PLATFORM_FILE_ERROR_FAILED);
    return;
  }
  int64 error = file_stream_->Seek(net::FROM_BEGIN, offset_);
  if (error != offset_) {
    OnError(base::PLATFORM_FILE_ERROR_FAILED);
    return;
  }
  Read();
}

void FileWriterDelegate::OnReadCompleted(net::URLRequest* request,
                                         int bytes_read) {
  DCHECK_EQ(request_, request);
  if (!request->status().is_success()) {
    OnError(base::PLATFORM_FILE_ERROR_FAILED);
    return;
  }
  OnDataReceived(bytes_read);
}

void FileWriterDelegate::Read() {
  bytes_written_ = 0;
  bytes_read_ = 0;
  if (request_->Read(io_buffer_.get(), io_buffer_->size(), &bytes_read_)) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        method_factory_.NewRunnableMethod(
            &FileWriterDelegate::OnDataReceived, bytes_read_));
  } else if (!request_->status().is_io_pending()) {
    OnError(base::PLATFORM_FILE_ERROR_FAILED);
  }
}

void FileWriterDelegate::OnDataReceived(int bytes_read) {
  bytes_read_ = bytes_read;
  if (!bytes_read_) {  // We're done.
    OnProgress(0, true);
  } else {
    // This could easily be optimized to rotate between a pool of buffers, so
    // that we could read and write at the same time.  It's not yet clear that
    // it's necessary.
    Write();
  }
}

void FileWriterDelegate::Write() {
  int write_response = file_stream_->Write(
        io_buffer_->data() + bytes_written_,
        bytes_read_ - bytes_written_,
        callback_factory_.NewCallback(&FileWriterDelegate::OnDataWritten));
  if (write_response > 0)
    MessageLoop::current()->PostTask(
        FROM_HERE,
        method_factory_.NewRunnableMethod(
            &FileWriterDelegate::OnDataWritten, write_response));
  else if (net::ERR_IO_PENDING != write_response)
    OnError(base::PLATFORM_FILE_ERROR_FAILED);
}

void FileWriterDelegate::OnDataWritten(int write_response) {
  if (write_response > 0) {
    OnProgress(write_response, false);
    bytes_written_ += write_response;
    if (bytes_written_ == bytes_read_)
      Read();
    else
      Write();
  } else {
    OnError(base::PLATFORM_FILE_ERROR_FAILED);
  }
}

void FileWriterDelegate::OnError(base::PlatformFileError error) {
  request_->Cancel();
  file_system_operation_->DidWrite(error, 0, true);
}

void FileWriterDelegate::OnProgress(int bytes_read, bool done) {
  DCHECK(bytes_read + bytes_read_backlog_ >= bytes_read_backlog_);
  static const int kMinProgressDelayMS = 200;
  base::Time currentTime = base::Time::Now();
  if (done || last_progress_event_time_.is_null() ||
      (currentTime - last_progress_event_time_).InMilliseconds() >
          kMinProgressDelayMS) {
    bytes_read += bytes_read_backlog_;
    last_progress_event_time_ = currentTime;
    bytes_read_backlog_ = 0;
    file_system_operation_->DidWrite(
        base::PLATFORM_FILE_OK, bytes_read, done);
    return;
  }
  bytes_read_backlog_ += bytes_read;
}

}  // namespace fileapi
