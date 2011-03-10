// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/blob/blob_url_request_job.h"

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/file_util_proxy.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_status.h"

namespace webkit_blob {

static const int kHTTPOk = 200;
static const int kHTTPPartialContent = 206;
static const int kHTTPNotAllowed = 403;
static const int kHTTPNotFound = 404;
static const int kHTTPMethodNotAllow = 405;
static const int kHTTPRequestedRangeNotSatisfiable = 416;
static const int kHTTPInternalError = 500;

static const char kHTTPOKText[] = "OK";
static const char kHTTPPartialContentText[] = "Partial Content";
static const char kHTTPNotAllowedText[] = "Not Allowed";
static const char kHTTPNotFoundText[] = "Not Found";
static const char kHTTPMethodNotAllowText[] = "Method Not Allowed";
static const char kHTTPRequestedRangeNotSatisfiableText[] =
    "Requested Range Not Satisfiable";
static const char kHTTPInternalErrorText[] = "Internal Server Error";

BlobURLRequestJob::BlobURLRequestJob(
    net::URLRequest* request,
    BlobData* blob_data,
    base::MessageLoopProxy* file_thread_proxy)
    : net::URLRequestJob(request),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      blob_data_(blob_data),
      file_thread_proxy_(file_thread_proxy),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          io_callback_(this, &BlobURLRequestJob::DidRead)),
      item_index_(0),
      total_size_(0),
      current_item_offset_(0),
      remaining_bytes_(0),
      read_buf_offset_(0),
      read_buf_size_(0),
      read_buf_remaining_bytes_(0),
      error_(false),
      headers_set_(false),
      byte_range_set_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
}

BlobURLRequestJob::~BlobURLRequestJob() {
}

void BlobURLRequestJob::Start() {
  // Continue asynchronously.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(&BlobURLRequestJob::DidStart));
}

void BlobURLRequestJob::DidStart() {
  // We only support GET request per the spec.
  if (request()->method() != "GET") {
    NotifyFailure(net::ERR_METHOD_NOT_SUPPORTED);
    return;
  }

  // If the blob data is not present, bail out.
  if (!blob_data_) {
    NotifyFailure(net::ERR_FILE_NOT_FOUND);
    return;
  }

  CountSize();
}

void BlobURLRequestJob::Kill() {
  stream_.Close();

  net::URLRequestJob::Kill();
  callback_factory_.RevokeAll();
  method_factory_.RevokeAll();
}

void BlobURLRequestJob::ResolveFile(const FilePath& file_path) {
  // If the file thread proxy is provided, we can use it get the file info.
  if (file_thread_proxy_) {
    base::FileUtilProxy::GetFileInfo(
          file_thread_proxy_,
          file_path,
          callback_factory_.NewCallback(&BlobURLRequestJob::DidResolve));
    return;
  }

  // Otherwise, we use current thread, i.e. IO thread, as this is the case when
  // we run the unittest or test shell.
  // TODO(jianli): Consider using the proxy of current thread.
  base::PlatformFileInfo file_info;
  bool exists = file_util::GetFileInfo(file_path, &file_info);

  // Continue asynchronously.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &BlobURLRequestJob::DidResolve,
          exists ? base::PLATFORM_FILE_OK : base::PLATFORM_FILE_ERROR_NOT_FOUND,
          file_info));
}

void BlobURLRequestJob::DidResolve(base::PlatformFileError rv,
                                   const base::PlatformFileInfo& file_info) {
  // If an error occured, bail out.
  if (rv == base::PLATFORM_FILE_ERROR_NOT_FOUND) {
    NotifyFailure(net::ERR_FILE_NOT_FOUND);
    return;
  } else if (rv != base::PLATFORM_FILE_OK) {
    NotifyFailure(net::ERR_FAILED);
    return;
  }

  // Validate the expected modification time.
  // Note that the expected modification time from WebKit is based on
  // time_t precision. So we have to convert both to time_t to compare.
  const BlobData::Item& item = blob_data_->items().at(item_index_);
  DCHECK(item.type() == BlobData::TYPE_FILE);

  if (!item.expected_modification_time().is_null() &&
      item.expected_modification_time().ToTimeT() !=
          file_info.last_modified.ToTimeT()) {
    NotifyFailure(net::ERR_FILE_NOT_FOUND);
    return;
  }

  // If item length is -1, we need to use the file size being resolved
  // in the real time.
  int64 item_length = static_cast<int64>(item.length());
  if (item_length == -1)
    item_length = file_info.size;

  // Cache the size and add it to the total size.
  item_length_list_.push_back(item_length);
  total_size_ += item_length;

  // Continue counting the size for the remaining items.
  item_index_++;
  CountSize();
}

