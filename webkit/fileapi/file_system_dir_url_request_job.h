// Copyright (c) 2006-2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_DIR_URL_REQUEST_JOB_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_DIR_URL_REQUEST_JOB_H_
#pragma once

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/scoped_callback_factory.h"
#include "base/task.h"
#include "net/url_request/url_request_job.h"

namespace fileapi {
class FileSystemPathManager;

// A request job that handles reading filesystem: URLs for directories.
class FileSystemDirURLRequestJob : public net::URLRequestJob {
 public:
  FileSystemDirURLRequestJob(
      net::URLRequest* request, FileSystemPathManager* path_manager,
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

 private:
  virtual ~FileSystemDirURLRequestJob();

  void StartAsync();
  void DidGetRootPath(bool success, const FilePath& root_path,
                      const std::string& name);
  void DidReadDirectory(base::PlatformFileError error_code,
                        const std::vector<base::FileUtilProxy::Entry>& entries);

  void NotifyFailed(int rv);

  std::string data_;
  FilePath relative_dir_path_;
  FilePath absolute_dir_path_;
  FileSystemPathManager* const path_manager_;

  ScopedRunnableMethodFactory<FileSystemDirURLRequestJob> method_factory_;
  base::ScopedCallbackFactory<FileSystemDirURLRequestJob> callback_factory_;
  scoped_refptr<base::MessageLoopProxy> file_thread_proxy_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemDirURLRequestJob);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_DIR_URL_REQUEST_JOB_H_
