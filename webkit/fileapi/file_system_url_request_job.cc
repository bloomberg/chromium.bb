// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_url_request_job.h"

#include "base/compiler_specific.h"
#include "base/file_util_proxy.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "googleurl/src/gurl.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_util.h"

using net::URLRequest;
using net::URLRequestJob;
using net::URLRequestStatus;

namespace fileapi {

static const int kFileFlags = base::PLATFORM_FILE_OPEN |
                              base::PLATFORM_FILE_READ |
                              base::PLATFORM_FILE_ASYNC;

static net::HttpResponseHeaders* CreateHttpResponseHeaders() {
  // HttpResponseHeaders expects its input string to be terminated by two NULs.
  static const char kStatus[] = "HTTP/1.1 200 OK\0";
  static const size_t kStatusLen = arraysize(kStatus);

  net::HttpResponseHeaders* headers =
      new net::HttpResponseHeaders(std::string(kStatus, kStatusLen));

  // Tell WebKit never to cache this content.
  std::string cache_control(net::HttpRequestHeaders::kCacheControl);
  cache_control.append(": no-cache");
  headers->AddHeader(cache_control);

  return headers;
}

class FileSystemURLRequestJob::CallbackDispatcher
    : public FileSystemCallbackDispatcher {
 public:
  explicit CallbackDispatcher(FileSystemURLRequestJob* job)
      : job_(job) {
    DCHECK(job_);
  }

  // fileapi::FileSystemCallbackDispatcher overrides.
  virtual void DidSucceed() OVERRIDE {
    NOTREACHED();
  }

  virtual void DidReadMetadata(const base::PlatformFileInfo& file_info,
                               const FilePath& platform_path) OVERRIDE {
    job_->DidGetMetadata(file_info, platform_path);
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
    job_->NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
  }

 private:
  // TODO(adamk): Get rid of the need for refcounting here by
  // allowing FileSystemOperations to be cancelled.
  scoped_refptr<FileSystemURLRequestJob> job_;
  DISALLOW_COPY_AND_ASSIGN(CallbackDispatcher);
};

FileSystemURLRequestJob::FileSystemURLRequestJob(
    URLRequest* request, FileSystemContext* file_system_context,
    scoped_refptr<base::MessageLoopProxy> file_thread_proxy)
    : URLRequestJob(request),
      file_system_context_(file_system_context),
      file_thread_proxy_(file_thread_proxy),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(callback_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          io_callback_(this, &FileSystemURLRequestJob::DidRead)),
      stream_(NULL),
      is_directory_(false),
      remaining_bytes_(0) {
}

FileSystemURLRequestJob::~FileSystemURLRequestJob() {
  // Since we use the two-arg constructor of FileStream, we need to call Close()
  // manually: ~FileStream won't call it for us.
  if (stream_ != NULL)
    stream_->Close();
}

void FileSystemURLRequestJob::Start() {
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(
          &FileSystemURLRequestJob::StartAsync));
}

void FileSystemURLRequestJob::Kill() {
  if (stream_ != NULL) {
    stream_->Close();
    stream_.reset(NULL);
  }
  URLRequestJob::Kill();
  method_factory_.RevokeAll();
  callback_factory_.RevokeAll();
}

bool FileSystemURLRequestJob::ReadRawData(net::IOBuffer* dest, int dest_size,
                                          int *bytes_read) {
  DCHECK_NE(dest_size, 0);
  DCHECK(bytes_read);
  DCHECK_GE(remaining_bytes_, 0);

  if (stream_ == NULL)
    return false;

  if (remaining_bytes_ < dest_size)
    dest_size = static_cast<int>(remaining_bytes_);

  if (!dest_size) {
    *bytes_read = 0;
    return true;
  }

  int rv = stream_->Read(dest->data(), dest_size, &io_callback_);
  if (rv >= 0) {
    // Data is immediately available.
    *bytes_read = rv;
    remaining_bytes_ -= rv;
    DCHECK_GE(remaining_bytes_, 0);
    return true;
  }

  // Otherwise, a read error occured.  We may just need to wait...
  if (rv == net::ERR_IO_PENDING)
    SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  else
    NotifyFailed(rv);
  return false;
}