void BlobURLRequestJob::CountSize() {
  for (; item_index_ < blob_data_->items().size(); ++item_index_) {
    const BlobData::Item& item = blob_data_->items().at(item_index_);
    int64 item_length = static_cast<int64>(item.length());

    // If there is a file item, do the resolving.
    if (item.type() == BlobData::TYPE_FILE) {
      ResolveFile(item.file_path());
      return;
    }

    // Cache the size and add it to the total size.
    item_length_list_.push_back(item_length);
    total_size_ += item_length;
  }

  // Reset item_index_ since it will be reused to read the items.
  item_index_ = 0;

  // Apply the range requirement.
  if (!byte_range_.ComputeBounds(total_size_)) {
    NotifyFailure(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    return;
  }

  remaining_bytes_ = byte_range_.last_byte_position() -
                     byte_range_.first_byte_position() + 1;
  DCHECK_GE(remaining_bytes_, 0);

  // Do the seek at the beginning of the request.
  if (byte_range_.first_byte_position())
    Seek(byte_range_.first_byte_position());

  NotifySuccess();
}

void BlobURLRequestJob::Seek(int64 offset) {
  // Skip the initial items that are not in the range.
  for (item_index_ = 0;
       item_index_ < blob_data_->items().size() &&
           offset >= item_length_list_[item_index_];
       ++item_index_) {
    offset -= item_length_list_[item_index_];
  }

  // Set the offset that need to jump to for the first item in the range.
  current_item_offset_ = offset;
}

bool BlobURLRequestJob::ReadRawData(net::IOBuffer* dest,
                                    int dest_size,
                                    int* bytes_read) {
  DCHECK_NE(dest_size, 0);
  DCHECK(bytes_read);
  DCHECK_GE(remaining_bytes_, 0);

  // Bail out immediately if we encounter an error.
  if (error_) {
    *bytes_read = 0;
    return true;
  }

  if (remaining_bytes_ < dest_size)
    dest_size = static_cast<int>(remaining_bytes_);

  // If we should copy zero bytes because |remaining_bytes_| is zero, short
  // circuit here.
  if (!dest_size) {
    *bytes_read = 0;
    return true;
  }

  // Keep track of the buffer.
  DCHECK(!read_buf_);
  read_buf_ = dest;
  read_buf_offset_ = 0;
  read_buf_size_ = dest_size;
  read_buf_remaining_bytes_ = dest_size;

  return ReadLoop(bytes_read);
}

bool BlobURLRequestJob::ReadLoop(int* bytes_read) {
  // Read until we encounter an error or could not get the data immediately.
  while (remaining_bytes_ > 0 && read_buf_remaining_bytes_ > 0) {
    if (!ReadItem())
      return false;
  }

  *bytes_read = ReadCompleted();
  return true;
}

bool BlobURLRequestJob::ReadItem() {
  // Are we done with reading all the blob data?
  if (remaining_bytes_ == 0)
    return true;

  // If we get to the last item but still expect something to read, bail out
  // since something is wrong.
  if (item_index_ >= blob_data_->items().size()) {
    NotifyFailure(net::ERR_FAILED);
    return false;
  }

  const BlobData::Item& item = blob_data_->items().at(item_index_);

  // Compute the bytes to read for current item.
  int64 current_item_remaining_bytes =
      item_length_list_[item_index_] - current_item_offset_;
  int bytes_to_read = (read_buf_remaining_bytes_ > current_item_remaining_bytes)
      ? static_cast<int>(current_item_remaining_bytes)
      : read_buf_remaining_bytes_;
  if (bytes_to_read > remaining_bytes_)
    bytes_to_read = static_cast<int>(remaining_bytes_);

  // If nothing to read for current item, advance to next item.
  if (bytes_to_read == 0) {
    AdvanceItem();
    return ReadItem();
  }

  // Do the reading.
  switch (item.type()) {
    case BlobData::TYPE_DATA:
      return ReadBytes(item, bytes_to_read);
    case BlobData::TYPE_FILE:
      return ReadFile(item, bytes_to_read);
    default:
      DCHECK(false);
      return false;
  }
}

bool BlobURLRequestJob::ReadBytes(const BlobData::Item& item,
                                  int bytes_to_read) {
  DCHECK(read_buf_remaining_bytes_ >= bytes_to_read);

  memcpy(read_buf_->data() + read_buf_offset_,
         &item.data().at(0) + item.offset() + current_item_offset_,
         bytes_to_read);

  AdvanceBytesRead(bytes_to_read);
  return true;
}

bool BlobURLRequestJob::ReadFile(const BlobData::Item& item,
                                 int bytes_to_read) {
  DCHECK(read_buf_remaining_bytes_ >= bytes_to_read);

  // Open the file if not yet.
  if (!stream_.IsOpen()) {
    // stream_.Open() and stream_.Seek() block the IO thread.
    // See http://crbug.com/75548.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    int rv = stream_.Open(item.file_path(), base::PLATFORM_FILE_OPEN |
        base::PLATFORM_FILE_READ | base::PLATFORM_FILE_ASYNC);
    if (rv != net::OK) {
      NotifyFailure(net::ERR_FAILED);
      return false;
    }

    // Seek the file if needed.
    int64 offset = current_item_offset_ + static_cast<int64>(item.offset());
    if (offset > 0) {
      if (offset != stream_.Seek(net::FROM_BEGIN, offset)) {
        NotifyFailure(net::ERR_FAILED);
        return false;
      }
    }
  }

  // Start the asynchronous reading.
  int rv = stream_.Read(read_buf_->data() + read_buf_offset_,
                        bytes_to_read,
                        &io_callback_);

  // If I/O pending error is returned, we just need to wait.
  if (rv == net::ERR_IO_PENDING) {
    SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
    return false;
  }

  // For all other errors, bail out.
  if (rv < 0) {
    NotifyFailure(net::ERR_FAILED);
    return false;
  }

  // Otherwise, data is immediately available.
  AdvanceBytesRead(rv);
  return true;
}

