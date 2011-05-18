// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_url_request_job_factory.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/url_request/url_request.h"
#include "webkit/fileapi/file_system_url_request_job.h"
#include "webkit/fileapi/file_system_dir_url_request_job.h"

namespace fileapi {

namespace {

class FileSystemProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  explicit FileSystemProtocolHandler(
      FileSystemContext* context,
      base::MessageLoopProxy* loop_proxy);
  virtual ~FileSystemProtocolHandler();

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request) const OVERRIDE;

 private:
  // No scoped_refptr because |file_system_context_| is owned by the
  // ProfileIOData, which also owns this ProtocolHandler.
  FileSystemContext* const file_system_context_;
  const scoped_refptr<base::MessageLoopProxy> file_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemProtocolHandler);
};

FileSystemProtocolHandler::FileSystemProtocolHandler(
    FileSystemContext* context,
    base::MessageLoopProxy* file_loop_proxy)
    : file_system_context_(context),
      file_loop_proxy_(file_loop_proxy) {
  DCHECK(file_system_context_);
  DCHECK(file_loop_proxy_);
}

FileSystemProtocolHandler::~FileSystemProtocolHandler() {}

net::URLRequestJob* FileSystemProtocolHandler::MaybeCreateJob(
    net::URLRequest* request) const {
  const std::string path = request->url().path();

  // If the path ends with a /, we know it's a directory. If the path refers
  // to a directory and gets dispatched to FileSystemURLRequestJob, that class
  // redirects back here, by adding a / to the URL.
  if (!path.empty() && path[path.size() - 1] == '/') {
    return new FileSystemDirURLRequestJob(
        request, file_system_context_, file_loop_proxy_);
  }
  return new FileSystemURLRequestJob(
      request, file_system_context_, file_loop_proxy_);
}

}  // anonymous namespace

net::URLRequestJobFactory::ProtocolHandler*
CreateFileSystemProtocolHandler(FileSystemContext* context,
                                base::MessageLoopProxy* loop_proxy) {
  DCHECK(context);
  DCHECK(loop_proxy);
  return new FileSystemProtocolHandler(context, loop_proxy);
}

}  // namespace fileapi
