// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/media/buffered_data_source.h"

#include "media/base/filter_host.h"
#include "net/base/net_errors.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFrame;

namespace {

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

} // namespace

namespace webkit_glue {

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
// media::Filter implementation.
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
  return IsProtocolSupportedForMedia(gurl) && !gurl.SchemeIs(kDataScheme);
}

void BufferedDataSource::Stop(media::FilterCallback* callback) {
  {
    base::AutoLock auto_lock(lock_);
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
// media::DataSource implementation.
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
      base::AutoLock auto_lock(lock_);
      DoneRead_Locked(net::ERR_FAILED);
  }

  CleanupTask();
  frame_ = NULL;
}

/////////////////////////////////////////////////////////////////////////////
// Render thread tasks.
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

  if (url_.SchemeIs(kHttpScheme) || url_.SchemeIs(kHttpsScheme)) {
    // Do an unbounded range request starting at the beginning.  If the server
    // responds with 200 instead of 206 we'll fall back into a streaming mode.
    loader_ = CreateResourceLoader(0, kPositionNotSpecified);
    loader_->Start(
        NewCallback(this, &BufferedDataSource::HttpInitialStartCallback),
        NewCallback(this, &BufferedDataSource::NetworkEventCallback),
        frame_);
  } else {
    // For all other protocols, assume they support range request. We fetch
    // the full range of the resource to obtain the instance size because
    // we won't be served HTTP headers.
    loader_ = CreateResourceLoader(kPositionNotSpecified,
                                   kPositionNotSpecified);
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

  loader_ = CreateResourceLoader(read_position_, kPositionNotSpecified);
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
  loader_ = CreateResourceLoader(read_position_, kPositionNotSpecified);
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
// BufferedResourceLoader callback methods.
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
    loader_ = CreateResourceLoader(kPositionNotSpecified,
                                   kPositionNotSpecified);
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
  base::AutoLock auto_lock(lock_);
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
  base::AutoLock auto_lock(lock_);
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
  base::AutoLock auto_lock(lock_);
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
  base::AutoLock auto_lock(lock_);
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
  int64 buffered_position = loader_->GetBufferedPosition();

  // If we get an unspecified value, return immediately.
  if (buffered_position == kPositionNotSpecified)
    return;

  // We need to prevent calling to filter host and running the callback if
  // we have received the stop signal. We need to lock down the whole callback
  // method to prevent bad things from happening. The reason behind this is
  // that we cannot guarantee tasks on render thread have completely stopped
  // when we receive the Stop() method call. So only way to solve this is to
  // let tasks on render thread to run but make sure they don't call outside
  // this object when Stop() method is ever called. Locking this method is safe
  // because |lock_| is only acquired in tasks on render thread.
  base::AutoLock auto_lock(lock_);
  if (stop_signal_received_)
    return;

  if (network_activity != network_activity_) {
    network_activity_ = network_activity;
    host()->SetNetworkActivity(network_activity);
  }
  host()->SetBufferedBytes(buffered_position + 1);
}

}  // namespace webkit_glue
