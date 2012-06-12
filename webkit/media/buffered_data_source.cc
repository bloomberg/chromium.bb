// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/buffered_data_source.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "media/base/media_log.h"
#include "net/base/net_errors.h"

using WebKit::WebFrame;

namespace webkit_media {

// BufferedDataSource has an intermediate buffer, this value governs the initial
// size of that buffer. It is set to 32KB because this is a typical read size
// of FFmpeg.
static const int kInitialReadBufferSize = 32768;

// Number of cache misses we allow for a single Read() before signalling an
// error.
static const int kNumCacheMissRetries = 3;

BufferedDataSource::BufferedDataSource(
    MessageLoop* render_loop,
    WebFrame* frame,
    media::MediaLog* media_log,
    const DownloadingCB& downloading_cb)
    : total_bytes_(kPositionNotSpecified),
      buffered_bytes_(0),
      streaming_(false),
      frame_(frame),
      loader_(NULL),
      read_size_(0),
      read_buffer_(NULL),
      last_read_start_(0),
      intermediate_read_buffer_(new uint8[kInitialReadBufferSize]),
      intermediate_read_buffer_size_(kInitialReadBufferSize),
      render_loop_(render_loop),
      stop_signal_received_(false),
      stopped_on_render_loop_(false),
      media_has_played_(false),
      preload_(AUTO),
      cache_miss_retries_left_(kNumCacheMissRetries),
      bitrate_(0),
      playback_rate_(0.0),
      media_log_(media_log),
      downloading_cb_(downloading_cb) {
  DCHECK(!downloading_cb_.is_null());
}

BufferedDataSource::~BufferedDataSource() {}

// A factory method to create BufferedResourceLoader using the read parameters.
// This method can be overrided to inject mock BufferedResourceLoader object
// for testing purpose.
BufferedResourceLoader* BufferedDataSource::CreateResourceLoader(
    int64 first_byte_position, int64 last_byte_position) {
  DCHECK(MessageLoop::current() == render_loop_);

  BufferedResourceLoader::DeferStrategy strategy = preload_ == METADATA ?
      BufferedResourceLoader::kReadThenDefer :
      BufferedResourceLoader::kThresholdDefer;

  return new BufferedResourceLoader(url_,
                                    cors_mode_,
                                    first_byte_position,
                                    last_byte_position,
                                    strategy,
                                    bitrate_,
                                    playback_rate_,
                                    media_log_);
}

void BufferedDataSource::set_host(media::DataSourceHost* host) {
  DataSource::set_host(host);

  if (loader_.get()) {
    base::AutoLock auto_lock(lock_);
    UpdateHostState_Locked();
  }
}

void BufferedDataSource::Initialize(
    const GURL& url,
    BufferedResourceLoader::CORSMode cors_mode,
    const media::PipelineStatusCB& initialize_cb) {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(!initialize_cb.is_null());
  DCHECK(!loader_.get());
  url_ = url;
  cors_mode_ = cors_mode;

  initialize_cb_ = initialize_cb;

  if (url_.SchemeIs(kHttpScheme) || url_.SchemeIs(kHttpsScheme)) {
    // Do an unbounded range request starting at the beginning.  If the server
    // responds with 200 instead of 206 we'll fall back into a streaming mode.
    loader_.reset(CreateResourceLoader(0, kPositionNotSpecified));
    loader_->Start(
        base::Bind(&BufferedDataSource::HttpInitialStartCallback, this),
        base::Bind(&BufferedDataSource::NetworkEventCallback, this),
        frame_);
    return;
  }

  // For all other protocols, assume they support range request. We fetch
  // the full range of the resource to obtain the instance size because
  // we won't be served HTTP headers.
  loader_.reset(CreateResourceLoader(kPositionNotSpecified,
                                     kPositionNotSpecified));
  loader_->Start(
      base::Bind(&BufferedDataSource::NonHttpInitialStartCallback, this),
      base::Bind(&BufferedDataSource::NetworkEventCallback, this),
      frame_);
}

void BufferedDataSource::SetPreload(Preload preload) {
  DCHECK(MessageLoop::current() == render_loop_);
  preload_ = preload;
}

bool BufferedDataSource::HasSingleOrigin() {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(initialize_cb_.is_null() && loader_.get())
      << "Initialize() must complete before calling HasSingleOrigin()";
  return loader_->HasSingleOrigin();
}

bool BufferedDataSource::DidPassCORSAccessCheck() const {
  return loader_.get() && loader_->DidPassCORSAccessCheck();
}

void BufferedDataSource::Abort() {
  DCHECK(MessageLoop::current() == render_loop_);

  CleanupTask();
  frame_ = NULL;
}

/////////////////////////////////////////////////////////////////////////////
// media::Filter implementation.
void BufferedDataSource::Stop(const base::Closure& closure) {
  {
    base::AutoLock auto_lock(lock_);
    stop_signal_received_ = true;
  }
  if (!closure.is_null())
    closure.Run();

  render_loop_->PostTask(FROM_HERE,
      base::Bind(&BufferedDataSource::CleanupTask, this));
}

void BufferedDataSource::SetPlaybackRate(float playback_rate) {
  render_loop_->PostTask(FROM_HERE, base::Bind(
      &BufferedDataSource::SetPlaybackRateTask, this, playback_rate));
}

void BufferedDataSource::SetBitrate(int bitrate) {
  render_loop_->PostTask(FROM_HERE, base::Bind(
      &BufferedDataSource::SetBitrateTask, this, bitrate));
}

/////////////////////////////////////////////////////////////////////////////
// media::DataSource implementation.
void BufferedDataSource::Read(
    int64 position, int size, uint8* data,
    const media::DataSource::ReadCB& read_cb) {
  DVLOG(1) << "Read: " << position << " offset, " << size << " bytes";
  DCHECK(!read_cb.is_null());

  {
    base::AutoLock auto_lock(lock_);
    DCHECK(read_cb_.is_null());

    if (stop_signal_received_ || stopped_on_render_loop_) {
      read_cb.Run(kReadError);
      return;
    }

    read_cb_ = read_cb;
  }

  render_loop_->PostTask(FROM_HERE, base::Bind(
      &BufferedDataSource::ReadTask, this, position, size, data));
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
// Render thread tasks.
void BufferedDataSource::ReadTask(
    int64 position,
    int read_size,
    uint8* buffer) {
  DCHECK(MessageLoop::current() == render_loop_);
  {
    base::AutoLock auto_lock(lock_);
    if (stopped_on_render_loop_)
      return;

    DCHECK(!read_cb_.is_null());
  }

  // Saves the read parameters.
  last_read_start_ = position;
  read_size_ = read_size;
  read_buffer_ = buffer;
  cache_miss_retries_left_ = kNumCacheMissRetries;

  // Call to read internal to perform the actual read.
  ReadInternal();
}

void BufferedDataSource::CleanupTask() {
  DCHECK(MessageLoop::current() == render_loop_);

  {
    base::AutoLock auto_lock(lock_);
    initialize_cb_.Reset();
    if (stopped_on_render_loop_)
      return;

    // Signal that stop task has finished execution.
    // NOTE: it's vital that this be set under lock, as that's how Read() tests
    // before registering a new |read_cb_| (which is cleared below).
    stopped_on_render_loop_ = true;

    if (!read_cb_.is_null())
      DoneRead_Locked(kReadError);
  }

  // We just need to stop the loader, so it stops activity.
  if (loader_.get())
    loader_->Stop();

  // Reset the parameters of the current read request.
  read_size_ = 0;
  read_buffer_ = 0;
}

void BufferedDataSource::RestartLoadingTask() {
  DCHECK(MessageLoop::current() == render_loop_);
  if (stopped_on_render_loop_)
    return;

  {
    // If there's no outstanding read then return early.
    base::AutoLock auto_lock(lock_);
    if (read_cb_.is_null())
      return;
  }

  loader_.reset(
      CreateResourceLoader(last_read_start_, kPositionNotSpecified));
  loader_->Start(
      base::Bind(&BufferedDataSource::PartialReadStartCallback, this),
      base::Bind(&BufferedDataSource::NetworkEventCallback, this),
      frame_);
}

void BufferedDataSource::SetPlaybackRateTask(float playback_rate) {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(loader_.get());

  if (playback_rate != 0)
    media_has_played_ = true;

  playback_rate_ = playback_rate;
  loader_->SetPlaybackRate(playback_rate);

  if (!loader_->range_supported()) {
    // 200 responses end up not being reused to satisfy future range requests,
    // and we don't want to get too far ahead of the read-head (and thus require
    // a restart), so keep to the thresholds.
    loader_->UpdateDeferStrategy(BufferedResourceLoader::kThresholdDefer);
  } else if (media_has_played_ && playback_rate == 0) {
    // If the playback has started (at which point the preload value is ignored)
    // and we're paused, then try to load as much as possible (the loader will
    // fall back to kThresholdDefer if it knows the current response won't be
    // useful from the cache in the future).
    loader_->UpdateDeferStrategy(BufferedResourceLoader::kNeverDefer);
  } else {
    // If media is currently playing or the page indicated preload=auto,
    // use threshold strategy to enable/disable deferring when the buffer
    // is full/depleted.
    loader_->UpdateDeferStrategy(BufferedResourceLoader::kThresholdDefer);
  }
}

void BufferedDataSource::SetBitrateTask(int bitrate) {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(loader_.get());

  bitrate_ = bitrate;
  loader_->SetBitrate(bitrate);
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
  loader_->Read(
      last_read_start_, read_size_, intermediate_read_buffer_.get(),
      base::Bind(&BufferedDataSource::ReadCallback, this));
}

void BufferedDataSource::DoneRead_Locked(int bytes_read) {
  DVLOG(1) << "DoneRead: " << bytes_read << " bytes";
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(!read_cb_.is_null());
  DCHECK(bytes_read >= 0 || bytes_read == kReadError);
  lock_.AssertAcquired();

  read_cb_.Run(bytes_read);
  read_cb_.Reset();
  read_size_ = 0;
  read_buffer_ = 0;
}

void BufferedDataSource::DoneInitialization_Locked(
    media::PipelineStatus status) {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(!initialize_cb_.is_null());
  lock_.AssertAcquired();

  initialize_cb_.Run(status);
  initialize_cb_.Reset();
}

/////////////////////////////////////////////////////////////////////////////
// BufferedResourceLoader callback methods.
void BufferedDataSource::HttpInitialStartCallback(
    BufferedResourceLoader::Status status) {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(loader_.get());

  bool initialize_cb_is_null = false;
  {
    base::AutoLock auto_lock(lock_);
    initialize_cb_is_null = initialize_cb_.is_null();
  }
  if (initialize_cb_is_null) {
    loader_->Stop();
    return;
  }

  bool success = status == BufferedResourceLoader::kOk;
  if (success) {
    // TODO(hclam): Needs more thinking about supporting servers without range
    // request or their partial response is not complete.
    total_bytes_ = loader_->instance_size();
    streaming_ = (total_bytes_ == kPositionNotSpecified) ||
        !loader_->range_supported();
  } else {
    // TODO(hclam): In case of failure, we can retry several times.
    loader_->Stop();
  }

  // Reference to prevent destruction while inside the |initialize_cb_|
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

    UpdateHostState_Locked();
    DoneInitialization_Locked(media::PIPELINE_OK);
  }
}

void BufferedDataSource::NonHttpInitialStartCallback(
    BufferedResourceLoader::Status status) {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(loader_.get());

  bool initialize_cb_is_null = false;
  {
    base::AutoLock auto_lock(lock_);
    initialize_cb_is_null = initialize_cb_.is_null();
  }
  if (initialize_cb_is_null) {
    loader_->Stop();
    return;
  }

  int64 instance_size = loader_->instance_size();
  bool success = status == BufferedResourceLoader::kOk &&
     instance_size != kPositionNotSpecified;

  if (success) {
    total_bytes_ = instance_size;
    buffered_bytes_ = total_bytes_;
  } else {
    loader_->Stop();
  }

  // Reference to prevent destruction while inside the |initialize_cb_|
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
    if (stop_signal_received_ || initialize_cb_.is_null())
      return;

    if (!success) {
      DoneInitialization_Locked(media::PIPELINE_ERROR_NETWORK);
      return;
    }

    UpdateHostState_Locked();
    DoneInitialization_Locked(media::PIPELINE_OK);
  }
}

