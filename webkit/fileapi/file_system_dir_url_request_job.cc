// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_dir_url_request_job.h"

#include <algorithm>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_util_proxy.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_util.h"

using net::URLRequest;
using net::URLRequestJob;
using net::URLRequestStatus;

namespace fileapi {

static FilePath GetRelativePath(const GURL& url) {
  FilePath relative_path;
  GURL unused_url;
  FileSystemType unused_type;
  CrackFileSystemURL(url, &unused_url, &unused_type, &relative_path);
  return relative_path;
}

class FileSystemDirURLRequestJob::CallbackDispatcher
    : public FileSystemCallbackDispatcher {
 public:
  explicit CallbackDispatcher(FileSystemDirURLRequestJob* job)
      : job_(job) {
    DCHECK(job_);
  }

  // fileapi::FileSystemCallbackDispatcher overrides.
  virtual void DidSucceed() OVERRIDE {
    NOTREACHED();
  }

  virtual void DidReadMetadata(const base::PlatformFileInfo& file_info,
                               const FilePath& platform_path) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more) OVERRIDE {
    job_->DidReadDirectory(entries, has_more);
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
    job_->NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
  }

 private:
  // TODO(adamk): Get rid of the need for refcounting here by
  // allowing FileSystemOperations to be cancelled.
  scoped_refptr<FileSystemDirURLRequestJob> job_;
  DISALLOW_COPY_AND_ASSIGN(CallbackDispatcher);
};

FileSystemDirURLRequestJob::FileSystemDirURLRequestJob(
    URLRequest* request, FileSystemContext* file_system_context,
    scoped_refptr<base::MessageLoopProxy> file_thread_proxy)
    : URLRequestJob(request),
      file_system_context_(file_system_context),
      file_thread_proxy_(file_thread_proxy),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

FileSystemDirURLRequestJob::~FileSystemDirURLRequestJob() {
}

bool FileSystemDirURLRequestJob::ReadRawData(net::IOBuffer* dest, int dest_size,
                                             int *bytes_read) {

  int count = std::min(dest_size, static_cast<int>(data_.size()));
  if (count > 0) {
    memcpy(dest->data(), data_.data(), count);
    data_.erase(0, count);
  }
  *bytes_read = count;
  return true;
}

void FileSystemDirURLRequestJob::Start() {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FileSystemDirURLRequestJob::StartAsync,
                 weak_factory_.GetWeakPtr()));
}

void FileSystemDirURLRequestJob::Kill() {
  URLRequestJob::Kill();
  weak_factory_.InvalidateWeakPtrs();
}

bool FileSystemDirURLRequestJob::GetMimeType(std::string* mime_type) const {
  *mime_type = "text/html";
  return true;
}

bool FileSystemDirURLRequestJob::GetCharset(std::string* charset) {
  *charset = "utf-8";
  return true;
}

void FileSystemDirURLRequestJob::StartAsync() {
  if (request_)
    GetNewOperation()->ReadDirectory(request_->url());
}

void FileSystemDirURLRequestJob::DidReadDirectory(
    const std::vector<base::FileUtilProxy::Entry>& entries,
    bool has_more) {
  if (!request_)
    return;

  if (data_.empty()) {
    FilePath relative_path = GetRelativePath(request_->url());
#if defined(OS_WIN)
    const string16& title = relative_path.value();
#elif defined(OS_POSIX)
    const string16& title = ASCIIToUTF16("/") +
        WideToUTF16(base::SysNativeMBToWide(relative_path.value()));
#endif
    data_.append(net::GetDirectoryListingHeader(title));
  }

  typedef std::vector<base::FileUtilProxy::Entry>::const_iterator EntryIterator;
  for (EntryIterator it = entries.begin(); it != entries.end(); ++it) {
#if defined(OS_WIN)
    const string16& name = it->name;
#elif defined(OS_POSIX)
    const string16& name =
        WideToUTF16(base::SysNativeMBToWide(it->name));
#endif
    data_.append(net::GetDirectoryListingEntry(
        name, std::string(), it->is_directory, it->size,
        it->last_modified_time));
  }

  if (has_more) {
    GetNewOperation()->ReadDirectory(request_->url());
  } else {
    set_expected_content_size(data_.size());
    NotifyHeadersComplete();
  }
}

FileSystemOperation* FileSystemDirURLRequestJob::GetNewOperation() {
  return new FileSystemOperation(new CallbackDispatcher(this),
                                 file_thread_proxy_,
                                 file_system_context_);
}

}  // namespace fileapi
