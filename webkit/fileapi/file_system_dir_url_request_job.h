// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_DIR_URL_REQUEST_JOB_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_DIR_URL_REQUEST_JOB_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util_proxy.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "net/url_request/url_request_job.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/storage/webkit_storage_export.h"

namespace fileapi {
class FileSystemContext;
class FileSystemOperation;

// A request job that handles reading filesystem: URLs for directories.
class WEBKIT_STORAGE_EXPORT_PRIVATE FileSystemDirURLRequestJob
    : public net::URLRequestJob {
 public:
  FileSystemDirURLRequestJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      FileSystemContext* file_system_context);

  // URLRequestJob methods:
  virtual void Start() OVERRIDE;
  virtual void Kill() OVERRIDE;
  virtual bool ReadRawData(net::IOBuffer* buf,
                           int buf_size,
                           int* bytes_read) OVERRIDE;
  virtual bool GetCharset(std::string* charset) OVERRIDE;

  // FilterContext methods (via URLRequestJob):
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  // TODO(adamk): Implement GetResponseInfo and GetResponseCode to simulate
  // an HTTP response.

 private:
  class CallbackDispatcher;

  virtual ~FileSystemDirURLRequestJob();

  void StartAsync();
  void DidReadDirectory(base::PlatformFileError result,
                        const std::vector<base::FileUtilProxy::Entry>& entries,
                        bool has_more);
  FileSystemOperation* GetNewOperation(base::PlatformFileError* error_code);

  std::string data_;
  FileSystemURL url_;
  FileSystemContext* file_system_context_;
  base::WeakPtrFactory<FileSystemDirURLRequestJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemDirURLRequestJob);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_DIR_URL_REQUEST_JOB_H_
