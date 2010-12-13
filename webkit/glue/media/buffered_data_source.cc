// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "media/base/filter_host.h"
#include "media/base/media_format.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKitClient.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLError.h"
#include "webkit/glue/media/buffered_data_source.h"
#include "webkit/glue/multipart_response_delegate.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webmediaplayer_impl.h"

using WebKit::WebFrame;
using WebKit::WebString;
using WebKit::WebURLError;
using WebKit::WebURLLoader;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;
using webkit_glue::MultipartResponseDelegate;

namespace {

const char kHttpScheme[] = "http";
const char kHttpsScheme[] = "https";
const char kDataScheme[] = "data";
const int64 kPositionNotSpecified = -1;
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

// Defines how long we should wait for more data before we declare a connection
// timeout and start a new request.
// TODO(hclam): Set it to 5s, calibrate this value later.
const int kTimeoutMilliseconds = 5000;

// Defines how many times we should try to read from a buffered resource loader
// before we declare a read error. After each failure of read from a buffered
// resource loader, a new one is created to be read.
const int kReadTrials = 3;

// BufferedDataSource has an intermediate buffer, this value governs the initial
// size of that buffer. It is set to 32KB because this is a typical read size
// of FFmpeg.
const int kInitialReadBufferSize = 32768;

// Returns true if |url| operates on HTTP protocol.
bool IsHttpProtocol(const GURL& url) {
  return url.SchemeIs(kHttpScheme) || url.SchemeIs(kHttpsScheme);
}

bool IsDataProtocol(const GURL& url) {
  return url.SchemeIs(kDataScheme);
}

}  // namespace

namespace webkit_glue {

/////////////////////////////////////////////////////////////////////////////
// BufferedResourceLoader
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
  request.setHTTPHeaderField(WebString::fromUTF8("Range"),
                             WebString::fromUTF8(GenerateHeaders(
                                                 first_byte_position_,
                                                 last_byte_position_)));
  frame->setReferrerForRequest(request, WebKit::WebURL());
  // TODO(annacc): we should be using createAssociatedURLLoader() instead?
  frame->dispatchWillSendRequest(request);

  // This flag is for unittests as we don't want to reset |url_loader|
  if (!keep_test_loader_)
    url_loader_.reset(WebKit::webKitClient()->createURLLoader());

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

int64 BufferedResourceLoader::GetBufferedFirstBytePosition() {
  if (buffer_.get())
    return offset_ - static_cast<int>(buffer_->backward_bytes());
  return kPositionNotSpecified;
}

int64 BufferedResourceLoader::GetBufferedLastBytePosition() {
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
// BufferedResourceLoader, WebKit::WebURLLoaderClient implementations.
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

  if (!IsProtocolSupportedForMedia(newRequest.url())) {
    // Set the url in the request to an invalid value (empty url).
    newRequest.setURL(WebKit::WebURL());
    DoneStart(net::ERR_ADDRESS_INVALID);
    Stop();
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
  if (IsHttpProtocol(url_)) {
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

  // Expected content length can be -1, in that case |content_length_| is
  // not specified and this is a streaming response.
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

/////////////////////////////////////////////////////////////////////////////
// BufferedResourceLoader, private
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

/////////////////////////////////////////////////////////////////////////////
// BufferedDataSource
BufferedDataSource::BufferedDataSource(
    MessageLoop* render_loop,
    WebFrame* frame)
    : total_bytes_(kPositionNotSpecified),
      loaded_(false),
      streaming_(false),
      frame_(frame),
      single_origin_(true),
      loader_(NULL),
      network_activity_(false),
      initialize_callback_(NULL),
      read_callback_(NULL),
      read_position_(0),
      read_size_(0),
      read_buffer_(NULL),
      read_attempts_(0),
      intermediate_read_buffer_(new uint8[kInitialReadBufferSize]),
      intermediate_read_buffer_size_(kInitialReadBufferSize),
      render_loop_(render_loop),
      stop_signal_received_(false),
      stopped_on_render_loop_(false),
      media_is_paused_(true),
      using_range_request_(true) {
}

BufferedDataSource::~BufferedDataSource() {
}

// A factory method to create BufferedResourceLoader using the read parameters.
// This method can be overrided to inject mock BufferedResourceLoader object
// for testing purpose.
BufferedResourceLoader* BufferedDataSource::CreateResourceLoader(
    int64 first_byte_position, int64 last_byte_position) {
  DCHECK(MessageLoop::current() == render_loop_);

  return new BufferedResourceLoader(url_,
                                    first_byte_position,
                                    last_byte_position);
}

// This method simply returns kTimeoutMilliseconds. The purpose of this
// method is to be overidded so as to provide a different timeout value
// for testing purpose.
base::TimeDelta BufferedDataSource::GetTimeoutMilliseconds() {
  return base::TimeDelta::FromMilliseconds(kTimeoutMilliseconds);
}

/////////////////////////////////////////////////////////////////////////////
// BufferedDataSource, media::Filter implementation
void BufferedDataSource::Initialize(const std::string& url,
                                    media::FilterCallback* callback) {
  // Saves the url.
  url_ = GURL(url);

  if (!IsProtocolSupportedForMedia(url_)) {
    // This method is called on the thread where host() lives so it is safe
    // to make this call.
    host()->SetError(media::PIPELINE_ERROR_NETWORK);
    callback->Run();
    delete callback;
    return;
  }

  DCHECK(callback);
  initialize_callback_.reset(callback);

  media_format_.SetAsString(media::MediaFormat::kMimeType,
                            media::mime_type::kApplicationOctetStream);
  media_format_.SetAsString(media::MediaFormat::kURL, url);

  // Post a task to complete the initialization task.
  render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &BufferedDataSource::InitializeTask));
}

bool BufferedDataSource::IsUrlSupported(const std::string& url) {
  GURL gurl(url);

  // This data source doesn't support data:// protocol so reject it.
  return IsProtocolSupportedForMedia(gurl) && !IsDataProtocol(gurl);
}

void BufferedDataSource::Stop(media::FilterCallback* callback) {
  {
    AutoLock auto_lock(lock_);
    stop_signal_received_ = true;
  }
  if (callback) {
    callback->Run();
    delete callback;
  }

  render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &BufferedDataSource::CleanupTask));
}