void BlobURLRequestJob::DidRead(int result) {
  if (result < 0) {
    NotifyFailure(net::ERR_FAILED);
    return;
  }
  SetStatus(net::URLRequestStatus());  // Clear the IO_PENDING status

  AdvanceBytesRead(result);

  // If the read buffer is completely filled, we're done.
  if (!read_buf_remaining_bytes_) {
    int bytes_read = ReadCompleted();
    NotifyReadComplete(bytes_read);
    return;
  }

  // Otherwise, continue the reading.
  int bytes_read = 0;
  if (ReadLoop(&bytes_read))
    NotifyReadComplete(bytes_read);
}

void BlobURLRequestJob::AdvanceItem() {
  // Close the stream if the current item is a file.
  if (stream_.IsOpen())
    stream_.Close();

  // Advance to the next item.
  item_index_++;
  current_item_offset_ = 0;
}

void BlobURLRequestJob::AdvanceBytesRead(int result) {
  DCHECK_GT(result, 0);

  // Do we finish reading the current item?
  current_item_offset_ += result;
  if (current_item_offset_ == item_length_list_[item_index_])
    AdvanceItem();

  // Subtract the remaining bytes.
  remaining_bytes_ -= result;
  DCHECK_GE(remaining_bytes_, 0);

  // Adjust the read buffer.
  read_buf_offset_ += result;
  read_buf_remaining_bytes_ -= result;
  DCHECK_GE(read_buf_remaining_bytes_, 0);
}

int BlobURLRequestJob::ReadCompleted() {
  int bytes_read = read_buf_size_ - read_buf_remaining_bytes_;
  read_buf_ = NULL;
  read_buf_offset_ = 0;
  read_buf_size_ = 0;
  read_buf_remaining_bytes_ = 0;
  return bytes_read;
}

