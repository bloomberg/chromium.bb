// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "webkit/fileapi/file_system_url_request_job_base.h"

#include "base/message_loop.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"

using net::URLRequest;
using net::URLRequestJob;
using net::URLRequestStatus;

namespace fileapi {

class LocalPathCallbackDispatcher : public FileSystemCallbackDispatcher {
 public:
  explicit LocalPathCallbackDispatcher(FileSystemURLRequestJobBase* job)
      : job_(job) {
    DCHECK(job_);
  }

  // fileapi::FileSystemCallbackDispatcher overrides.
  virtual void DidSucceed() OVERRIDE {
    NOTREACHED();
  }

  virtual void DidGetLocalPath(const FilePath& local_path) {
    job_->OnGetLocalPath(local_path);
  }

  virtual void DidReadMetadata(const base::PlatformFileInfo& info,
                               const FilePath& unused) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidWrite(int64 bytes, bool complete) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidOpenFileSystem(const std::string& name,
                                 const GURL& root_path) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidFail(base::PlatformFileError error_code) OVERRIDE {
    int rv = net::ERR_FILE_NOT_FOUND;
    if (error_code == base::PLATFORM_FILE_ERROR_INVALID_URL)
      rv = net::ERR_INVALID_URL;
    job_->NotifyFailed(rv);
  }

 private:
  FileSystemURLRequestJobBase* job_;
  DISALLOW_COPY_AND_ASSIGN(LocalPathCallbackDispatcher);
};

FileSystemURLRequestJobBase::FileSystemURLRequestJobBase(
      URLRequest* request, FileSystemContext* file_system_context,
      scoped_refptr<base::MessageLoopProxy> file_thread_proxy)
    : URLRequestJob(request),
      file_system_context_(file_system_context),
      file_thread_proxy_(file_thread_proxy) {
}

FileSystemURLRequestJobBase::~FileSystemURLRequestJobBase() {
}

FileSystemOperation* FileSystemURLRequestJobBase::GetNewOperation() {
  LocalPathCallbackDispatcher* dispatcher =
      new LocalPathCallbackDispatcher(this);
  FileSystemOperation* operation = new FileSystemOperation(
      dispatcher,
      file_thread_proxy_,
      file_system_context_,
      NULL);
  return operation;
}

void FileSystemURLRequestJobBase::StartAsync() {
  GetNewOperation()->GetLocalPath(request_->url());
}

void FileSystemURLRequestJobBase::NotifyFailed(int rv) {
  NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
}

void FileSystemURLRequestJobBase::OnGetLocalPath(
    const FilePath& local_path) {
  DidGetLocalPath(local_path);
}

}  // namespace fileapi