void BufferedDataSource::SetPlaybackRate(float playback_rate) {
  render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &BufferedDataSource::SetPlaybackRateTask,
                        playback_rate));
}

/////////////////////////////////////////////////////////////////////////////
// BufferedDataSource, media::DataSource implementation
void BufferedDataSource::Read(int64 position, size_t size, uint8* data,
                              media::DataSource::ReadCallback* read_callback) {
  render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &BufferedDataSource::ReadTask,
                        position, static_cast<int>(size), data, read_callback));
}

bool BufferedDataSource::GetSize(int64* size_out) {
  if (total_bytes_ != kPositionNotSpecified) {
    *size_out = total_bytes_;
    return true;
  }
  *size_out = 0;
  return false;
}

bool BufferedDataSource::IsStreaming() {
  return streaming_;
}

bool BufferedDataSource::HasSingleOrigin() {
  DCHECK(MessageLoop::current() == render_loop_);
  return single_origin_;
}

void BufferedDataSource::Abort() {
  DCHECK(MessageLoop::current() == render_loop_);

  // If we are told to abort, immediately return from any pending read
  // with an error.
  if (read_callback_.get()) {
      AutoLock auto_lock(lock_);
      DoneRead_Locked(net::ERR_FAILED);
  }

  CleanupTask();
  frame_ = NULL;
}

/////////////////////////////////////////////////////////////////////////////
// BufferedDataSource, render thread tasks
void BufferedDataSource::InitializeTask() {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(!loader_.get());
  if (stopped_on_render_loop_)
    return;

  // Kick starts the watch dog task that will handle connection timeout.
  // We run the watch dog 2 times faster the actual timeout so as to catch
  // the timeout more accurately.
  watch_dog_timer_.Start(
      GetTimeoutMilliseconds() / 2,
      this,
      &BufferedDataSource::WatchDogTask);

  if (IsHttpProtocol(url_)) {
    // Fetch only first 1024 bytes as this usually covers the header portion
    // of a media file that gives enough information about the codecs, etc.
    // This also serve as a probe to determine server capability to serve
    // range request.
    // TODO(hclam): Do some experiments for the best approach.
    loader_ = CreateResourceLoader(0, 1024);
    loader_->Start(
        NewCallback(this, &BufferedDataSource::HttpInitialStartCallback),
        NewCallback(this, &BufferedDataSource::NetworkEventCallback),
        frame_);
  } else {
    // For all other protocols, assume they support range request. We fetch
    // the full range of the resource to obtain the instance size because
    // we won't be served HTTP headers.
    loader_ = CreateResourceLoader(-1, -1);
    loader_->Start(
        NewCallback(this, &BufferedDataSource::NonHttpInitialStartCallback),
        NewCallback(this, &BufferedDataSource::NetworkEventCallback),
        frame_);
  }
}

