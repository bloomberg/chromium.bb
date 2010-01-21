// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "webkit/appcache/appcache_url_request_job.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_status.h"

namespace appcache {

AppCacheURLRequestJob::AppCacheURLRequestJob(
    URLRequest* request, AppCacheStorage* storage)
    : URLRequestJob(request), storage_(storage),
      has_been_started_(false), has_been_killed_(false),
      delivery_type_(AWAITING_DELIVERY_ORDERS),
      cache_id_(kNoCacheId),
      ALLOW_THIS_IN_INITIALIZER_LIST(read_callback_(
          this, &AppCacheURLRequestJob::OnReadComplete)) {
  DCHECK(storage_);
}

AppCacheURLRequestJob::~AppCacheURLRequestJob() {
  if (storage_)
    storage_->CancelDelegateCallbacks(this);
}

void AppCacheURLRequestJob::DeliverAppCachedResponse(
    const GURL& manifest_url, int64 cache_id, const AppCacheEntry& entry) {
  DCHECK(!has_delivery_orders());
  DCHECK(entry.has_response_id());
  delivery_type_ = APPCACHED_DELIVERY;
  manifest_url_ = manifest_url;
  cache_id_ = cache_id;
  entry_ = entry;
  MaybeBeginDelivery();
}

void AppCacheURLRequestJob::DeliverNetworkResponse() {
  DCHECK(!has_delivery_orders());
  delivery_type_ = NETWORK_DELIVERY;
  storage_ = NULL;  // not needed
  MaybeBeginDelivery();
}

void AppCacheURLRequestJob::DeliverErrorResponse() {
  DCHECK(!has_delivery_orders());
  delivery_type_ = ERROR_DELIVERY;
  storage_ = NULL;  // not needed
  MaybeBeginDelivery();
}

void AppCacheURLRequestJob::MaybeBeginDelivery() {
  if (has_been_started() && has_delivery_orders()) {
    // Start asynchronously so that all error reporting and data
    // callbacks happen as they would for network requests.
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &AppCacheURLRequestJob::BeginDelivery));
  }
}

void AppCacheURLRequestJob::BeginDelivery() {
  DCHECK(has_delivery_orders() && has_been_started());

  if (has_been_killed())
    return;

  switch (delivery_type_) {
    case NETWORK_DELIVERY:
      // To fallthru to the network, we restart the request which will
      // cause a new job to be created to retrieve the resource from the
      // network. Our caller is responsible for arranging to not re-intercept
      // the same request.
      NotifyRestartRequired();
      break;

    case ERROR_DELIVERY:
      NotifyStartError(
          URLRequestStatus(URLRequestStatus::FAILED, net::ERR_FAILED));
      break;

    case APPCACHED_DELIVERY:
      storage_->LoadResponseInfo(manifest_url_, entry_.response_id(), this);
      break;

    default:
      NOTREACHED();
      break;
  }
}

void AppCacheURLRequestJob::OnResponseInfoLoaded(
      AppCacheResponseInfo* response_info, int64 response_id) {
  DCHECK(is_delivering_appcache_response());
  scoped_refptr<AppCacheURLRequestJob> protect(this);
  if (response_info) {
    info_ = response_info;
    reader_.reset(
        storage_->CreateResponseReader(manifest_url_, entry_.response_id()));

    if (is_range_request())
      SetupRangeResponse();

    NotifyHeadersComplete();
  } else {
    NotifyStartError(
        URLRequestStatus(URLRequestStatus::FAILED, net::ERR_FAILED));
  }
  storage_ = NULL;  // no longer needed
}

const net::HttpResponseInfo* AppCacheURLRequestJob::http_info() const {
  if (!info_.get())
    return NULL;
  if (range_response_info_.get())
    return range_response_info_.get();
  return info_->http_response_info();
}

