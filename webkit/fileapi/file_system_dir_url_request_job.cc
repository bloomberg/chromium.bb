// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_dir_url_request_job.h"

#include <algorithm>

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
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_util.h"

using net::URLRequest;
using net::URLRequestJob;
using net::URLRequestStatus;

namespace fileapi {

FileSystemDirURLRequestJob::FileSystemDirURLRequestJob(
    URLRequest* request, FileSystemPathManager* path_manager,
    scoped_refptr<base::MessageLoopProxy> file_thread_proxy)
    : URLRequestJob(request),
      path_manager_(path_manager),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(callback_factory_(this)),
      file_thread_proxy_(file_thread_proxy) {
}

FileSystemDirURLRequestJob::~FileSystemDirURLRequestJob() {
}

void FileSystemDirURLRequestJob::Start() {
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(
          &FileSystemDirURLRequestJob::StartAsync));
}

void FileSystemDirURLRequestJob::Kill() {
  URLRequestJob::Kill();
  callback_factory_.RevokeAll();
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

bool FileSystemDirURLRequestJob::GetMimeType(std::string* mime_type) const {
  *mime_type = "text/html";
  return true;
}

bool FileSystemDirURLRequestJob::GetCharset(std::string* charset) {
  *charset = "utf-8";
  return true;
}

void FileSystemDirURLRequestJob::StartAsync() {
  GURL origin_url;
  FileSystemType type;
  if (!CrackFileSystemURL(request_->url(), &origin_url, &type,
                          &relative_dir_path_)) {
    NotifyFailed(net::ERR_INVALID_URL);
    return;
  }

  path_manager_->GetFileSystemRootPath(
      origin_url, type, false,  // create
      callback_factory_.NewCallback(
          &FileSystemDirURLRequestJob::DidGetRootPath));
}

void FileSystemDirURLRequestJob::DidGetRootPath(bool success,
                                                const FilePath& root_path,
                                                const std::string& name) {
  if (!success) {
    NotifyFailed(net::ERR_FILE_NOT_FOUND);
    return;
  }

  absolute_dir_path_ = root_path.Append(relative_dir_path_);

  // We assume it's a directory if we've gotten here: either the path
  // ends with '/', or FileSystemDirURLRequestJob already statted it and
  // found it to be a directory.
  base::FileUtilProxy::ReadDirectory(file_thread_proxy_, absolute_dir_path_,
      callback_factory_.NewCallback(
          &FileSystemDirURLRequestJob::DidReadDirectory));
}

void FileSystemDirURLRequestJob::DidReadDirectory(
    base::PlatformFileError error_code,
    const std::vector<base::FileUtilProxy::Entry>& entries) {
  if (error_code != base::PLATFORM_FILE_OK) {
    NotifyFailed(error_code);
    return;
  }

#if defined(OS_WIN)
  const string16& title = relative_dir_path_.value();
#elif defined(OS_POSIX)
  const string16& title = WideToUTF16(
      base::SysNativeMBToWide(relative_dir_path_.value()));
#endif
  data_.append(net::GetDirectoryListingHeader(ASCIIToUTF16("/") + title));

  typedef std::vector<base::FileUtilProxy::Entry>::const_iterator EntryIterator;
  for (EntryIterator it = entries.begin(); it != entries.end(); ++it) {
#if defined(OS_WIN)
    const string16& name = it->name;
#elif defined(OS_POSIX)
    const string16& name =
        WideToUTF16(base::SysNativeMBToWide(it->name));
#endif
    // TODO(adamk): Add file size?
    data_.append(net::GetDirectoryListingEntry(
        name, std::string(), it->is_directory, 0, base::Time()));
  }

  set_expected_content_size(data_.size());
  NotifyHeadersComplete();
}

void FileSystemDirURLRequestJob::NotifyFailed(int rv) {
  NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
}

}  // namespace fileapi