void BufferedDataSource::ReadTask(
     int64 position, int read_size, uint8* buffer,
     media::DataSource::ReadCallback* read_callback) {
  DCHECK(MessageLoop::current() == render_loop_);
  if (stopped_on_render_loop_)
    return;

  DCHECK(!read_callback_.get());
  DCHECK(read_callback);

  // Saves the read parameters.
  read_position_ = position;
  read_size_ = read_size;
  read_callback_.reset(read_callback);
  read_buffer_ = buffer;
  read_submitted_time_ = base::Time::Now();
  read_attempts_ = 0;

  // Call to read internal to perform the actual read.
  ReadInternal();
}

void BufferedDataSource::CleanupTask() {
  DCHECK(MessageLoop::current() == render_loop_);
  if (stopped_on_render_loop_)
    return;

  // Stop the watch dog.
  watch_dog_timer_.Stop();

  // We just need to stop the loader, so it stops activity.
  if (loader_.get())
    loader_->Stop();

  // Reset the parameters of the current read request.
  read_callback_.reset();
  read_position_ = 0;
  read_size_ = 0;
  read_buffer_ = 0;
  read_submitted_time_ = base::Time();
  read_attempts_ = 0;

  // Signal that stop task has finished execution.
  stopped_on_render_loop_ = true;
}

void BufferedDataSource::RestartLoadingTask() {
  DCHECK(MessageLoop::current() == render_loop_);
  if (stopped_on_render_loop_)
    return;

  // If there's no outstanding read then return early.
  if (!read_callback_.get())
    return;

  loader_ = CreateResourceLoader(read_position_, -1);
  loader_->SetAllowDefer(!media_is_paused_);
  loader_->Start(
      NewCallback(this, &BufferedDataSource::PartialReadStartCallback),
      NewCallback(this, &BufferedDataSource::NetworkEventCallback),
      frame_);
}

void BufferedDataSource::WatchDogTask() {
  DCHECK(MessageLoop::current() == render_loop_);
  if (stopped_on_render_loop_)
    return;

  // We only care if there is an active read request.
  if (!read_callback_.get())
    return;

  DCHECK(loader_.get());
  base::TimeDelta delta = base::Time::Now() - read_submitted_time_;
  if (delta < GetTimeoutMilliseconds())
    return;

  // TODO(hclam): Maybe raise an error here. But if an error is reported
  // the whole pipeline may get destroyed...
  if (read_attempts_ >= kReadTrials)
    return;

  ++read_attempts_;
  read_submitted_time_ = base::Time::Now();

  // Stops the current loader and creates a new resource loader and
  // retry the request.
  loader_->Stop();
  loader_ = CreateResourceLoader(read_position_, -1);
  loader_->SetAllowDefer(!media_is_paused_);
  loader_->Start(
      NewCallback(this, &BufferedDataSource::PartialReadStartCallback),
      NewCallback(this, &BufferedDataSource::NetworkEventCallback),
      frame_);
}

void BufferedDataSource::SetPlaybackRateTask(float playback_rate) {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(loader_.get());

  bool previously_paused = media_is_paused_;
  media_is_paused_ = (playback_rate == 0.0);

  // Disallow deferring data when we are pausing, allow deferring data
  // when we resume playing.
  if (previously_paused && !media_is_paused_) {
    loader_->SetAllowDefer(true);
  } else if (!previously_paused && media_is_paused_) {
    loader_->SetAllowDefer(false);
  }
}

