// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_WRITER_DELEGATE_H_
#define WEBKIT_FILEAPI_FILE_WRITER_DELEGATE_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "base/time.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/storage/webkit_storage_export.h"

namespace fileapi {

class FileStreamWriter;
class FileSystemOperationContext;
class FileSystemQuotaUtil;

class WEBKIT_STORAGE_EXPORT_PRIVATE FileWriterDelegate
    : public net::URLRequest::Delegate {
 public:
  enum WriteProgressStatus {
    SUCCESS_IO_PENDING,
    SUCCESS_COMPLETED,
    ERROR_WRITE_STARTED,
    ERROR_WRITE_NOT_STARTED,
  };

  typedef base::Callback<void(base::PlatformFileError result,
                              int64 bytes,
                              WriteProgressStatus write_status)>
      DelegateWriteCallback;

  FileWriterDelegate(
      const DelegateWriteCallback& write_callback,
      scoped_ptr<FileStreamWriter> file_writer);
  virtual ~FileWriterDelegate();

  void Start(scoped_ptr<net::URLRequest> request);

  // Cancels the current write operation.  Returns true if it is ok to
  // delete this instance immediately.  Otherwise this will call
  // |write_operation|->DidWrite() with complete=true to let the operation
  // perform the final cleanup.
  bool Cancel();

  virtual void OnReceivedRedirect(net::URLRequest* request,
                                  const GURL& new_url,
                                  bool* defer_redirect) OVERRIDE;
  virtual void OnAuthRequired(net::URLRequest* request,
                              net::AuthChallengeInfo* auth_info) OVERRIDE;
  virtual void OnCertificateRequested(
      net::URLRequest* request,
      net::SSLCertRequestInfo* cert_request_info) OVERRIDE;
  virtual void OnSSLCertificateError(net::URLRequest* request,
                                     const net::SSLInfo& ssl_info,
                                     bool fatal) OVERRIDE;
  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE;
  virtual void OnReadCompleted(net::URLRequest* request,
                               int bytes_read) OVERRIDE;

 private:
  void OnGetFileInfoAndStartRequest(
      scoped_ptr<net::URLRequest> request,
      base::PlatformFileError error,
      const base::PlatformFileInfo& file_info);
  void Read();
  void OnDataReceived(int bytes_read);
  void Write();
  void OnDataWritten(int write_response);
  void OnError(base::PlatformFileError error);
  void OnProgress(int bytes_read, bool done);
  void OnWriteCancelled(int status);
  void FlushForCompletion(base::PlatformFileError error,
                          int bytes_written,
                          WriteProgressStatus progress_status);
  void OnFlushed(base::PlatformFileError error,
                 int bytes_written,
                 WriteProgressStatus progress_status,
                 int flush_error);

  FileSystemQuotaUtil* quota_util() const;
  WriteProgressStatus GetCompletionStatusOnError() const;

  DelegateWriteCallback write_callback_;
  scoped_ptr<FileStreamWriter> file_stream_writer_;
  base::Time last_progress_event_time_;
  bool writing_started_;
  int bytes_written_backlog_;
  int bytes_written_;
  int bytes_read_;
  scoped_refptr<net::IOBufferWithSize> io_buffer_;
  scoped_refptr<net::DrainableIOBuffer> cursor_;
  scoped_ptr<net::URLRequest> request_;
  base::WeakPtrFactory<FileWriterDelegate> weak_factory_;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_WRITER_DELEGATE_H_
