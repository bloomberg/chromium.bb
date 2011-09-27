// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_writer_delegate.h"

#include "base/file_util_proxy.h"
#include "base/message_loop.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/net_errors.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_quota_util.h"
#include "webkit/fileapi/quota_file_util.h"

namespace fileapi {

static const int kReadBufSize = 32768;

namespace {

typedef Callback2<base::PlatformFileError /* error code */,
                  const base::PlatformFileInfo& /* file_info */
                  >::Type InitializeTaskCallback;

class InitializeTask : public base::RefCountedThreadSafe<InitializeTask> {
 public:
  InitializeTask(
      base::PlatformFile file,
      FileSystemOperationContext* context,
      InitializeTaskCallback* callback)
      : origin_message_loop_proxy_(
            base::MessageLoopProxy::current()),
        error_code_(base::PLATFORM_FILE_OK),
        file_(file),
        context_(*context),
        callback_(callback) {
    DCHECK(callback);
  }

  bool Start(scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
             const tracked_objects::Location& from_here) {
    return message_loop_proxy->PostTask(from_here, NewRunnableMethod(this,
        &InitializeTask::ProcessOnTargetThread));
  }

 private:
  friend class base::RefCountedThreadSafe<InitializeTask>;

  void RunCallback() {
    callback_->Run(error_code_, file_info_);
    delete callback_;
  }

  void ProcessOnTargetThread() {
    DCHECK(context_.file_system_context());
    FileSystemQuotaUtil* quota_util = context_.file_system_context()->
        GetQuotaUtil(context_.src_type());
    if (quota_util) {
      DCHECK(quota_util->proxy());
      quota_util->proxy()->StartUpdateOrigin(
          context_.src_origin_url(), context_.src_type());
    }
    if (!base::GetPlatformFileInfo(file_, &file_info_))
      error_code_ = base::PLATFORM_FILE_ERROR_FAILED;
    origin_message_loop_proxy_->PostTask(FROM_HERE, NewRunnableMethod(this,
        &InitializeTask::RunCallback));
  }

  scoped_refptr<base::MessageLoopProxy> origin_message_loop_proxy_;
  base::PlatformFileError error_code_;

  base::PlatformFile file_;
  FileSystemOperationContext context_;
  InitializeTaskCallback* callback_;

  base::PlatformFileInfo file_info_;
};

}  // namespace (anonymous)

FileWriterDelegate::FileWriterDelegate(
    FileSystemOperation* file_system_operation, int64 offset,
    scoped_refptr<base::MessageLoopProxy> proxy)
    : file_system_operation_(file_system_operation),
      file_(base::kInvalidPlatformFileValue),
      offset_(offset),
      proxy_(proxy),
      bytes_written_backlog_(0),
      bytes_written_(0),
      bytes_read_(0),
      total_bytes_written_(0),
      allowed_bytes_to_write_(0),
      io_buffer_(new net::IOBufferWithSize(kReadBufSize)),
      io_callback_(ALLOW_THIS_IN_INITIALIZER_LIST(this),
                   &FileWriterDelegate::OnDataWritten),
      method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

FileWriterDelegate::~FileWriterDelegate() {
}

void FileWriterDelegate::OnGetFileInfoAndCallStartUpdate(
    base::PlatformFileError error,
    const base::PlatformFileInfo& file_info) {
  if (error) {
    OnError(error);
    return;
  }
  int64 allowed_bytes_growth =
      file_system_operation_context()->allowed_bytes_growth();
  if (allowed_bytes_growth < 0)
    allowed_bytes_growth = 0;
  int64 overlap = file_info.size - offset_;
  allowed_bytes_to_write_ = allowed_bytes_growth;
  if (kint64max - overlap > allowed_bytes_growth)
    allowed_bytes_to_write_ += overlap;
  size_ = file_info.size;
  file_stream_.reset(new net::FileStream(file_,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE |
      base::PLATFORM_FILE_ASYNC));
  request_->Start();
}

void FileWriterDelegate::Start(base::PlatformFile file,
                               net::URLRequest* request) {
  file_ = file;
  request_ = request;

  scoped_refptr<InitializeTask> relay = new InitializeTask(
      file_, file_system_operation_context(),
      callback_factory_.NewCallback(
          &FileWriterDelegate::OnGetFileInfoAndCallStartUpdate));
  relay->Start(proxy_, FROM_HERE);
}

void FileWriterDelegate::OnReceivedRedirect(net::URLRequest* request,
                                            const GURL& new_url,
                                            bool* defer_redirect) {
  NOTREACHED();
  OnError(base::PLATFORM_FILE_ERROR_SECURITY);
}

void FileWriterDelegate::OnAuthRequired(net::URLRequest* request,
                                        net::AuthChallengeInfo* auth_info) {
  NOTREACHED();
  OnError(base::PLATFORM_FILE_ERROR_SECURITY);
}

void FileWriterDelegate::OnCertificateRequested(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  NOTREACHED();
  OnError(base::PLATFORM_FILE_ERROR_SECURITY);
}

void FileWriterDelegate::OnSSLCertificateError(net::URLRequest* request,
                                               const net::SSLInfo& ssl_info,
                                               bool is_hsts_host) {
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
  // allowed_bytes_to_write could be negative if the file size is
  // greater than the current (possibly new) quota.
  // (The UI should clear the entire origin data if the smaller quota size
  // is set in general, though the UI/deletion code is not there yet.)
  DCHECK(total_bytes_written_ <= allowed_bytes_to_write_ ||
         allowed_bytes_to_write_ < 0);
  if (total_bytes_written_ >= allowed_bytes_to_write_) {
    OnError(base::PLATFORM_FILE_ERROR_NO_SPACE);
    return;
  }

  int64 bytes_to_write = bytes_read_ - bytes_written_;
  if (bytes_to_write > allowed_bytes_to_write_ - total_bytes_written_)
    bytes_to_write = allowed_bytes_to_write_ - total_bytes_written_;

  int write_response = file_stream_->Write(io_buffer_->data() + bytes_written_,
                                           static_cast<int>(bytes_to_write),
                                           &io_callback_);
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
    total_bytes_written_ += write_response;
    if (bytes_written_ == bytes_read_)
      Read();
    else
      Write();
  } else {
    OnError(base::PLATFORM_FILE_ERROR_FAILED);
  }
}

