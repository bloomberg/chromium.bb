// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/task.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request_job.h"

class GURL;
class FilePath;

namespace net {
class FileStream;
}

namespace fileapi {
class FileSystemContext;

// A request job that handles reading filesystem: URLs
class FileSystemURLRequestJob : public net::URLRequestJob {
 public:
  FileSystemURLRequestJob(
      net::URLRequest* request,
      FileSystemContext* file_system_context,
      scoped_refptr<base::MessageLoopProxy> file_thread_proxy);

  // URLRequestJob methods:
  virtual void Start() OVERRIDE;
  virtual void Kill() OVERRIDE;
  virtual bool ReadRawData(net::IOBuffer* buf,
                           int buf_size,
                           int* bytes_read) OVERRIDE;
  virtual bool IsRedirectResponse(GURL* location,
                                  int* http_status_code) OVERRIDE;
  virtual void SetExtraRequestHeaders(
      const net::HttpRequestHeaders& headers) OVERRIDE;
  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE;
  virtual int GetResponseCode() const OVERRIDE;

  // FilterContext methods (via URLRequestJob):
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;

 private:
  class CallbackDispatcher;

  virtual ~FileSystemURLRequestJob();

  void StartAsync();
  void DidGetMetadata(const base::PlatformFileInfo& file_info,
                      const FilePath& platform_path);
  void DidOpen(base::PlatformFileError error_code,
               base::PassPlatformFile file, bool created);
  void DidRead(int result);
  void NotifyFailed(int rv);

  FileSystemContext* file_system_context_;
  scoped_refptr<base::MessageLoopProxy> file_thread_proxy_;
  base::WeakPtrFactory<FileSystemURLRequestJob> weak_factory_;
  scoped_ptr<net::FileStream> stream_;
  bool is_directory_;
  scoped_ptr<net::HttpResponseInfo> response_info_;
  int64 remaining_bytes_;
  net::HttpByteRange byte_range_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemURLRequestJob);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_H_
