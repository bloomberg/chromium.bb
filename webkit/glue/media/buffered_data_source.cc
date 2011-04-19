// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/media/buffered_data_source.h"

#include "media/base/filter_host.h"
#include "net/base/net_errors.h"
#include "webkit/glue/media/web_data_source_factory.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFrame;

namespace webkit_glue {

// Defines how long we should wait for more data before we declare a connection
// timeout and start a new request.
// TODO(hclam): Set it to 5s, calibrate this value later.
static const int kTimeoutMilliseconds = 5000;

// Defines how many times we should try to read from a buffered resource loader
// before we declare a read error. After each failure of read from a buffered
// resource loader, a new one is created to be read.
static const int kReadTrials = 3;

// BufferedDataSource has an intermediate buffer, this value governs the initial
// size of that buffer. It is set to 32KB because this is a typical read size
// of FFmpeg.
static const int kInitialReadBufferSize = 32768;

static WebDataSource* NewBufferedDataSource(MessageLoop* render_loop,
                                            WebKit::WebFrame* frame) {
  return new BufferedDataSource(render_loop, frame);
}

// static
media::DataSourceFactory* BufferedDataSource::CreateFactory(
    MessageLoop* render_loop,
    WebKit::WebFrame* frame,
    WebDataSourceBuildObserverHack* build_observer) {
  return new WebDataSourceFactory(render_loop, frame, &NewBufferedDataSource,
                                  build_observer);
}

BufferedDataSource::BufferedDataSource(
    MessageLoop* render_loop,
    WebFrame* frame)
    : total_bytes_(kPositionNotSpecified),
      buffered_bytes_(0),
      loaded_(false),
      streaming_(false),
      frame_(frame),
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
      media_has_played_(false),
      preload_(media::METADATA),
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

void BufferedDataSource::set_host(media::FilterHost* host) {
  DataSource::set_host(host);

  if (loader_.get())
    UpdateHostState();
}

void BufferedDataSource::Initialize(const std::string& url,
                                    media::PipelineStatusCallback* callback) {
  // Saves the url.
  url_ = GURL(url);

  // This data source doesn't support data:// protocol so reject it.
  if (url_.SchemeIs(kDataScheme)) {
    callback->Run(media::DATASOURCE_ERROR_URL_NOT_SUPPORTED);
    delete callback;
    return;
  } else if (!IsProtocolSupportedForMedia(url_)) {
    callback->Run(media::PIPELINE_ERROR_NETWORK);
    delete callback;
    return;
  }

  DCHECK(callback);
  initialize_callback_.reset(callback);

  media_format_.SetAsString(media::MediaFormat::kURL, url);

  // Post a task to complete the initialization task.
  render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &BufferedDataSource::InitializeTask));
}

void BufferedDataSource::CancelInitialize() {
  base::AutoLock auto_lock(lock_);
  DCHECK(initialize_callback_.get());

  initialize_callback_.reset();

  render_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &BufferedDataSource::CleanupTask));
}

/////////////////////////////////////////////////////////////////////////////
// media::Filter implementation.
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

void BufferedDataSource::SetPreload(media::Preload preload) {
  render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &BufferedDataSource::SetPreloadTask,
                        preload));
}

/////////////////////////////////////////////////////////////////////////////
// media::DataSource implementation.
void BufferedDataSource::Read(int64 position, size_t size, uint8* data,
                              media::DataSource::ReadCallback* read_callback) {
  DCHECK(read_callback);

  {
    base::AutoLock auto_lock(lock_);
    DCHECK(!read_callback_.get());

    if (stop_signal_received_ || stopped_on_render_loop_) {
      read_callback->RunWithParams(
          Tuple1<size_t>(static_cast<size_t>(media::DataSource::kReadError)));
      delete read_callback;
      return;
    }

    read_callback_.reset(read_callback);
  }

  render_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &BufferedDataSource::ReadTask,
                        position, static_cast<int>(size), data));
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
  return loader_.get() ? loader_->HasSingleOrigin() : true;
}

void BufferedDataSource::Abort() {
  DCHECK(MessageLoop::current() == render_loop_);

  CleanupTask();
  frame_ = NULL;
}

