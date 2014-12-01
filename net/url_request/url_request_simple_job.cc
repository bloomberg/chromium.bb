// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_simple_job.h"

#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/profiler/scoped_tracker.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_status.h"

namespace net {

URLRequestSimpleJob::URLRequestSimpleJob(
    URLRequest* request, NetworkDelegate* network_delegate)
    : URLRangeRequestJob(request, network_delegate),
      data_offset_(0),
      weak_factory_(this) {}

void URLRequestSimpleJob::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestSimpleJob::StartAsync, weak_factory_.GetWeakPtr()));
}

bool URLRequestSimpleJob::GetMimeType(std::string* mime_type) const {
  *mime_type = mime_type_;
  return true;
}

bool URLRequestSimpleJob::GetCharset(std::string* charset) {
  *charset = charset_;
  return true;
}

URLRequestSimpleJob::~URLRequestSimpleJob() {}

bool URLRequestSimpleJob::ReadRawData(IOBuffer* buf, int buf_size,
                                      int* bytes_read) {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/422489 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422489 URLRequestSimpleJob::ReadRawData"));

  DCHECK(bytes_read);
  buf_size = static_cast<int>(std::min(
      static_cast<int64>(buf_size),
      byte_range_.last_byte_position() - data_offset_ + 1));
  memcpy(buf->data(), data_->front() + data_offset_, buf_size);
  data_offset_ += buf_size;
  *bytes_read = buf_size;
  return true;
}

int URLRequestSimpleJob::GetData(std::string* mime_type,
                                 std::string* charset,
                                 std::string* data,
                                 const CompletionCallback& callback) const {
  NOTREACHED();
  return ERR_UNEXPECTED;
}

int URLRequestSimpleJob::GetRefCountedData(
    std::string* mime_type,
    std::string* charset,
    scoped_refptr<base::RefCountedMemory>* data,
    const CompletionCallback& callback) const {
  scoped_refptr<base::RefCountedString> str_data(new base::RefCountedString());
  int result = GetData(mime_type, charset, &str_data->data(), callback);
  *data = str_data;
  return result;
}

void URLRequestSimpleJob::StartAsync() {
  if (!request_)
    return;

  if (ranges().size() > 1) {
    // TODO(vadimt): Remove ScopedTracker below once crbug.com/422489 is fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "422489 URLRequestSimpleJob::StartAsync 1"));

    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
                                ERR_REQUEST_RANGE_NOT_SATISFIABLE));
    return;
  }

  if (!ranges().empty() && range_parse_result() == OK)
    byte_range_ = ranges().front();

  int result;
  {
    // TODO(vadimt): Remove ScopedTracker below once crbug.com/422489 is fixed.
    // Remove the block and assign 'result' in its declaration.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "422489 URLRequestSimpleJob::StartAsync 2"));

    result =
        GetRefCountedData(&mime_type_, &charset_, &data_,
                          base::Bind(&URLRequestSimpleJob::OnGetDataCompleted,
                                     weak_factory_.GetWeakPtr()));
  }

  if (result != ERR_IO_PENDING) {
    // TODO(vadimt): Remove ScopedTracker below once crbug.com/422489 is fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "422489 URLRequestSimpleJob::StartAsync 3"));

    OnGetDataCompleted(result);
  }
}

void URLRequestSimpleJob::OnGetDataCompleted(int result) {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/422489 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422489 URLRequestSimpleJob::OnGetDataCompleted"));

  if (result == OK) {
    // Notify that the headers are complete
    if (!byte_range_.ComputeBounds(data_->size())) {
      NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
                 ERR_REQUEST_RANGE_NOT_SATISFIABLE));
      return;
    }

    data_offset_ = byte_range_.first_byte_position();
    set_expected_content_size(
        byte_range_.last_byte_position() - data_offset_ + 1);
    NotifyHeadersComplete();
  } else {
    NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, result));
  }
}

}  // namespace net
