// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_DIR_URL_REQUEST_JOB_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_DIR_URL_REQUEST_JOB_H_
#pragma once

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/task.h"
#include "net/url_request/url_request_job.h"
#include "webkit/fileapi/file_system_url_request_job_base.h"

namespace fileapi {
class FileSystemContext;
class FileSystemOperation;

// A request job that handles reading filesystem: URLs for directories.
class FileSystemDirURLRequestJob : public FileSystemURLRequestJobBase {
 public:
  FileSystemDirURLRequestJob(
      net::URLRequest* request, FileSystemContext* file_system_context,
      scoped_refptr<base::MessageLoopProxy> file_thread_proxy);

  // URLRequestJob methods:
  virtual void Start();
  virtual void Kill();
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int* bytes_read);
  virtual bool GetCharset(std::string* charset);

  // FilterContext methods (via URLRequestJob):
  virtual bool GetMimeType(std::string* mime_type) const;
  // TODO(adamk): Implement GetResponseInfo and GetResponseCode to simulate
  // an HTTP response.

 protected:
  // FileSystemURLRequestJobBase methods.
  virtual void DidGetLocalPath(const FilePath& local_path);

  virtual ~FileSystemDirURLRequestJob();

  void DidReadDirectory(base::PlatformFileError error_code,
                        const std::vector<base::FileUtilProxy::Entry>& entries);
  fileapi::FileSystemOperation* GetNewOperation();

  std::string data_;
  ScopedRunnableMethodFactory<FileSystemDirURLRequestJob> method_factory_;
  base::ScopedCallbackFactory<FileSystemDirURLRequestJob> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemDirURLRequestJob);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_DIR_URL_REQUEST_JOB_H_