/////////////////////////////////////////////////////////////////////////////
// Render thread tasks.
void BufferedDataSource::InitializeTask() {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(!loader_.get());
  if (stopped_on_render_loop_ || !initialize_callback_.get())
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
    int64 position,
    int read_size,
    uint8* buffer) {
  DCHECK(MessageLoop::current() == render_loop_);
  {
    base::AutoLock auto_lock(lock_);
    if (stopped_on_render_loop_)
      return;

    DCHECK(read_callback_.get());
  }

  // Saves the read parameters.
  read_position_ = position;
  read_size_ = read_size;
  read_buffer_ = buffer;
  read_submitted_time_ = base::Time::Now();
  read_attempts_ = 0;

  // Call to read internal to perform the actual read.
  ReadInternal();
}

void BufferedDataSource::CleanupTask() {
  DCHECK(MessageLoop::current() == render_loop_);

  {
    base::AutoLock auto_lock(lock_);
    if (stopped_on_render_loop_)
      return;

    // Signal that stop task has finished execution.
    // NOTE: it's vital that this be set under lock, as that's how Read() tests
    // before registering a new |read_callback_| (which is cleared below).
    stopped_on_render_loop_ = true;

    if (read_callback_.get())
      DoneRead_Locked(net::ERR_FAILED);
  }

  // Stop the watch dog.
  watch_dog_timer_.Stop();

  // We just need to stop the loader, so it stops activity.
  if (loader_.get())
    loader_->Stop();

  // Reset the parameters of the current read request.
  read_position_ = 0;
  read_size_ = 0;
  read_buffer_ = 0;
  read_submitted_time_ = base::Time();
  read_attempts_ = 0;
}

void BufferedDataSource::RestartLoadingTask() {
  DCHECK(MessageLoop::current() == render_loop_);
  if (stopped_on_render_loop_)
    return;

  {
    // If there's no outstanding read then return early.
    base::AutoLock auto_lock(lock_);
    if (!read_callback_.get())
      return;
  }

  loader_ = CreateResourceLoader(read_position_, kPositionNotSpecified);
  BufferedResourceLoader::DeferStrategy strategy = ChooseDeferStrategy();
  loader_->UpdateDeferStrategy(strategy);
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
  {
    base::AutoLock auto_lock(lock_);
    if (!read_callback_.get())
      return;
  }

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
  BufferedResourceLoader::DeferStrategy strategy = ChooseDeferStrategy();
  loader_->UpdateDeferStrategy(strategy);
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

  if (!media_has_played_ && previously_paused && !media_is_paused_)
    media_has_played_ = true;

  BufferedResourceLoader::DeferStrategy strategy = ChooseDeferStrategy();
  loader_->UpdateDeferStrategy(strategy);
}

void BufferedDataSource::SetPreloadTask(media::Preload preload) {
  DCHECK(MessageLoop::current() == render_loop_);
  preload_ = preload;
}

