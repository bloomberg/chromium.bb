// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SANDBOX_FILE_STREAM_WRITER_H_
#define WEBKIT_FILEAPI_SANDBOX_FILE_STREAM_WRITER_H_

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_stream_writer.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/fileapi_export.h"
#include "webkit/fileapi/task_runner_bound_observer_list.h"
#include "webkit/quota/quota_types.h"

namespace fileapi {

class FileSystemContext;
class FileSystemQuotaUtil;
class LocalFileStreamWriter;

class FILEAPI_EXPORT_PRIVATE SandboxFileStreamWriter : public FileStreamWriter {
 public:
  SandboxFileStreamWriter(FileSystemContext* file_system_context,
                          const FileSystemURL& url,
                          int64 initial_offset,
                          const UpdateObserverList& observers);
  virtual ~SandboxFileStreamWriter();

  // FileStreamWriter overrides.
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback) OVERRIDE;
  virtual int Cancel(const net::CompletionCallback& callback) OVERRIDE;

  // Used only by tests.
  void set_default_quota(int64 quota) {
    default_quota_ = quota;
  }

 private:
  // Performs quota calculation and calls local_file_writer_->Write().
  int WriteInternal(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback);

  // Callbacks that are chained for the first write.  This eventually calls
  // WriteInternal.
  void DidGetFileInfo(const net::CompletionCallback& callback,
                      base::PlatformFileError file_error,
                      const base::PlatformFileInfo& file_info,
                      const FilePath& platform_path);
  void DidGetUsageAndQuota(const net::CompletionCallback& callback,
                           quota::QuotaStatusCode status,
                           int64 usage, int64 quota);
  void DidInitializeForWrite(net::IOBuffer* buf, int buf_len,
                             const net::CompletionCallback& callback,
                             int init_status);

  void DidWrite(const net::CompletionCallback& callback, int write_response);

  // Stops the in-flight operation, calls |cancel_callback_| and returns true
  // if there's a pending cancel request.
  bool CancelIfRequested();

  scoped_refptr<FileSystemContext> file_system_context_;
  FileSystemURL url_;
  int64 initial_offset_;
  scoped_ptr<LocalFileStreamWriter> local_file_writer_;
  net::CompletionCallback cancel_callback_;

  UpdateObserverList observers_;

  FilePath file_path_;
  int64 file_size_;
  int64 total_bytes_written_;
  int64 allowed_bytes_to_write_;
  bool has_pending_operation_;

  int64 default_quota_;

  base::WeakPtrFactory<SandboxFileStreamWriter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SandboxFileStreamWriter);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SANDBOX_FILE_STREAM_WRITER_H_
