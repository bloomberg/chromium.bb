// Copyright (c) 2006-2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/scoped_callback_factory.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "googleurl/src/gurl.h"
#include "net/base/completion_callback.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request_job.h"

namespace net {
class FileStream;
}

namespace fileapi {
class FileSystemPathManager;

// A request job that handles reading filesystem: URLs
class FileSystemURLRequestJob : public net::URLRequestJob {
 public:
  FileSystemURLRequestJob(
      net::URLRequest* request, FileSystemPathManager* path_manager,
      scoped_refptr<base::MessageLoopProxy> file_thread_proxy);

  // URLRequestJob methods:
  virtual void Start();
  virtual void Kill();
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int* bytes_read);
  virtual bool IsRedirectResponse(GURL* location, int* http_status_code);
  virtual bool GetMimeType(std::string* mime_type) const;
  virtual void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers);
  // TODO(adamk): Implement the rest of the methods required to simulate HTTP.

 private:
  virtual ~FileSystemURLRequestJob();

  void StartAsync();
  void DidGetRootPath(bool success, const FilePath& root_path,
                      const std::string& name);
  void DidResolve(base::PlatformFileError error_code,
                  const base::PlatformFileInfo& file_info);
  void DidOpen(base::PlatformFileError error_code,
               base::PassPlatformFile file, bool created);
  void DidRead(int result);

  void NotifyFailed(int rv);

  FilePath relative_file_path_;
  FilePath absolute_file_path_;
  GURL origin_url_;
  FileSystemPathManager* const path_manager_;

  net::CompletionCallbackImpl<FileSystemURLRequestJob> io_callback_;
  scoped_ptr<net::FileStream> stream_;
  bool is_directory_;

  net::HttpByteRange byte_range_;
  int64 remaining_bytes_;
  int startup_error_;

  ScopedRunnableMethodFactory<FileSystemURLRequestJob> method_factory_;
  base::ScopedCallbackFactory<FileSystemURLRequestJob> callback_factory_;
  scoped_refptr<base::MessageLoopProxy> file_thread_proxy_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemURLRequestJob);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_H_