BufferedResourceLoader::DeferStrategy
BufferedDataSource::ChooseDeferStrategy() {
  // If the user indicates preload=metadata, then just load exactly
  // what is needed for starting the pipeline and prerolling frames.
  if (preload_ == media::METADATA && !media_has_played_)
    return BufferedResourceLoader::kReadThenDefer;

  // In general, we want to try to buffer the entire video when the video
  // is paused. But we don't want to do this if the video hasn't played yet
  // and preload!=auto.
  if (media_is_paused_ &&
      (preload_ == media::AUTO || media_has_played_)) {
    return BufferedResourceLoader::kNeverDefer;
  }

  // When the video is playing, regardless of preload state, we buffer up
  // to a hard limit and enable/disable deferring when the buffer is
  // depleted/full.
  return BufferedResourceLoader::kThresholdDefer;
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

void BufferedDataSource::DoneInitialization_Locked(
    media::PipelineStatus status) {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(initialize_callback_.get());
  lock_.AssertAcquired();

  scoped_ptr<media::PipelineStatusCallback> initialize_callback(
      initialize_callback_.release());
  initialize_callback->Run(status);
}

/////////////////////////////////////////////////////////////////////////////
// BufferedResourceLoader callback methods.
void BufferedDataSource::HttpInitialStartCallback(int error) {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(loader_.get());

  int64 instance_size = loader_->instance_size();
  bool success = error == net::OK;

  if (!initialize_callback_.get()) {
    loader_->Stop();
    return;
  }

  if (success) {
    // TODO(hclam): Needs more thinking about supporting servers without range
    // request or their partial response is not complete.
    total_bytes_ = instance_size;
    loaded_ = false;
    streaming_ = (instance_size == kPositionNotSpecified) ||
        !loader_->range_supported();
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

  // Reference to prevent destruction while inside the |initialize_callback_|
  // call. This is a temporary fix to prevent crashes caused by holding the
  // lock and running the destructor.
  // TODO: Review locking in this class and figure out a way to run the callback
  //       w/o the lock.
  scoped_refptr<BufferedDataSource> destruction_guard(this);
  {
    // We need to prevent calling to filter host and running the callback if
    // we have received the stop signal. We need to lock down the whole callback
    // method to prevent bad things from happening. The reason behind this is
    // that we cannot guarantee tasks on render thread have completely stopped
    // when we receive the Stop() method call. The only way to solve this is to
    // let tasks on render thread to run but make sure they don't call outside
    // this object when Stop() method is ever called. Locking this method is
    // safe because |lock_| is only acquired in tasks on render thread.
    base::AutoLock auto_lock(lock_);
    if (stop_signal_received_)
      return;

    if (!success) {
      DoneInitialization_Locked(media::PIPELINE_ERROR_NETWORK);
      return;
    }

    UpdateHostState();
    DoneInitialization_Locked(media::PIPELINE_OK);
  }
}

void BufferedDataSource::NonHttpInitialStartCallback(int error) {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(loader_.get());

  if (!initialize_callback_.get()) {
    loader_->Stop();
    return;
  }

  int64 instance_size = loader_->instance_size();
  bool success = error == net::OK && instance_size != kPositionNotSpecified;

  if (success) {
    total_bytes_ = instance_size;
    buffered_bytes_ = total_bytes_;
    loaded_ = true;
  } else {
    loader_->Stop();
  }

  // Reference to prevent destruction while inside the |initialize_callback_|
  // call. This is a temporary fix to prevent crashes caused by holding the
  // lock and running the destructor.
  // TODO: Review locking in this class and figure out a way to run the callback
  //       w/o the lock.
  scoped_refptr<BufferedDataSource> destruction_guard(this);
  {
    // We need to prevent calling to filter host and running the callback if
    // we have received the stop signal. We need to lock down the whole callback
    // method to prevent bad things from happening. The reason behind this is
    // that we cannot guarantee tasks on render thread have completely stopped
    // when we receive the Stop() method call. The only way to solve this is to
    // let tasks on render thread to run but make sure they don't call outside
    // this object when Stop() method is ever called. Locking this method is
    // safe because |lock_| is only acquired in tasks on render thread.
    base::AutoLock auto_lock(lock_);
    if (stop_signal_received_ || !initialize_callback_.get())
      return;

    if (!success) {
      DoneInitialization_Locked(media::PIPELINE_ERROR_NETWORK);
      return;
    }

    UpdateHostState();
    DoneInitialization_Locked(media::PIPELINE_OK);
  }
}

void BufferedDataSource::PartialReadStartCallback(int error) {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(loader_.get());

  if (error == net::OK) {
    // Once the request has started successfully, we can proceed with
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
  } else if (error == 0 && total_bytes_ == kPositionNotSpecified) {
    // We've reached the end of the file and we didn't know the total size
    // before. Update the total size so Read()s past the end of the file will
    // fail like they would if we had known the file size at the beginning.
    total_bytes_ = loader_->instance_size();

    if (host() && total_bytes_ != kPositionNotSpecified)
      host()->SetTotalBytes(total_bytes_);
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
    if (host())
      host()->SetNetworkActivity(network_activity);
  }

  buffered_bytes_ = buffered_position + 1;
  if (host())
    host()->SetBufferedBytes(buffered_bytes_);
}

void BufferedDataSource::UpdateHostState() {
  media::FilterHost* filter_host = host();
  if (!filter_host)
    return;

  filter_host->SetLoaded(loaded_);

  if (streaming_) {
    filter_host->SetStreaming(true);
  } else {
    filter_host->SetTotalBytes(total_bytes_);
    filter_host->SetBufferedBytes(buffered_bytes_);
  }
}

}  // namespace webkit_glue
