// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_BASE_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_BASE_H_
#pragma once

#include "base/file_path.h"
#include "base/message_loop_proxy.h"
#include "net/url_request/url_request_job.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation.h"

namespace fileapi {

// A base class for request jobs that handle reading filesystem: URLs for
// files and directories.
class FileSystemURLRequestJobBase : public net::URLRequestJob {
 public:
  FileSystemURLRequestJobBase(
      net::URLRequest* request, FileSystemContext* file_system_context,
      scoped_refptr<base::MessageLoopProxy> file_thread_proxy);

  virtual ~FileSystemURLRequestJobBase();

  void StartAsync();

 protected:
  virtual void DidGetLocalPath(const FilePath& local_path) = 0 ;

  void NotifyFailed(int rv);
  FileSystemOperation* GetNewOperation();

  FilePath relative_file_path_;
  FilePath absolute_file_path_;
  scoped_refptr<FileSystemContext> file_system_context_;
  scoped_refptr<base::MessageLoopProxy> file_thread_proxy_;

 private:
  friend class LocalPathCallbackDispatcher;
  void OnGetLocalPath(const FilePath& local_path);
  DISALLOW_COPY_AND_ASSIGN(FileSystemURLRequestJobBase);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_BASE_H_