void FileWriterDelegate::OnError(base::PlatformFileError error) {
  request_->set_delegate(NULL);
  request_->Cancel();

  if (quota_util()) {
    quota_util()->proxy()->EndUpdateOrigin(
        file_system_operation_context()->src_origin_url(),
        file_system_operation_context()->src_type());
  }

  file_system_operation_->DidWrite(error, 0, true);
}

void FileWriterDelegate::OnProgress(int bytes_written, bool done) {
  DCHECK(bytes_written + bytes_written_backlog_ >= bytes_written_backlog_);
  if (quota_util() &&
      bytes_written > 0 &&
      total_bytes_written_ + bytes_written + offset_ > size_) {
    int overlapped = 0;
    if (total_bytes_written_ + offset_ < size_)
      overlapped = size_ - total_bytes_written_ - offset_;
    quota_util()->proxy()->UpdateOriginUsage(
        file_system_operation_->file_system_context()->quota_manager_proxy(),
        file_system_operation_context()->src_origin_url(),
        file_system_operation_context()->src_type(),
        bytes_written - overlapped);
  }
  static const int kMinProgressDelayMS = 200;
  base::Time currentTime = base::Time::Now();
  if (done || last_progress_event_time_.is_null() ||
      (currentTime - last_progress_event_time_).InMilliseconds() >
          kMinProgressDelayMS) {
    bytes_written += bytes_written_backlog_;
    last_progress_event_time_ = currentTime;
    bytes_written_backlog_ = 0;
    if (done && quota_util()) {
      if (quota_util()) {
        quota_util()->proxy()->EndUpdateOrigin(
            file_system_operation_context()->src_origin_url(),
            file_system_operation_context()->src_type());
      }
    }
    file_system_operation_->DidWrite(
        base::PLATFORM_FILE_OK, bytes_written, done);
    return;
  }
  bytes_written_backlog_ += bytes_written;
}

FileSystemOperationContext*
FileWriterDelegate::file_system_operation_context() const {
  DCHECK(file_system_operation_);
  DCHECK(file_system_operation_->file_system_operation_context());
  return file_system_operation_->file_system_operation_context();
}

FileSystemQuotaUtil* FileWriterDelegate::quota_util() const {
  DCHECK(file_system_operation_);
  DCHECK(file_system_operation_->file_system_context());
  DCHECK(file_system_operation_->file_system_operation_context());
  return file_system_operation_->file_system_context()->GetQuotaUtil(
      file_system_operation_context()->src_type());
}

}  // namespace fileapi
