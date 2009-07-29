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
#include "webkit/glue/webappcachecontext.h"

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

// The maximum offset to seek to, this value limits the range of seek to
// prevent corruption of calculations. The buffer has a maximum size of 12MB
// so this is enough to contain every valid operations.
const int kMaxSeek = 100 * kMegabyte;

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

// A helper method that accepts only HTTP, HTTPS and FILE protocol.
bool IsSchemeSupported(const GURL& url) {
  return url.SchemeIs(kHttpScheme) ||
         url.SchemeIs(kHttpsScheme) ||
         url.SchemeIsFile();
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
      bridge_factory_(bridge_factory),
      url_(url),
      first_byte_position_(first_byte_position),
      last_byte_position_(last_byte_position),
      start_callback_(NULL),
      bridge_(NULL),
      offset_(0),
      content_length_(kPositionNotSpecified),
      read_callback_(NULL),
      read_position_(0),
      read_size_(0),
      read_buffer_(NULL),
      first_offset_(0),
      last_offset_(0) {
}

BufferedResourceLoader::~BufferedResourceLoader() {
}

void BufferedResourceLoader::Start(net::CompletionCallback* start_callback) {
  // Make sure we have not started.
  DCHECK(!bridge_.get());
  DCHECK(!start_callback_.get());
  DCHECK(start_callback);

  start_callback_.reset(start_callback);

  if (first_byte_position_ != kPositionNotSpecified) {
    range_requested_ = true;
    // TODO(hclam): server may not support range request so |offset_| may not
    // equal to |first_byte_position_|.
    offset_ = first_byte_position_;
  }

  // Creates the bridge on render thread since we can only access
  // ResourceDispatcher on this thread.
  bridge_.reset(bridge_factory_->CreateBridge(url_,
                                              net::LOAD_BYPASS_CACHE,
                                              first_byte_position_,
                                              last_byte_position_));

  // And start the resource loading.
  bridge_->Start(this);
}

void BufferedResourceLoader::Stop() {
  // Reset callbacks.
  start_callback_.reset();
  read_callback_.reset();

  // Destroy internal buffer.
  buffer_.reset();

  if (bridge_.get()) {
    // Cancel the resource request.
    bridge_->Cancel();
    bridge_.reset();
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

  // Saves the parameter of reading.
  read_callback_.reset(read_callback);
  read_position_ = position;
  read_size_ = read_size;
  read_buffer_ = buffer;

  // Check that the read parameters are within the range that we can handle.
  // If the read request is made too far from the current offset, report that
  // we cannot serve the request.
  if (VerifyRead()) {
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
    return;
  }

  // TODO(hclam): We should report a better error code than just 0.
  DoneRead(0);
}

/////////////////////////////////////////////////////////////////////////////
// BufferedResourceLoader,
//     webkit_glue::ResourceLoaderBridge::Peer implementations
void BufferedResourceLoader::OnReceivedRedirect(const GURL& new_url) {
  DCHECK(bridge_.get());
  DCHECK(start_callback_.get());

  // Saves the new URL.
  url_ = new_url;

  // If we got redirected to an unsupported protocol then stop.
  if (!IsSchemeSupported(new_url)) {
    DoneStart(net::ERR_ADDRESS_INVALID);
    Stop();
  }
}

void BufferedResourceLoader::OnReceivedResponse(
    const webkit_glue::ResourceLoaderBridge::ResponseInfo& info,
    bool content_filtered) {
  DCHECK(bridge_.get());
  DCHECK(start_callback_.get());

  int64 first_byte_position = -1;
  int64 last_byte_position = -1;
  int64 instance_size = -1;

  // The file:// protocol should be able to serve any request we want, so we
  // take an exception for file protocol.
  if (!url_.SchemeIsFile()) {
    int error = net::OK;
    if (!info.headers) {
      // We expect to receive headers because this is a HTTP or HTTPS protocol,
      // if not report failure.
      error = net::ERR_INVALID_RESPONSE;
    } else if (range_requested_) {
      if (info.headers->response_code() != kHttpPartialContent ||
          !info.headers->GetContentRange(&first_byte_position,
                                         &last_byte_position,
                                          &instance_size)) {
        // We requested a range, but server didn't reply with partial content or
        // the "Content-Range" header is corrupted.
        // TODO(hclam): should also make sure this is the range we requested.
        error = net::ERR_INVALID_RESPONSE;
      }
    } else if (info.headers->response_code() != kHttpOK) {
      // We didn't request a range but server didn't reply with "200 OK".
      error = net::ERR_FAILED;
    }

    if (error != net::OK) {
      DoneStart(error);
      Stop();
      return;
    }
  }

  // |info.content_length| can be -1, in that case |content_length_| is
  // not specified and this is a streaming response.
  content_length_ = info.content_length;

  // We only care about the first byte position if it's given by the server.
  // TODO(hclam): If server replies with a different offset, consider failing
  // here.
  if (first_byte_position != kPositionNotSpecified)
    offset_ = first_byte_position;

  // Calls with a successful response.
  DoneStart(net::OK);
}

void BufferedResourceLoader::OnReceivedData(const char* data, int len) {
  DCHECK(bridge_.get());
  DCHECK(buffer_.get());

  // Writes more data to |buffer_|.
  buffer_->Append(len, reinterpret_cast<const uint8*>(data));

  // If there is an active read request, try to fulfill the request.
  if (HasPendingRead() && CanFulfillRead()) {
    ReadInternal();
  }

  // At last see if the buffer is full and we need to defer the downloading.
  EnableDeferIfNeeded();
}

void BufferedResourceLoader::OnCompletedRequest(
    const URLRequestStatus& status, const std::string& security_info) {
  DCHECK(bridge_.get());
  DCHECK(buffer_.get());

  // Saves the information that the request has completed.
  completed_ = true;

  // After the response has completed, we don't need the bridge any more.
  bridge_.reset();

  // If there is a start callback, calls it.
  if (start_callback_.get()) {
    DoneStart(status.os_error());
  }

  // If there is a pending read but the request has ended, returns with what we
  // have.
  if (HasPendingRead()) {
    // If the request has failed, then fail the read.
    if (!status.is_success()) {
      DoneRead(net::ERR_FAILED);
      return;
    }

    // Otherwise try to fulfill with what is in the buffer.
    if (CanFulfillRead())
      ReadInternal();
    else
      DoneRead(net::ERR_CACHE_MISS);
  }

  // There must not be any outstanding read request.
  DCHECK(!read_callback_.get());
}

/////////////////////////////////////////////////////////////////////////////
// BufferedResourceLoader, private
void BufferedResourceLoader::EnableDeferIfNeeded() {
  if (!deferred_ &&
      buffer_->forward_bytes() >= buffer_->forward_capacity()) {
    deferred_ = true;

    if (bridge_.get())
      bridge_->SetDefersLoading(true);
  }
}

void BufferedResourceLoader::DisableDeferIfNeeded() {
  if (deferred_ &&
      buffer_->forward_bytes() < buffer_->forward_capacity() / 2) {
    deferred_ = false;

    if (bridge_.get())
      bridge_->SetDefersLoading(false);
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

bool BufferedResourceLoader::VerifyRead() {
  // Make sure |offset_| and |read_position_| does not differ by a large
  // amount
  if (read_position_ > offset_ + kMaxSeek)
    return false;
  else if (read_position_ < offset_ - kMaxSeek)
    return false;

  // If we can manage the read request with int32 math, then prepare the
  // parameters.
  first_offset_ = static_cast<int>(read_position_ - offset_);
  last_offset_ = first_offset_ + read_size_;
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

//////////////////////////////////////////////////////////////////////////////
// BufferedDataSource, protected
BufferedDataSource::BufferedDataSource(
    MessageLoop* render_loop,
    webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory)
    : total_bytes_(kPositionNotSpecified),
      bridge_factory_(bridge_factory),
      loader_(NULL),
      read_callback_(NULL),
      read_position_(0),
      read_size_(0),
      read_buffer_(NULL),
      intermediate_read_buffer_(new uint8[kInitialReadBufferSize]),
      intermediate_read_buffer_size_(kInitialReadBufferSize),
      render_loop_(render_loop),
      initialize_callback_(NULL),
      stopped_(false) {
}

BufferedDataSource::~BufferedDataSource() {
}

// A factory method to create BufferedResourceLoader using the read parameters.
// This method can be overrided to inject mock BufferedResourceLoader object
// for testing purpose.
BufferedResourceLoader* BufferedDataSource::CreateLoader(
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
  DCHECK(callback);
  initialize_callback_.reset(callback);

  // Saves the url.
  url_ = GURL(url);

  if (!IsSchemeSupported(url_)) {
    host()->SetError(media::PIPELINE_ERROR_NETWORK);
    DoneInitialization();
    return;
  }

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
    stopped_ = true;
  }
  render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &BufferedDataSource::StopTask));
}

/////////////////////////////////////////////////////////////////////////////
// BufferedDataSource, media::DataSource implementation
void BufferedDataSource::Read(int64 position, size_t size,
                              uint8* data,
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

bool BufferedDataSource::IsSeekable() {
  return total_bytes_ != kPositionNotSpecified;
}

/////////////////////////////////////////////////////////////////////////////
// BufferedDataSource, render thread tasks
void BufferedDataSource::InitializeTask() {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(!loader_.get());

  // Kick starts the watch dog task that will handle connection timeout.
  // We run the watch dog 2 times faster the actual timeout so as to catch
  // the timeout more accurately.
  watch_dog_timer_.Start(
      GetTimeoutMilliseconds() / 2,
      this,
      &BufferedDataSource::WatchDogTask);

  // Creates a new resource loader with the full range.
  loader_.reset(CreateLoader(-1, -1));

  // And then start the resource request.
  loader_->Start(NewCallback(this,
                             &BufferedDataSource::InitializeStartCallback));
}

void BufferedDataSource::ReadTask(
     int64 position, int read_size, uint8* buffer,
     media::DataSource::ReadCallback* read_callback) {
  DCHECK(MessageLoop::current() == render_loop_);
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

  // Stop the watch dog.
  watch_dog_timer_.Stop();

  // We just need to stop the loader, so it stops activity.
  if (loader_.get()) {
    loader_->Stop();
    loader_.reset();
  }

  // Reset the parameters of the current read request.
  read_callback_.reset();
  read_position_ = 0;
  read_size_ = 0;
  read_buffer_ = 0;
  read_submitted_time_ = base::Time();
  read_attempts_ = 0;
}

void BufferedDataSource::SwapLoaderTask(BufferedResourceLoader* loader) {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(loader);

  loader_.reset(loader);
  loader_->Start(NewCallback(this,
                             &BufferedDataSource::PartialReadStartCallback));
}

void BufferedDataSource::WatchDogTask() {
  DCHECK(MessageLoop::current() == render_loop_);

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

  // Stops the current loader and swap in a new resource loader and
  // retry the request.
  loader_->Stop();
  SwapLoaderTask(CreateLoader(read_position_, -1));
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
  loader_->Read(read_position_, read_size_,
                intermediate_read_buffer_.get(),
                NewCallback(this, &BufferedDataSource::ReadCallback));
}

// Method to report the results of the current read request. Also reset all
// the read parameters.
void BufferedDataSource::DoneRead(int error) {
  DCHECK(MessageLoop::current() == render_loop_);
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

void BufferedDataSource::DoneInitialization() {
  DCHECK(initialize_callback_.get());
  lock_.AssertAcquired();

  initialize_callback_->Run();
  initialize_callback_.reset();
}

/////////////////////////////////////////////////////////////////////////////
// BufferedDataSource, callback methods.
// These methods are called on the render thread for the events reported by
// BufferedResourceLoader.
void BufferedDataSource::InitializeStartCallback(int error) {
  DCHECK(MessageLoop::current() == render_loop_);

  // We need to prevent calling to filter host and running the callback if
  // we have received the stop signal. We need to lock down the whole callback
  // method to prevent bad things from happening. The reason behind this is
  // that we cannot guarantee tasks on render thread have completely stopped
  // when we receive the Stop() method call. The only way to solve this is to
  // let tasks on render thread to run but make sure they don't call outside
  // this object when Stop() method is ever called. Locking this method is safe
  // because |lock_| is only acquired in tasks on render thread.
  AutoLock auto_lock(lock_);
  if (stopped_)
    return;

  DCHECK(loader_.get());

  if (error == net::OK) {
    total_bytes_ = loader_->content_length();
    // TODO(hclam): Figure out what to do when total bytes is not known.
    if (total_bytes_ >= 0) {
      host()->SetTotalBytes(total_bytes_);

      // This value governs the range that we can seek to.
      // TODO(hclam): Report the correct value of buffered bytes.
      host()->SetBufferedBytes(total_bytes_);
    }
  } else {
    // TODO(hclam): In case of failure, we can retry several times.
    // Also it might be bad to access host() here.
    host()->SetError(media::PIPELINE_ERROR_NETWORK);

    // Stops the loader, just to be safe.
    loader_->Stop();
  }

  DoneInitialization();
}

void BufferedDataSource::PartialReadStartCallback(int error) {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(loader_.get());

  if (error == net::OK) {
    // Once the range request has start successfully, we can proceed with
    // reading from it.
    ReadInternal();
  } else {
    // We need to prevent calling to filter host and running the callback if
    // we have received the stop signal. We need to lock down the whole callback
    // method to prevent bad things from happening. The reason behind this is
    // that we cannot guarantee tasks on render thread have completely stopped
    // when we receive the Stop() method call. So only way to solve this is to
    // let tasks on render thread to run but make sure they don't call outside
    // this object when Stop() method is ever called. Locking this method is
    // safe because |lock_| is only acquired in tasks on render thread.
    AutoLock auto_lock(lock_);
    if (stopped_)
      return;

    // TODO(hclam): It may be bad to access host() here.
    host()->SetError(media::PIPELINE_ERROR_NETWORK);

    // Kill the loader just to be safe.
    loader_->Stop();
  }
}

void BufferedDataSource::ReadCallback(int error) {
  DCHECK(MessageLoop::current() == render_loop_);

  // We need to prevent calling to filter host and running the callback if
  // we have received the stop signal. We need to lock down the whole callback
  // method to prevent bad things from happening. The reason behind this is
  // that we cannot guarantee tasks on render thread have completely stopped
  // when we receive the Stop() method call. So only way to solve this is to
  // let tasks on render thread to run but make sure they don't call outside
  // this object when Stop() method is ever called. Locking this method is safe
  // because |lock_| is only acquired in tasks on render thread.
  AutoLock auto_lock(lock_);
  if (stopped_)
    return;

  DCHECK(loader_.get());
  DCHECK(read_callback_.get());

  if (error >= 0) {
    // If a position error code is received, read was successful. So copy
    // from intermediate read buffer to the target read buffer.
    memcpy(read_buffer_, intermediate_read_buffer_.get(), error);

    DoneRead(error);
  } else if (error == net::ERR_CACHE_MISS) {
    // If the current loader cannot serve this read request, we need to create
    // a new one.
    // We have the following conditions:
    // 1. Read is beyond the content length of the file (if known).
    // 2. We have tried too many times (TODO here).
    if (read_position_ >= total_bytes_) {
      DoneRead(0);
      return;
    }

    // TODO(hclam): we need to count how many times it failed to prevent
    // excessive trials.

    // Stops the current resource loader.
    loader_->Stop();

    // Since this method is called from the current buffered resource loader,
    // we cannot delete it. So we need to post a task to swap in a new
    // resource loader and starts it.
    render_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this,
                          &BufferedDataSource::SwapLoaderTask,
                          CreateLoader(read_position_, -1)));
  } else {
    // The read has finished with error.
    DoneRead(error);

    // TODO(hclam): It may be bad to access host() here.
    host()->SetError(media::PIPELINE_ERROR_NETWORK);

    // Stops the laoder.
    loader_->Stop();
  }
}

}  // namespace webkit_glue