void AppCacheURLRequestJob::SetupRangeResponse() {
  DCHECK(is_range_request() && reader_.get() &&
         is_delivering_appcache_response());
  int resource_size = reader_->GetResourceSize();
  if (resource_size < 0 || !range_requested_.ComputeBounds(resource_size)) {
    range_requested_ = net::HttpByteRange();
    return;
  }

  DCHECK(range_requested_.HasFirstBytePosition() &&
         range_requested_.HasLastBytePosition());
  int offset = static_cast<int>(range_requested_.first_byte_position());
  int length = static_cast<int>(range_requested_.last_byte_position() -
                                range_requested_.first_byte_position() + 1);

  // Tell the reader about the range to read.
  reader_->SetReadRange(offset, length);

  // Make a copy of the full response headers and fix them up
  // for the range we'll be returning.
  const char kLengthHeader[] = "Content-Length";
  const char kRangeHeader[] = "Content-Range";
  const char kPartialStatusLine[] = "HTTP/1.1 206 Partial Content";
  range_response_info_.reset(
      new net::HttpResponseInfo(*info_->http_response_info()));
  net::HttpResponseHeaders* headers = range_response_info_->headers;
  headers->RemoveHeader(kLengthHeader);
  headers->RemoveHeader(kRangeHeader);
  headers->ReplaceStatusLine(kPartialStatusLine);
  headers->AddHeader(
      StringPrintf("%s: %d", kLengthHeader, length));
  headers->AddHeader(
      StringPrintf("%s: bytes %d-%d/%d",
                   kRangeHeader,
                   offset,
                   offset + length - 1,
                   resource_size));
}

void AppCacheURLRequestJob::OnReadComplete(int result) {
  DCHECK(is_delivering_appcache_response());
  if (result == 0)
    NotifyDone(URLRequestStatus());
  else if (result < 0)
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, result));
  else
    SetStatus(URLRequestStatus());  // Clear the IO_PENDING status

  NotifyReadComplete(result);
}

// URLRequestJob overrides ------------------------------------------------

void AppCacheURLRequestJob::Start() {
  DCHECK(!has_been_started());
  has_been_started_ = true;
  MaybeBeginDelivery();
}

void AppCacheURLRequestJob::Kill() {
  if (!has_been_killed_) {
    has_been_killed_ = true;
    reader_.reset();
    if (storage_) {
      storage_->CancelDelegateCallbacks(this);
      storage_ = NULL;
    }
    URLRequestJob::Kill();
  }
}

net::LoadState AppCacheURLRequestJob::GetLoadState() const {
  if (!has_been_started())
    return net::LOAD_STATE_IDLE;
  if (!has_delivery_orders())
    return net::LOAD_STATE_WAITING_FOR_CACHE;
  if (delivery_type_ != APPCACHED_DELIVERY)
    return net::LOAD_STATE_IDLE;
  if (!info_.get())
    return net::LOAD_STATE_WAITING_FOR_CACHE;
  if (reader_.get() && reader_->IsReadPending())
    return net::LOAD_STATE_READING_RESPONSE;
  return net::LOAD_STATE_IDLE;
}

bool AppCacheURLRequestJob::GetMimeType(std::string* mime_type) const {
  if (!http_info())
    return false;
  return http_info()->headers->GetMimeType(mime_type);
}

bool AppCacheURLRequestJob::GetCharset(std::string* charset) {
  if (!http_info())
    return false;
  return http_info()->headers->GetCharset(charset);
}

void AppCacheURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (!http_info())
    return;
  *info = *http_info();
}

int AppCacheURLRequestJob::GetResponseCode() const {
  if (!http_info())
    return -1;
  return http_info()->headers->response_code();
}

bool AppCacheURLRequestJob::ReadRawData(net::IOBuffer* buf, int buf_size,
                                        int *bytes_read) {
  DCHECK(is_delivering_appcache_response());
  DCHECK_NE(buf_size, 0);
  DCHECK(bytes_read);
  DCHECK(!reader_->IsReadPending());
  reader_->ReadData(buf, buf_size, &read_callback_);
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  return false;
}

void AppCacheURLRequestJob::SetExtraRequestHeaders(
    const std::string& headers) {
  // If multiple ranges are requested, we play dumb and
  // return the entire response with 200 OK.
  std::vector<net::HttpByteRange> ranges;
  if (!net::HttpUtil::ParseRanges(headers, &ranges) || (ranges.size() > 1U))
    return;
  range_requested_ = ranges[0];
}

}  // namespace appcache

