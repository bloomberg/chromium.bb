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
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_util.h"

using net::URLRequest;
using net::URLRequestJob;
using net::URLRequestStatus;

namespace fileapi {

FileSystemDirURLRequestJob::FileSystemDirURLRequestJob(
    URLRequest* request, FileSystemContext* file_system_context,
    scoped_refptr<base::MessageLoopProxy> file_thread_proxy)
    : FileSystemURLRequestJobBase(request, file_system_context,
                                  file_thread_proxy),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(callback_factory_(this)) {
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
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(
          &FileSystemURLRequestJobBase::StartAsync));
}

void FileSystemDirURLRequestJob::Kill() {
  URLRequestJob::Kill();
  callback_factory_.RevokeAll();
}

void FileSystemDirURLRequestJob::DidGetLocalPath(
    const FilePath& local_path) {
  absolute_file_path_ = local_path;
  base::FileUtilProxy::ReadDirectory(file_thread_proxy_, absolute_file_path_,
      callback_factory_.NewCallback(
          &FileSystemDirURLRequestJob::DidReadDirectory));
}

bool FileSystemDirURLRequestJob::GetMimeType(std::string* mime_type) const {
  *mime_type = "text/html";
  return true;
}

bool FileSystemDirURLRequestJob::GetCharset(std::string* charset) {
  *charset = "utf-8";
  return true;
}

void FileSystemDirURLRequestJob::DidReadDirectory(
    base::PlatformFileError error_code,
    const std::vector<base::FileUtilProxy::Entry>& entries) {
  if (error_code != base::PLATFORM_FILE_OK) {
    NotifyFailed(error_code);
    return;
  }

#if defined(OS_WIN)
  const string16& title = relative_file_path_.value();
#elif defined(OS_POSIX)
  const string16& title = WideToUTF16(
      base::SysNativeMBToWide(relative_file_path_.value()));
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

}  // namespace fileapi
