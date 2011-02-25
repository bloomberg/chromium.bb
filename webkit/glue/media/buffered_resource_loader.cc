// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/media/buffered_resource_loader.h"

#include "base/format_macros.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKitClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLError.h"
#include "webkit/glue/multipart_response_delegate.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFrame;
using WebKit::WebString;
using WebKit::WebURLError;
using WebKit::WebURLLoader;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;
using webkit_glue::MultipartResponseDelegate;

namespace {

const int kHttpOK = 200;
const int kHttpPartialContent = 206;

// Define the number of bytes in a megabyte.
const size_t kMegabyte = 1024 * 1024;

// Backward capacity of the buffer, by default 2MB.
const size_t kBackwardCapcity = 2 * kMegabyte;

// Forward capacity of the buffer, by default 10MB.
const size_t kForwardCapacity = 10 * kMegabyte;

// The threshold of bytes that we should wait until the data arrives in the
// future instead of restarting a new connection. This number is defined in the
// number of bytes, we should determine this value from typical connection speed
// and amount of time for a suitable wait. Now I just make a guess for this
// number to be 2MB.
// TODO(hclam): determine a better value for this.
const int kForwardWaitThreshold = 2 * kMegabyte;

} // namespace

namespace webkit_glue {

BufferedResourceLoader::BufferedResourceLoader(
    const GURL& url,
    int64 first_byte_position,
    int64 last_byte_position)
    : buffer_(new media::SeekableBuffer(kBackwardCapcity, kForwardCapacity)),
      deferred_(false),
      defer_allowed_(true),
      completed_(false),
      range_requested_(false),
      partial_response_(false),
      url_(url),
      first_byte_position_(first_byte_position),
      last_byte_position_(last_byte_position),
      single_origin_(true),
      start_callback_(NULL),
      offset_(0),
      content_length_(kPositionNotSpecified),
      instance_size_(kPositionNotSpecified),
      read_callback_(NULL),
      read_position_(0),
      read_size_(0),
      read_buffer_(NULL),
      first_offset_(0),
      last_offset_(0),
      keep_test_loader_(false) {
}

BufferedResourceLoader::~BufferedResourceLoader() {
  if (!completed_ && url_loader_.get())
    url_loader_->cancel();
}

void BufferedResourceLoader::Start(net::CompletionCallback* start_callback,
                                   NetworkEventCallback* event_callback,
                                   WebFrame* frame) {
  // Make sure we have not started.
  DCHECK(!start_callback_.get());
  DCHECK(!event_callback_.get());
  DCHECK(start_callback);
  DCHECK(event_callback);
  CHECK(frame);

  start_callback_.reset(start_callback);
  event_callback_.reset(event_callback);

  if (first_byte_position_ != kPositionNotSpecified) {
    range_requested_ = true;
    // TODO(hclam): server may not support range request so |offset_| may not
    // equal to |first_byte_position_|.
    offset_ = first_byte_position_;
  }

  // Increment the reference count right before we start the request. This
  // reference will be release when this request has ended.
  AddRef();

  // Prepare the request.
  WebURLRequest request(url_);
  request.setTargetType(WebURLRequest::TargetIsMedia);
  request.setHTTPHeaderField(WebString::fromUTF8("Range"),
                             WebString::fromUTF8(GenerateHeaders(
                                                 first_byte_position_,
                                                 last_byte_position_)));
  frame->setReferrerForRequest(request, WebKit::WebURL());

  // This flag is for unittests as we don't want to reset |url_loader|
  if (!keep_test_loader_)
    url_loader_.reset(frame->createAssociatedURLLoader());

  // Start the resource loading.
  url_loader_->loadAsynchronously(request, this);
}

void BufferedResourceLoader::Stop() {
  // Reset callbacks.
  start_callback_.reset();
  event_callback_.reset();
  read_callback_.reset();

  // Use the internal buffer to signal that we have been stopped.
  // TODO(hclam): Not so pretty to do this.
  if (!buffer_.get())
    return;

  // Destroy internal buffer.
  buffer_.reset();

  if (url_loader_.get()) {
    if (deferred_)
      url_loader_->setDefersLoading(false);
    deferred_ = false;

    if (!completed_) {
      url_loader_->cancel();
      completed_ = true;
    }
  }
}

void BufferedResourceLoader::Read(int64 position,
                                  int read_size,
                                  uint8* buffer,
                                  net::CompletionCallback* read_callback) {
  DCHECK(!read_callback_.get());
  DCHECK(buffer_.get());
  DCHECK(read_callback);
  DCHECK(buffer);

  // Save the parameter of reading.
  read_callback_.reset(read_callback);
  read_position_ = position;
  read_size_ = read_size;
  read_buffer_ = buffer;

  // If read position is beyond the instance size, we cannot read there.
  if (instance_size_ != kPositionNotSpecified &&
      instance_size_ <= read_position_) {
    DoneRead(0);
    return;
  }

  // Make sure |offset_| and |read_position_| does not differ by a large
  // amount.
  if (read_position_ > offset_ + kint32max ||
      read_position_ < offset_ + kint32min) {
    DoneRead(net::ERR_CACHE_MISS);
    return;
  }

  // Prepare the parameters.
  first_offset_ = static_cast<int>(read_position_ - offset_);
  last_offset_ = first_offset_ + read_size_;

  // If we can serve the request now, do the actual read.
  if (CanFulfillRead()) {
    ReadInternal();
    DisableDeferIfNeeded();
    return;
  }

  // If we expected the read request to be fulfilled later, returns
  // immediately and let more data to flow in.
  if (WillFulfillRead())
    return;

  // Make a callback to report failure.
  DoneRead(net::ERR_CACHE_MISS);
}

int64 BufferedResourceLoader::GetBufferedPosition() {
  if (buffer_.get())
    return offset_ + static_cast<int>(buffer_->forward_bytes()) - 1;
  return kPositionNotSpecified;
}

void BufferedResourceLoader::SetAllowDefer(bool is_allowed) {
  defer_allowed_ = is_allowed;
  DisableDeferIfNeeded();
}

int64 BufferedResourceLoader::content_length() {
  return content_length_;
}

int64 BufferedResourceLoader::instance_size() {
  return instance_size_;
}

bool BufferedResourceLoader::partial_response() {
  return partial_response_;
}

bool BufferedResourceLoader::network_activity() {
  return !completed_ && !deferred_;
}

const GURL& BufferedResourceLoader::url() {
  return url_;
}

void BufferedResourceLoader::SetURLLoaderForTest(WebURLLoader* mock_loader) {
  url_loader_.reset(mock_loader);
  keep_test_loader_ = true;
}

/////////////////////////////////////////////////////////////////////////////
// WebKit::WebURLLoaderClient implementation.
void BufferedResourceLoader::willSendRequest(
    WebURLLoader* loader,
    WebURLRequest& newRequest,
    const WebURLResponse& redirectResponse) {

  // The load may have been stopped and |start_callback| is destroyed.
  // In this case we shouldn't do anything.
  if (!start_callback_.get()) {
    // Set the url in the request to an invalid value (empty url).
    newRequest.setURL(WebKit::WebURL());
    return;
  }

    // Only allow |single_origin_| if we haven't seen a different origin yet.
  if (single_origin_)
    single_origin_ = url_.GetOrigin() == GURL(newRequest.url()).GetOrigin();

  // Enforce same-origin policy and cause redirects to other origins to
  // look like network errors.
  // http://dev.w3.org/html5/spec/Overview.html#concept-media-load-resource
  // http://dev.w3.org/html5/spec/Overview.html#fetch
  if (!single_origin_ || !IsProtocolSupportedForMedia(newRequest.url())) {
    // Set the url in the request to an invalid value (empty url).
    newRequest.setURL(WebKit::WebURL());
    DoneStart(net::ERR_ADDRESS_INVALID);
    return;
  }

  url_ = newRequest.url();
}

void BufferedResourceLoader::didSendData(
    WebURLLoader* loader,
    unsigned long long bytes_sent,
    unsigned long long total_bytes_to_be_sent) {
  NOTIMPLEMENTED();
}

void BufferedResourceLoader::didReceiveResponse(
    WebURLLoader* loader,
    const WebURLResponse& response) {

  // The loader may have been stopped and |start_callback| is destroyed.
  // In this case we shouldn't do anything.
  if (!start_callback_.get())
    return;

  // We make a strong assumption that when we reach here we have either
  // received a response from HTTP/HTTPS protocol or the request was
  // successful (in particular range request). So we only verify the partial
  // response for HTTP and HTTPS protocol.
  if (url_.SchemeIs(kHttpScheme) || url_.SchemeIs(kHttpsScheme)) {
    int error = net::OK;

    if (response.httpStatusCode() == kHttpPartialContent)
      partial_response_ = true;

    if (range_requested_ && partial_response_) {
      // If we have verified the partial response and it is correct, we will
      // return net::OK.
      if (!VerifyPartialResponse(response))
        error = net::ERR_INVALID_RESPONSE;
    } else if (response.httpStatusCode() != kHttpOK) {
      // We didn't request a range but server didn't reply with "200 OK".
      error = net::ERR_FAILED;
    }

    if (error != net::OK) {
      DoneStart(error);
      Stop();
      return;
    }
  } else {
    // For any protocol other than HTTP and HTTPS, assume range request is
    // always fulfilled.
    partial_response_ = range_requested_;
  }

  // Expected content length can be |kPositionNotSpecified|, in that case
  // |content_length_| is not specified and this is a streaming response.
  content_length_ = response.expectedContentLength();

  // If we have not requested a range, then the size of the instance is equal
  // to the content length.
  if (!partial_response_)
    instance_size_ = content_length_;

  // Calls with a successful response.
  DoneStart(net::OK);
}

void BufferedResourceLoader::didReceiveData(
    WebURLLoader* loader,
    const char* data,
    int data_length) {
  DCHECK(!completed_);
  DCHECK_GT(data_length, 0);

  // If this loader has been stopped, |buffer_| would be destroyed.
  // In this case we shouldn't do anything.
  if (!buffer_.get())
    return;

  // Writes more data to |buffer_|.
  buffer_->Append(reinterpret_cast<const uint8*>(data), data_length);

  // If there is an active read request, try to fulfill the request.
  if (HasPendingRead() && CanFulfillRead()) {
    ReadInternal();
  } else if (!defer_allowed_) {
    // If we're not allowed to defer, slide the buffer window forward instead
    // of deferring.
    if (buffer_->forward_bytes() > buffer_->forward_capacity()) {
      size_t excess = buffer_->forward_bytes() - buffer_->forward_capacity();
      bool success = buffer_->Seek(excess);
      DCHECK(success);
      offset_ += first_offset_ + excess;
    }
  }

  // At last see if the buffer is full and we need to defer the downloading.
  EnableDeferIfNeeded();

  // Notify that we have received some data.
  NotifyNetworkEvent();
}

void BufferedResourceLoader::didDownloadData(
    WebKit::WebURLLoader* loader,
    int dataLength) {
  NOTIMPLEMENTED();
}

void BufferedResourceLoader::didReceiveCachedMetadata(
    WebURLLoader* loader,
    const char* data,
    int data_length) {
  NOTIMPLEMENTED();
}

void BufferedResourceLoader::didFinishLoading(
    WebURLLoader* loader,
    double finishTime) {
  DCHECK(!completed_);
  completed_ = true;

  // If there is a start callback, calls it.
  if (start_callback_.get()) {
    DoneStart(net::OK);
  }

  // If there is a pending read but the request has ended, returns with what
  // we have.
  if (HasPendingRead()) {
    // Make sure we have a valid buffer before we satisfy a read request.
    DCHECK(buffer_.get());

    // Try to fulfill with what is in the buffer.
    if (CanFulfillRead())
      ReadInternal();
    else
      DoneRead(net::ERR_CACHE_MISS);
  }

  // There must not be any outstanding read request.
  DCHECK(!HasPendingRead());

  // Notify that network response is completed.
  NotifyNetworkEvent();

  url_loader_.reset();
  Release();
}

void BufferedResourceLoader::didFail(
    WebURLLoader* loader,
    const WebURLError& error) {
  DCHECK(!completed_);
  completed_ = true;

  // If there is a start callback, calls it.
  if (start_callback_.get()) {
    DoneStart(error.reason);
  }

  // If there is a pending read but the request failed, return with the
  // reason for the error.
  if (HasPendingRead()) {
    DoneRead(error.reason);
  }

  // Notify that network response is completed.
  NotifyNetworkEvent();

  url_loader_.reset();
  Release();
}

bool BufferedResourceLoader::HasSingleOrigin() const {
  return single_origin_;
}

/////////////////////////////////////////////////////////////////////////////
// Helper methods.
void BufferedResourceLoader::EnableDeferIfNeeded() {
  if (!defer_allowed_)
    return;

  if (!deferred_ &&
      buffer_->forward_bytes() >= buffer_->forward_capacity()) {
    deferred_ = true;

  if (url_loader_.get())
    url_loader_->setDefersLoading(true);

    NotifyNetworkEvent();
  }
}

void BufferedResourceLoader::DisableDeferIfNeeded() {
  if (deferred_ &&
      (!defer_allowed_ ||
       buffer_->forward_bytes() < buffer_->forward_capacity() / 2)) {
    deferred_ = false;

    if (url_loader_.get())
      url_loader_->setDefersLoading(false);

    NotifyNetworkEvent();
  }
}

bool BufferedResourceLoader::CanFulfillRead() {
  // If we are reading too far in the backward direction.
  if (first_offset_ < 0 &&
      first_offset_ + static_cast<int>(buffer_->backward_bytes()) < 0)
    return false;

  // If the start offset is too far ahead.
  if (first_offset_ >= static_cast<int>(buffer_->forward_bytes()))
    return false;

  // At the point, we verified that first byte requested is within the buffer.
  // If the request has completed, then just returns with what we have now.
  if (completed_)
    return true;

  // If the resource request is still active, make sure the whole requested
  // range is covered.
  if (last_offset_ > static_cast<int>(buffer_->forward_bytes()))
    return false;

  return true;
}

bool BufferedResourceLoader::WillFulfillRead() {
  // Reading too far in the backward direction.
  if (first_offset_ < 0 &&
      first_offset_ + static_cast<int>(buffer_->backward_bytes()) < 0)
    return false;

  // Try to read too far ahead.
  if (last_offset_ > kForwardWaitThreshold)
    return false;

  // The resource request has completed, there's no way we can fulfill the
  // read request.
  if (completed_)
    return false;

  return true;
}

void BufferedResourceLoader::ReadInternal() {
  // Seek to the first byte requested.
  bool ret = buffer_->Seek(first_offset_);
  DCHECK(ret);

  // Then do the read.
  int read = static_cast<int>(buffer_->Read(read_buffer_, read_size_));
  offset_ += first_offset_ + read;

  // And report with what we have read.
  DoneRead(read);
}

bool BufferedResourceLoader::VerifyPartialResponse(
    const WebURLResponse& response) {
  int first_byte_position, last_byte_position, instance_size;

  if (!MultipartResponseDelegate::ReadContentRanges(response,
                         &first_byte_position,
                         &last_byte_position,
                         &instance_size)) {
    return false;
  }

  if (instance_size != kPositionNotSpecified) {
    instance_size_ = instance_size;
  }

  if (first_byte_position_ != kPositionNotSpecified &&
      first_byte_position_ != first_byte_position) {
    return false;
  }

  // TODO(hclam): I should also check |last_byte_position|, but since
  // we will never make such a request that it is ok to leave it unimplemented.
  return true;
}

std::string BufferedResourceLoader::GenerateHeaders(
    int64 first_byte_position,
    int64 last_byte_position) {
  // Construct the value for the range header.
  std::string header;
  if (first_byte_position > kPositionNotSpecified &&
      last_byte_position > kPositionNotSpecified) {
    if (first_byte_position <= last_byte_position) {
      header = base::StringPrintf("bytes=%" PRId64 "-%" PRId64,
                                  first_byte_position,
                                  last_byte_position);
    }
  } else if (first_byte_position > kPositionNotSpecified) {
    header = base::StringPrintf("bytes=%" PRId64 "-",
                                first_byte_position);
  } else if (last_byte_position > kPositionNotSpecified) {
    NOTIMPLEMENTED() << "Suffix range not implemented";
  }
  return header;
}

void BufferedResourceLoader::DoneRead(int error) {
  read_callback_->RunWithParams(Tuple1<int>(error));
  read_callback_.reset();
  read_position_ = 0;
  read_size_ = 0;
  read_buffer_ = NULL;
  first_offset_ = 0;
  last_offset_ = 0;
}

void BufferedResourceLoader::DoneStart(int error) {
  start_callback_->RunWithParams(Tuple1<int>(error));
  start_callback_.reset();
}

void BufferedResourceLoader::NotifyNetworkEvent() {
  if (event_callback_.get())
    event_callback_->Run();
}

}  // namespace webkit_glue