void BufferedDataSource::PartialReadStartCallback(
    BufferedResourceLoader::Status status) {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(loader_.get());

  if (status == BufferedResourceLoader::kOk) {
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
  DoneRead_Locked(kReadError);
}

void BufferedDataSource::ReadCallback(
    BufferedResourceLoader::Status status,
    int bytes_read) {
  DCHECK(MessageLoop::current() == render_loop_);

  if (status != BufferedResourceLoader::kOk) {
    // Stop the resource load if it failed.
    loader_->Stop();

    if (status == BufferedResourceLoader::kCacheMiss &&
        cache_miss_retries_left_ > 0) {
      cache_miss_retries_left_--;
      render_loop_->PostTask(FROM_HERE,
          base::Bind(&BufferedDataSource::RestartLoadingTask, this));
      return;
    }

    // Fall through to signal a read error.
    bytes_read = kReadError;
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

  if (bytes_read > 0) {
    memcpy(read_buffer_, intermediate_read_buffer_.get(), bytes_read);
  } else if (bytes_read == 0 && total_bytes_ == kPositionNotSpecified) {
    // We've reached the end of the file and we didn't know the total size
    // before. Update the total size so Read()s past the end of the file will
    // fail like they would if we had known the file size at the beginning.
    total_bytes_ = loader_->instance_size();

    if (host() && total_bytes_ != kPositionNotSpecified) {
      host()->SetTotalBytes(total_bytes_);
      host()->AddBufferedByteRange(loader_->first_byte_position(),
                                   total_bytes_);
    }
  }
  DoneRead_Locked(bytes_read);
}

void BufferedDataSource::NetworkEventCallback() {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(loader_.get());

  // In case of non-HTTP request we don't need to report network events,
  // so return immediately.
  if (!url_.SchemeIs(kHttpScheme) && !url_.SchemeIs(kHttpsScheme))
    return;

  downloading_cb_.Run(loader_->is_downloading_data());

  int64 current_buffered_position = loader_->GetBufferedPosition();

  // If we get an unspecified value, return immediately.
  if (current_buffered_position == kPositionNotSpecified)
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

  int64 start = loader_->first_byte_position();
  if (host() && current_buffered_position > start)
    host()->AddBufferedByteRange(start, current_buffered_position);
}

void BufferedDataSource::UpdateHostState_Locked() {
  // Called from various threads, under lock.
  lock_.AssertAcquired();

  if (!host())
    return;

  if (total_bytes_ != kPositionNotSpecified)
    host()->SetTotalBytes(total_bytes_);
  int64 start = loader_->first_byte_position();
  if (buffered_bytes_ > start)
    host()->AddBufferedByteRange(start, buffered_bytes_);
}

}  // namespace webkit_media