// This method is the place where actual read happens, |loader_| must be valid
// prior to make this method call.
void BufferedDataSource::ReadInternal() {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(loader_);

  // First we prepare the intermediate read buffer for BufferedResourceLoader
  // to write to.
  if (read_size_ > intermediate_read_buffer_size_) {
    intermediate_read_buffer_.reset(new uint8[read_size_]);
  }

  // Perform the actual read with BufferedResourceLoader.
  loader_->Read(read_position_, read_size_, intermediate_read_buffer_.get(),
                NewCallback(this, &BufferedDataSource::ReadCallback));
}

// Method to report the results of the current read request. Also reset all
// the read parameters.
void BufferedDataSource::DoneRead_Locked(int error) {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(read_callback_.get());
  lock_.AssertAcquired();

  if (error >= 0) {
    read_callback_->RunWithParams(Tuple1<size_t>(error));
  } else {
    read_callback_->RunWithParams(
        Tuple1<size_t>(static_cast<size_t>(media::DataSource::kReadError)));
  }

  read_callback_.reset();
  read_position_ = 0;
  read_size_ = 0;
  read_buffer_ = 0;
}

void BufferedDataSource::DoneInitialization_Locked() {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(initialize_callback_.get());
  lock_.AssertAcquired();

  initialize_callback_->Run();
  initialize_callback_.reset();
}

/////////////////////////////////////////////////////////////////////////////
// BufferedDataSource, callback methods.
// These methods are called on the render thread for the events reported by
// BufferedResourceLoader.
void BufferedDataSource::HttpInitialStartCallback(int error) {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(loader_.get());

  // Check if the request ended up at a different origin via redirect.
  single_origin_ = url_.GetOrigin() == loader_->url().GetOrigin();

  int64 instance_size = loader_->instance_size();
  bool partial_response = loader_->partial_response();
  bool success = error == net::OK;

  if (success) {
    // TODO(hclam): Needs more thinking about supporting servers without range
    // request or their partial response is not complete.
    total_bytes_ = instance_size;
    loaded_ = false;
    streaming_ = (instance_size == kPositionNotSpecified) || !partial_response;
  } else {
    // TODO(hclam): In case of failure, we can retry several times.
    loader_->Stop();
  }

  if (error == net::ERR_INVALID_RESPONSE && using_range_request_) {
    // Assuming that the Range header was causing the problem. Retry without
    // the Range header.
    using_range_request_ = false;
    loader_ = CreateResourceLoader(-1, -1);
    loader_->Start(
        NewCallback(this, &BufferedDataSource::HttpInitialStartCallback),
        NewCallback(this, &BufferedDataSource::NetworkEventCallback),
        frame_);
    return;
  }

  // We need to prevent calling to filter host and running the callback if
  // we have received the stop signal. We need to lock down the whole callback
  // method to prevent bad things from happening. The reason behind this is
  // that we cannot guarantee tasks on render thread have completely stopped
  // when we receive the Stop() method call. The only way to solve this is to
  // let tasks on render thread to run but make sure they don't call outside
  // this object when Stop() method is ever called. Locking this method is safe
  // because |lock_| is only acquired in tasks on render thread.
  AutoLock auto_lock(lock_);
  if (stop_signal_received_)
    return;

  if (!success) {
    host()->SetError(media::PIPELINE_ERROR_NETWORK);
    DoneInitialization_Locked();
    return;
  }

  if (streaming_) {
    // If the server didn't reply with an instance size, it is likely this
    // is a streaming response.
    host()->SetStreaming(true);
  } else {
    // This value governs the range that we can seek to.
    // TODO(hclam): Report the correct value of buffered bytes.
    host()->SetTotalBytes(total_bytes_);
    host()->SetBufferedBytes(0);
  }

  // Currently, only files can be used reliably w/o a network.
  host()->SetLoaded(false);
  DoneInitialization_Locked();
}

