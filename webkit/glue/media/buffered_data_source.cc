// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/common/extensions/url_pattern.h"
#include "media/base/filter_host.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "webkit/glue/media/buffered_data_source.h"
#include "webkit/glue/webkit_glue.h"

namespace {

const char kHttpScheme[] = "http";
const char kHttpsScheme[] = "https";
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
// TODO(hclam): Use this value when retry is implemented.
// TODO(hclam): Set it to 5s, calibrate this value later.
const int kTimeoutMilliseconds = 5000;

// Defines how many times we should try to read from a buffered resource loader
// before we declare a read error. After each failure of read from a buffered
// resource loader, a new one is created to be read.
// TODO(hclam): Use this value when retry is implemented.
const int kReadTrials = 3;

// BufferedDataSource has an intermediate buffer, this value governs the initial
// size of that buffer. It is set to 32KB because this is a typical read size
// of FFmpeg.
const int kInitialReadBufferSize = 32768;

// Returns true if |url| operates on HTTP protocol.
bool IsHttpProtocol(const GURL& url) {
  return url.SchemeIs(kHttpScheme) || url.SchemeIs(kHttpsScheme);
}

}  // namespace

namespace webkit_glue {

/////////////////////////////////////////////////////////////////////////////
// BufferedResourceLoader
BufferedResourceLoader::BufferedResourceLoader(
    webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory,
    const GURL& url,
    int64 first_byte_position,
    int64 last_byte_position)
    : buffer_(new media::SeekableBuffer(kBackwardCapcity, kForwardCapacity)),
      deferred_(false),
      completed_(false),
      range_requested_(false),
      partial_response_(false),
      bridge_factory_(bridge_factory),
      url_(url),
      first_byte_position_(first_byte_position),
      last_byte_position_(last_byte_position),
      start_callback_(NULL),
      bridge_(NULL),
      offset_(0),
      content_length_(kPositionNotSpecified),
      instance_size_(kPositionNotSpecified),
      read_callback_(NULL),
      read_position_(0),
      read_size_(0),
      read_buffer_(NULL),
      first_offset_(0),
      last_offset_(0) {
}

BufferedResourceLoader::~BufferedResourceLoader() {
}

void BufferedResourceLoader::Start(net::CompletionCallback* start_callback,
                                   NetworkEventCallback* event_callback) {
  // Make sure we have not started.
  DCHECK(!bridge_.get());
  DCHECK(!start_callback_.get());
  DCHECK(!event_callback_.get());
  DCHECK(start_callback);
  DCHECK(event_callback);

  start_callback_.reset(start_callback);
  event_callback_.reset(event_callback);

  if (first_byte_position_ != kPositionNotSpecified) {
    range_requested_ = true;
    // TODO(hclam): server may not support range request so |offset_| may not
    // equal to |first_byte_position_|.
    offset_ = first_byte_position_;
  }

  // Creates the bridge on render thread since we can only access
  // ResourceDispatcher on this thread.
  bridge_.reset(
      bridge_factory_->CreateBridge(
          url_,
          IsMediaCacheEnabled() ? net::LOAD_NORMAL : net::LOAD_BYPASS_CACHE,
          first_byte_position_,
          last_byte_position_));

  // Increment the reference count right before we start the request. This
  // reference will be release when this request has ended.
  AddRef();

  // And start the resource loading.
  bridge_->Start(this);
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

  if (bridge_.get()) {
    // Cancel the request. This method call will cancel the request
    // asynchronously. We may still get data or messages until we receive
    // a response completed message.
    if (deferred_)
      bridge_->SetDefersLoading(false);
    deferred_ = false;
    bridge_->Cancel();
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

/////////////////////////////////////////////////////////////////////////////
// BufferedResourceLoader,
//     webkit_glue::ResourceLoaderBridge::Peer implementations
bool BufferedResourceLoader::OnReceivedRedirect(
    const GURL& new_url,
    const webkit_glue::ResourceLoaderBridge::ResponseInfo& info) {
  DCHECK(bridge_.get());

  // Save the new URL.
  url_ = new_url;

  // The load may have been stopped and |start_callback| is destroyed.
  // In this case we shouldn't do anything.
  if (!start_callback_.get())
    return true;

  if (!IsProtocolSupportedForMedia(new_url)) {
    DoneStart(net::ERR_ADDRESS_INVALID);
    Stop();
    return false;
  }
  return true;
}

void BufferedResourceLoader::OnReceivedResponse(
    const webkit_glue::ResourceLoaderBridge::ResponseInfo& info,
    bool content_filtered) {
  DCHECK(bridge_.get());

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
    if (!info.headers) {
      // We expect to receive headers because this is a HTTP or HTTPS protocol,
      // if not report failure.
      error = net::ERR_INVALID_RESPONSE;
    } else {
      if (info.headers->response_code() == kHttpPartialContent)
        partial_response_ = true;

      if (range_requested_ && partial_response_) {
        // If we have verified the partial response and it is correct, we will
        // return net::OK.
        if (!VerifyPartialResponse(info))
          error = net::ERR_INVALID_RESPONSE;
      } else if (info.headers->response_code() != kHttpOK) {
        // We didn't request a range but server didn't reply with "200 OK".
        error = net::ERR_FAILED;
      }
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

  // |info.content_length| can be -1, in that case |content_length_| is
  // not specified and this is a streaming response.
  content_length_ = info.content_length;

  // If we have not requested a range, then the size of the instance is equal
  // to the content length.
  if (!partial_response_)
    instance_size_ = content_length_;

  // Calls with a successful response.
  DoneStart(net::OK);
}

void BufferedResourceLoader::OnReceivedData(const char* data, int len) {
  DCHECK(bridge_.get());

  // If this loader has been stopped, |buffer_| would be destroyed.
  // In this case we shouldn't do anything.
  if (!buffer_.get())
    return;

  // Writes more data to |buffer_|.
  buffer_->Append(len, reinterpret_cast<const uint8*>(data));

  // If there is an active read request, try to fulfill the request.
  if (HasPendingRead() && CanFulfillRead()) {
    ReadInternal();
  }

  // At last see if the buffer is full and we need to defer the downloading.
  EnableDeferIfNeeded();

  // Notify that we have received some data.
  NotifyNetworkEvent();
}

void BufferedResourceLoader::OnCompletedRequest(
    const URLRequestStatus& status, const std::string& security_info) {
  DCHECK(bridge_.get());

  // Saves the information that the request has completed.
  completed_ = true;

  // If there is a start callback, calls it.
  if (start_callback_.get()) {
    DoneStart(status.os_error());
  }

  // If there is a pending read but the request has ended, returns with what
  // we have.
  if (HasPendingRead()) {
    // Make sure we have a valid buffer before we satisfy a read request.
    DCHECK(buffer_.get());

    if (status.is_success()) {
      // Try to fulfill with what is in the buffer.
      if (CanFulfillRead())
        ReadInternal();
      else
        DoneRead(net::ERR_CACHE_MISS);
    } else {
      // If the request has failed, then fail the read.
      DoneRead(net::ERR_FAILED);
    }
  }

  // There must not be any outstanding read request.
  DCHECK(!HasPendingRead());

  // Notify that network response is completed.
  NotifyNetworkEvent();

  // We incremented the reference count when the loader was started. We balance
  // that reference here so that we get destroyed. This is also the only safe
  // place to destroy the ResourceLoaderBridge.
  bridge_.reset();
  Release();
}

/////////////////////////////////////////////////////////////////////////////
// BufferedResourceLoader, private
void BufferedResourceLoader::EnableDeferIfNeeded() {
  if (!deferred_ &&
      buffer_->forward_bytes() >= buffer_->forward_capacity()) {
    deferred_ = true;

    if (bridge_.get())
      bridge_->SetDefersLoading(true);

    NotifyNetworkEvent();
  }
}

void BufferedResourceLoader::DisableDeferIfNeeded() {
  if (deferred_ &&
      buffer_->forward_bytes() < buffer_->forward_capacity() / 2) {
    deferred_ = false;

    if (bridge_.get())
      bridge_->SetDefersLoading(false);

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
  int read = static_cast<int>(buffer_->Read(read_size_, read_buffer_));
  offset_ += first_offset_ + read;

  // And report with what we have read.
  DoneRead(read);
}

bool BufferedResourceLoader::VerifyPartialResponse(
    const ResourceLoaderBridge::ResponseInfo& info) {
  int64 first_byte_position, last_byte_position, instance_size;
  if (!info.headers->GetContentRange(&first_byte_position,
                                     &last_byte_position,
                                     &instance_size)) {
    return false;
  }

  if (instance_size != kPositionNotSpecified)
    instance_size_ = instance_size;

  if (first_byte_position_ != -1 &&
      first_byte_position_ != first_byte_position) {
    return false;
  }

  // TODO(hclam): I should also check |last_byte_position|, but since
  // we will never make such a request that it is ok to leave it unimplemented.
  return true;
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
// BufferedDataSource, protected
BufferedDataSource::BufferedDataSource(
    MessageLoop* render_loop,
    webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory)
    : total_bytes_(kPositionNotSpecified),
      loaded_(false),
      streaming_(false),
      bridge_factory_(bridge_factory),
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
      stopped_on_render_loop_(false) {
}

BufferedDataSource::~BufferedDataSource() {
}

// A factory method to create BufferedResourceLoader using the read parameters.
// This method can be overrided to inject mock BufferedResourceLoader object
// for testing purpose.
BufferedResourceLoader* BufferedDataSource::CreateResourceLoader(
    int64 first_byte_position, int64 last_byte_position) {
  DCHECK(MessageLoop::current() == render_loop_);

  return new BufferedResourceLoader(bridge_factory_.get(), url_,
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
// BufferedDataSource, media::MediaFilter implementation
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

void BufferedDataSource::Stop() {
  {
    AutoLock auto_lock(lock_);
    stop_signal_received_ = true;
  }
  render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &BufferedDataSource::StopTask));
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

/////////////////////////////////////////////////////////////////////////////
// BufferedDataSource, render thread tasks
void BufferedDataSource::InitializeTask() {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(!loader_.get());
  DCHECK(!stopped_on_render_loop_);

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
        NewCallback(this, &BufferedDataSource::NetworkEventCallback));
  } else {
    // For all other protocols, assume they support range request. We fetch
    // the full range of the resource to obtain the instance size because
    // we won't be served HTTP headers.
    loader_ = CreateResourceLoader(-1, -1);
    loader_->Start(
        NewCallback(this, &BufferedDataSource::NonHttpInitialStartCallback),
        NewCallback(this, &BufferedDataSource::NetworkEventCallback));
  }
}

void BufferedDataSource::ReadTask(
     int64 position, int read_size, uint8* buffer,
     media::DataSource::ReadCallback* read_callback) {
  DCHECK(MessageLoop::current() == render_loop_);

  // If StopTask() was executed we should return immediately. We check this
  // variable to prevent doing any actual work after clean up was done. We do
  // not check |stop_signal_received_| because anything use of it has to be
  // within |lock_| which is not desirable.
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

void BufferedDataSource::StopTask() {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(!stopped_on_render_loop_);

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

  // This variable is set in StopTask(). We check this and do an early return.
  // The sequence of actions which enable this conditions is:
  // 1. Stop() is called from the pipeline.
  // 2. ReadCallback() is called from the resource loader.
  // 3. StopTask() is executed.
  // 4. RestartLoadingTask() is executed.
  if (stopped_on_render_loop_)
    return;

  // If there's no outstanding read then return early.
  if (!read_callback_.get())
    return;

  loader_ = CreateResourceLoader(read_position_, -1);
  loader_->Start(
      NewCallback(this, &BufferedDataSource::PartialReadStartCallback),
      NewCallback(this, &BufferedDataSource::NetworkEventCallback));
}

void BufferedDataSource::WatchDogTask() {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(!stopped_on_render_loop_);

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
  loader_->Start(
      NewCallback(this, &BufferedDataSource::PartialReadStartCallback),
      NewCallback(this, &BufferedDataSource::NetworkEventCallback));
}

// This method is the place where actual read happens, |loader_| must be valid
// prior to make this method call.
void BufferedDataSource::ReadInternal() {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(loader_.get());

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
