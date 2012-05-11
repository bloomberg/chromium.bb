// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_writer_delegate.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util_proxy.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/net_errors.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_quota_util.h"

namespace fileapi {

static const int kReadBufSize = 32768;

namespace {

typedef base::Callback<void(base::PlatformFileError /* error code */,
                            const base::PlatformFileInfo& /* file_info */)>
    InitializeTaskCallback;

class InitializeTask : public base::RefCountedThreadSafe<InitializeTask> {
 public:
  InitializeTask(
      base::PlatformFile file,
      const InitializeTaskCallback& callback)
      : original_loop_(base::MessageLoopProxy::current()),
        error_code_(base::PLATFORM_FILE_OK),
        file_(file),
        callback_(callback) {
    DCHECK_EQ(false, callback.is_null());
  }

  bool Start(base::SequencedTaskRunner* task_runner,
             const tracked_objects::Location& from_here) {
    return task_runner->PostTask(
        from_here,
        base::Bind(&InitializeTask::ProcessOnTargetThread, this));
  }

 private:
  friend class base::RefCountedThreadSafe<InitializeTask>;
  ~InitializeTask() {}

  void RunCallback() {
    callback_.Run(error_code_, file_info_);
  }

  void ProcessOnTargetThread() {
    if (!base::GetPlatformFileInfo(file_, &file_info_))
      error_code_ = base::PLATFORM_FILE_ERROR_FAILED;
    original_loop_->PostTask(
        FROM_HERE,
        base::Bind(&InitializeTask::RunCallback, this));
  }

  scoped_refptr<base::MessageLoopProxy> original_loop_;
  base::PlatformFileError error_code_;

  base::PlatformFile file_;
  InitializeTaskCallback callback_;

  base::PlatformFileInfo file_info_;
};

}  // namespace (anonymous)

FileWriterDelegate::FileWriterDelegate(
    FileSystemOperation* file_system_operation,
    const FileSystemPath& path,
    int64 offset)
    : file_system_operation_(file_system_operation),
      file_(base::kInvalidPlatformFileValue),
      path_(path),
      offset_(offset),
      has_pending_write_(false),
      bytes_written_backlog_(0),
      bytes_written_(0),
      bytes_read_(0),
      total_bytes_written_(0),
      allowed_bytes_to_write_(0),
      io_buffer_(new net::IOBufferWithSize(kReadBufSize)),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

FileWriterDelegate::~FileWriterDelegate() {
}

void FileWriterDelegate::OnGetFileInfoAndStartRequest(
    scoped_ptr<net::URLRequest> request,
    base::PlatformFileError error,
    const base::PlatformFileInfo& file_info) {
  if (error != base::PLATFORM_FILE_OK) {
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
  file_stream_.reset(new net::FileStream(
      file_,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE |
      base::PLATFORM_FILE_ASYNC,
      NULL));
  DCHECK(!request_.get());
  request_ = request.Pass();
  request_->Start();
}

void FileWriterDelegate::Start(base::PlatformFile file,
                               scoped_ptr<net::URLRequest> request) {
  file_ = file;

  scoped_refptr<InitializeTask> relay = new InitializeTask(
      file_,
      base::Bind(&FileWriterDelegate::OnGetFileInfoAndStartRequest,
                 weak_factory_.GetWeakPtr(), base::Passed(&request)));
  relay->Start(file_system_operation_context()->file_task_runner(), FROM_HERE);
}

bool FileWriterDelegate::Cancel() {
  if (request_.get()) {
    // This halts any callbacks on this delegate.
    request_->set_delegate(NULL);
    request_->Cancel();
  }

  // Return true to finish immediately if we're not writing.
  // Otherwise we'll do the final cleanup in the write callback.
  return !has_pending_write_;
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
                                               bool fatal) {
  NOTREACHED();
  OnError(base::PLATFORM_FILE_ERROR_SECURITY);
}

void FileWriterDelegate::OnResponseStarted(net::URLRequest* request) {
  DCHECK_EQ(request_.get(), request);
  // file_stream_->Seek() blocks the IO thread.
  // See http://crbug.com/75548.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  if (!request->status().is_success() || request->GetResponseCode() != 200) {
    OnError(base::PLATFORM_FILE_ERROR_FAILED);
    return;
  }
  int64 error = file_stream_->SeekSync(net::FROM_BEGIN, offset_);
  if (error != offset_) {
    OnError(base::PLATFORM_FILE_ERROR_FAILED);
    return;
  }
  Read();
}

void FileWriterDelegate::OnReadCompleted(net::URLRequest* request,
                                         int bytes_read) {
  DCHECK_EQ(request_.get(), request);
  if (!request->status().is_success()) {
    OnError(base::PLATFORM_FILE_ERROR_FAILED);
    return;
  }
  OnDataReceived(bytes_read);
}

void FileWriterDelegate::Read() {
  bytes_written_ = 0;
  bytes_read_ = 0;
  if (request_->Read(io_buffer_.get(), io_buffer_->size(),
                     &bytes_read_)) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&FileWriterDelegate::OnDataReceived,
                   weak_factory_.GetWeakPtr(), bytes_read_));
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
    cursor_ = new net::DrainableIOBuffer(io_buffer_, bytes_read_);
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

  has_pending_write_ = true;
  int write_response =
      file_stream_->Write(cursor_,
                          static_cast<int>(bytes_to_write),
                          base::Bind(&FileWriterDelegate::OnDataWritten,
                                     weak_factory_.GetWeakPtr()));
  if (write_response > 0)
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&FileWriterDelegate::OnDataWritten,
                   weak_factory_.GetWeakPtr(), write_response));
  else if (net::ERR_IO_PENDING != write_response)
    OnError(base::PLATFORM_FILE_ERROR_FAILED);
}

void FileWriterDelegate::OnDataWritten(int write_response) {
  has_pending_write_ = false;
  if (write_response > 0) {
    if (request_->status().status() == net::URLRequestStatus::CANCELED) {
      OnProgress(write_response, true);
      return;
    }
    OnProgress(write_response, false);
    cursor_->DidConsume(write_response);
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
  if (request_.get()) {
    request_->set_delegate(NULL);
    request_->Cancel();
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
        path_.origin(), path_.type(),
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
    if (done && quota_util())
      quota_util()->proxy()->EndUpdateOrigin(path_.origin(), path_.type());
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
      path_.type());
}

}  // namespace fileapi