bool FileSystemURLRequestJob::GetMimeType(std::string* mime_type) const {
  // URL requests should not block on the disk!  On Windows this goes to the
  // registry.
  //   http://code.google.com/p/chromium/issues/detail?id=59849
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  DCHECK(request_);
  return net::GetMimeTypeFromFile(absolute_file_path_, mime_type);
}

void FileSystemURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  std::string range_header;
  if (headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header)) {
    std::vector<net::HttpByteRange> ranges;
    if (net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
      if (ranges.size() == 1)
        byte_range_ = ranges[0];
      else {
        // We don't support multiple range requests in one single URL request.
        // TODO(adamk): decide whether we want to support multiple range
        // requests.
        NotifyFailed(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
      }
    }
  }
}

void FileSystemURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (response_info_.get())
    *info = *response_info_;
}

int FileSystemURLRequestJob::GetResponseCode() const {
  if (response_info_.get())
    return 200;
  return URLRequestJob::GetResponseCode();
}

void FileSystemURLRequestJob::StartAsync() {
  if (request_) {
    (new FileSystemOperation(new CallbackDispatcher(this),
                             file_thread_proxy_,
                             file_system_context_,
                             NULL))->GetMetadata(request_->url());
  }
}

void FileSystemURLRequestJob::DidGetMetadata(
    const base::PlatformFileInfo& file_info,
    const FilePath& platform_path) {
  // We may have been orphaned...
  if (!request_)
    return;

  absolute_file_path_ = platform_path;
  is_directory_ = file_info.is_directory;

  if (!byte_range_.ComputeBounds(file_info.size)) {
    NotifyFailed(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    return;
  }

  if (!is_directory_) {
    base::FileUtilProxy::CreateOrOpen(
        file_thread_proxy_, absolute_file_path_, kFileFlags,
        callback_factory_.NewCallback(&FileSystemURLRequestJob::DidOpen));
  } else
    NotifyHeadersComplete();
}

void FileSystemURLRequestJob::DidOpen(base::PlatformFileError error_code,
                                      base::PassPlatformFile file,
                                      bool created) {
  if (error_code != base::PLATFORM_FILE_OK) {
    NotifyFailed(error_code);
    return;
  }

  stream_.reset(new net::FileStream(file.ReleaseValue(), kFileFlags));

  remaining_bytes_ = byte_range_.last_byte_position() -
                     byte_range_.first_byte_position() + 1;
  DCHECK_GE(remaining_bytes_, 0);

  // TODO(adamk): Please remove this ScopedAllowIO once we support async seek on
  // FileStream.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  // Do the seek at the beginning of the request.
  if (remaining_bytes_ > 0 &&
      byte_range_.first_byte_position() != 0 &&
      byte_range_.first_byte_position() !=
          stream_->Seek(net::FROM_BEGIN, byte_range_.first_byte_position())) {
    NotifyFailed(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    return;
  }

  set_expected_content_size(remaining_bytes_);
  response_info_.reset(new net::HttpResponseInfo());
  response_info_->headers = CreateHttpResponseHeaders();

  NotifyHeadersComplete();
}

void FileSystemURLRequestJob::DidRead(int result) {
  if (result > 0)
    SetStatus(URLRequestStatus());  // Clear the IO_PENDING status
  else if (result == 0)
    NotifyDone(URLRequestStatus());
  else
    NotifyFailed(result);

  remaining_bytes_ -= result;
  DCHECK_GE(remaining_bytes_, 0);

  NotifyReadComplete(result);
}

bool FileSystemURLRequestJob::IsRedirectResponse(GURL* location,
                                                 int* http_status_code) {
  if (is_directory_) {
    // This happens when we discovered the file is a directory, so needs a
    // slash at the end of the path.
    std::string new_path = request_->url().path();
    new_path.push_back('/');
    GURL::Replacements replacements;
    replacements.SetPathStr(new_path);
    *location = request_->url().ReplaceComponents(replacements);
    *http_status_code = 301;  // simulate a permanent redirect
    return true;
  }

  return false;
}

void FileSystemURLRequestJob::NotifyFailed(int rv) {
  NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
}

}  // namespace fileapi
