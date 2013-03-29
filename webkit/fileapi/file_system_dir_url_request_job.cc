// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_dir_url_request_job.h"

#include <algorithm>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_util_proxy.h"
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
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_url.h"

using net::NetworkDelegate;
using net::URLRequest;
using net::URLRequestJob;
using net::URLRequestStatus;

namespace fileapi {

FileSystemDirURLRequestJob::FileSystemDirURLRequestJob(
    URLRequest* request,
    NetworkDelegate* network_delegate,
    FileSystemContext* file_system_context)
    : URLRequestJob(request, network_delegate),
      file_system_context_(file_system_context),
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
  if (!request_)
    return;
  url_ = file_system_context_->CrackURL(request_->url());
  base::PlatformFileError error_code;
  FileSystemOperation* operation = GetNewOperation(&error_code);
  if (error_code != base::PLATFORM_FILE_OK) {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
                                net::PlatformFileErrorToNetError(error_code)));
    return;
  }
  operation->ReadDirectory(
      url_,
      base::Bind(&FileSystemDirURLRequestJob::DidReadDirectory, this));
}

void FileSystemDirURLRequestJob::DidReadDirectory(
    base::PlatformFileError result,
    const std::vector<base::FileUtilProxy::Entry>& entries,
    bool has_more) {
  if (result != base::PLATFORM_FILE_OK) {
    int rv = net::ERR_FILE_NOT_FOUND;
    if (result == base::PLATFORM_FILE_ERROR_INVALID_URL)
      rv = net::ERR_INVALID_URL;
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
    return;
  }

  if (!request_)
    return;

  if (data_.empty()) {
    base::FilePath relative_path = url_.path();
#if defined(OS_POSIX)
    relative_path = base::FilePath(FILE_PATH_LITERAL("/") + relative_path.value());
#endif
    const base::string16& title = relative_path.LossyDisplayName();
    data_.append(net::GetDirectoryListingHeader(title));
  }

  typedef std::vector<base::FileUtilProxy::Entry>::const_iterator EntryIterator;
  for (EntryIterator it = entries.begin(); it != entries.end(); ++it) {
    const base::string16& name = base::FilePath(it->name).LossyDisplayName();
    data_.append(net::GetDirectoryListingEntry(
        name, std::string(), it->is_directory, it->size,
        it->last_modified_time));
  }

  if (has_more) {
    base::PlatformFileError error_code;
    FileSystemOperation* operation = GetNewOperation(&error_code);
    if (error_code != base::PLATFORM_FILE_OK) {
      NotifyDone(URLRequestStatus(
          URLRequestStatus::FAILED,
          net::PlatformFileErrorToNetError(error_code)));
      return;
    }

    operation->ReadDirectory(
        url_,
        base::Bind(&FileSystemDirURLRequestJob::DidReadDirectory, this));
  } else {
    set_expected_content_size(data_.size());
    NotifyHeadersComplete();
  }
}

FileSystemOperation* FileSystemDirURLRequestJob::GetNewOperation(
    base::PlatformFileError* error_code) {
  return file_system_context_->CreateFileSystemOperation(url_, error_code);
}

}  // namespace fileapi