void BufferedDataSource::NonHttpInitialStartCallback(int error) {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(loader_.get());

  // Check if the request ended up at a different origin via redirect.
  single_origin_ = url_.GetOrigin() == loader_->url().GetOrigin();

  int64 instance_size = loader_->instance_size();
  bool success = error == net::OK && instance_size != kPositionNotSpecified;

  if (success) {
    total_bytes_ = instance_size;
    loaded_ = true;
  } else {
    loader_->Stop();
  }

  // We need to prevent calling to filter host and running the callback if
  // we have received the stop signal. We need to lock down the whole callback
  // method to prevent bad things from happening. The reason behind this is
  // that we cannot guarantee tasks on render thread have completely stopped
  // when we receive the Stop() method call. The only way to solve this is to
  // let tasks on render thread to run but make sure they don't call outside
  // this object when Stop() method is ever called. Locking this method is safe
  // because |lock_| is only acquired in tasks on render thread.
  AutoLock auto_lock(lock_);
  if (stop_signal_received_)
    return;

  if (success) {
    host()->SetTotalBytes(total_bytes_);
    host()->SetBufferedBytes(total_bytes_);
    host()->SetLoaded(loaded_);
  } else {
    host()->SetError(media::PIPELINE_ERROR_NETWORK);
  }
  DoneInitialization_Locked();
}

void BufferedDataSource::PartialReadStartCallback(int error) {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(loader_.get());

  // This callback method is invoked after we have verified the server has
  // range request capability, so as a safety guard verify again the response
  // is partial.
  if (error == net::OK && loader_->partial_response()) {
    // Once the range request has started successfully, we can proceed with
    // reading from it.
    ReadInternal();
    return;
  }

  // Stop the resource loader since we have received an error.
  loader_->Stop();

  // We need to prevent calling to filter host and running the callback if
  // we have received the stop signal. We need to lock down the whole callback
  // method to prevent bad things from happening. The reason behind this is
  // that we cannot guarantee tasks on render thread have completely stopped
  // when we receive the Stop() method call. So only way to solve this is to
  // let tasks on render thread to run but make sure they don't call outside
  // this object when Stop() method is ever called. Locking this method is
  // safe because |lock_| is only acquired in tasks on render thread.
  AutoLock auto_lock(lock_);
  if (stop_signal_received_)
    return;
  DoneRead_Locked(net::ERR_INVALID_RESPONSE);
}

void BufferedDataSource::ReadCallback(int error) {
  DCHECK(MessageLoop::current() == render_loop_);

  if (error < 0) {
    DCHECK(loader_.get());

    // Stop the resource load if it failed.
    loader_->Stop();

    if (error == net::ERR_CACHE_MISS) {
      render_loop_->PostTask(FROM_HERE,
          NewRunnableMethod(this, &BufferedDataSource::RestartLoadingTask));
      return;
    }
  }

  // We need to prevent calling to filter host and running the callback if
  // we have received the stop signal. We need to lock down the whole callback
  // method to prevent bad things from happening. The reason behind this is
  // that we cannot guarantee tasks on render thread have completely stopped
  // when we receive the Stop() method call. So only way to solve this is to
  // let tasks on render thread to run but make sure they don't call outside
  // this object when Stop() method is ever called. Locking this method is safe
  // because |lock_| is only acquired in tasks on render thread.
  AutoLock auto_lock(lock_);
  if (stop_signal_received_)
    return;

  if (error > 0) {
    // If a position error code is received, read was successful. So copy
    // from intermediate read buffer to the target read buffer.
    memcpy(read_buffer_, intermediate_read_buffer_.get(), error);
  }
  DoneRead_Locked(error);
}

void BufferedDataSource::NetworkEventCallback() {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(loader_.get());

  // In case of non-HTTP request we don't need to report network events,
  // so return immediately.
  if (loaded_)
    return;

  bool network_activity = loader_->network_activity();
  int64 buffered_last_byte_position = loader_->GetBufferedLastBytePosition();

  // If we get an unspecified value, return immediately.
  if (buffered_last_byte_position == kPositionNotSpecified)
    return;

  // We need to prevent calling to filter host and running the callback if
  // we have received the stop signal. We need to lock down the whole callback
  // method to prevent bad things from happening. The reason behind this is
  // that we cannot guarantee tasks on render thread have completely stopped
  // when we receive the Stop() method call. So only way to solve this is to
  // let tasks on render thread to run but make sure they don't call outside
  // this object when Stop() method is ever called. Locking this method is safe
  // because |lock_| is only acquired in tasks on render thread.
  AutoLock auto_lock(lock_);
  if (stop_signal_received_)
    return;

  if (network_activity != network_activity_) {
    network_activity_ = network_activity;
    host()->SetNetworkActivity(network_activity);
  }
  host()->SetBufferedBytes(buffered_last_byte_position + 1);
}

}  // namespace webkit_glue