void BlobURLRequestJob::HeadersCompleted(int status_code,
                                         const std::string& status_text) {
  std::string status("HTTP/1.1 ");
  status.append(base::IntToString(status_code));
  status.append(" ");
  status.append(status_text);
  status.append("\0\0", 2);
  net::HttpResponseHeaders* headers = new net::HttpResponseHeaders(status);

  if (status_code == kHTTPOk || status_code == kHTTPPartialContent) {
    std::string content_length_header(net::HttpRequestHeaders::kContentLength);
    content_length_header.append(": ");
    content_length_header.append(base::Int64ToString(remaining_bytes_));
    headers->AddHeader(content_length_header);
    if (!blob_data_->content_type().empty()) {
      std::string content_type_header(net::HttpRequestHeaders::kContentType);
      content_type_header.append(": ");
      content_type_header.append(blob_data_->content_type());
      headers->AddHeader(content_type_header);
    }
    if (!blob_data_->content_disposition().empty()) {
      std::string content_disposition_header("Content-Disposition: ");
      content_disposition_header.append(blob_data_->content_disposition());
      headers->AddHeader(content_disposition_header);
    }
  }

  response_info_.reset(new net::HttpResponseInfo());
  response_info_->headers = headers;

  set_expected_content_size(remaining_bytes_);
  NotifyHeadersComplete();

  headers_set_ = true;
}

void BlobURLRequestJob::NotifySuccess() {
  int status_code = 0;
  std::string status_text;
  if (byte_range_set_ && byte_range_.IsValid()) {
    status_code = kHTTPPartialContent;
    status_text += kHTTPPartialContentText;
  } else {
    status_code = kHTTPOk;
    status_text = kHTTPOKText;
  }
  HeadersCompleted(status_code, status_text);
}

void BlobURLRequestJob::NotifyFailure(int error_code) {
  error_ = true;

  // If we already return the headers on success, we can't change the headers
  // now. Instead, we just error out.
  if (headers_set_) {
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                     error_code));
    return;
  }

  int status_code = 0;
  std::string status_txt;
  switch (error_code) {
    case net::ERR_ACCESS_DENIED:
      status_code = kHTTPNotAllowed;
      status_txt = kHTTPNotAllowedText;
      break;
    case net::ERR_FILE_NOT_FOUND:
      status_code = kHTTPNotFound;
      status_txt = kHTTPNotFoundText;
      break;
    case net::ERR_METHOD_NOT_SUPPORTED:
      status_code = kHTTPMethodNotAllow;
      status_txt = kHTTPMethodNotAllowText;
      break;
    case net::ERR_REQUEST_RANGE_NOT_SATISFIABLE:
      status_code = kHTTPRequestedRangeNotSatisfiable;
      status_txt = kHTTPRequestedRangeNotSatisfiableText;
      break;
    case net::ERR_FAILED:
      status_code = kHTTPInternalError;
      status_txt = kHTTPInternalErrorText;
      break;
    default:
      DCHECK(false);
      status_code = kHTTPInternalError;
      status_txt = kHTTPInternalErrorText;
      break;
  }
  HeadersCompleted(status_code, status_txt);
}

bool BlobURLRequestJob::GetMimeType(std::string* mime_type) const {
  if (!response_info_.get())
    return false;

  return response_info_->headers->GetMimeType(mime_type);
}

void BlobURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (response_info_.get())
    *info = *response_info_;
}

int BlobURLRequestJob::GetResponseCode() const {
  if (!response_info_.get())
    return -1;

  return response_info_->headers->response_code();
}

void BlobURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  std::string range_header;
  if (headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header)) {
    // We only care about "Range" header here.
    std::vector<net::HttpByteRange> ranges;
    if (net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
      if (ranges.size() == 1) {
        byte_range_set_ = true;
        byte_range_ = ranges[0];
      } else {
        // We don't support multiple range requests in one single URL request,
        // because we need to do multipart encoding here.
        // TODO(jianli): Support multipart byte range requests.
        NotifyFailure(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
      }
    }
  }
}

}  // namespace webkit_blob
