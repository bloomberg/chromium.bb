// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/buffered_data_source.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop.h"
#include "media/base/media_log.h"
#include "net/base/net_errors.h"

using WebKit::WebFrame;

namespace {

// BufferedDataSource has an intermediate buffer, this value governs the initial
// size of that buffer. It is set to 32KB because this is a typical read size
// of FFmpeg.
const int kInitialReadBufferSize = 32768;

// Number of cache misses we allow for a single Read() before signaling an
// error.
const int kNumCacheMissRetries = 3;

}  // namespace

namespace webkit_media {

BufferedDataSource::BufferedDataSource(
    MessageLoop* render_loop,
    WebFrame* frame,
    media::MediaLog* media_log,
    const DownloadingCB& downloading_cb)
    : cors_mode_(BufferedResourceLoader::kUnspecified),
      total_bytes_(kPositionNotSpecified),
      assume_fully_buffered_(false),
      streaming_(false),
      frame_(frame),
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
// This method can be overridden to inject mock BufferedResourceLoader object
// for testing purpose.
BufferedResourceLoader* BufferedDataSource::CreateResourceLoader(
    int64 first_byte_position, int64 last_byte_position) {
  DCHECK(MessageLoop::current() == render_loop_);

  BufferedResourceLoader::DeferStrategy strategy = preload_ == METADATA ?
      BufferedResourceLoader::kReadThenDefer :
      BufferedResourceLoader::kCapacityDefer;

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
    const InitializeCB& init_cb) {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(!init_cb.is_null());
  DCHECK(!loader_.get());
  url_ = url;
  cors_mode_ = cors_mode;

  init_cb_ = init_cb;

  if (url_.SchemeIs(kHttpScheme) || url_.SchemeIs(kHttpsScheme)) {
    // Do an unbounded range request starting at the beginning.  If the server
    // responds with 200 instead of 206 we'll fall back into a streaming mode.
    loader_.reset(CreateResourceLoader(0, kPositionNotSpecified));
  } else {
    // For all other protocols, assume they support range request. We fetch
    // the full range of the resource to obtain the instance size because
    // we won't be served HTTP headers.
    loader_.reset(CreateResourceLoader(kPositionNotSpecified,
                                       kPositionNotSpecified));
    assume_fully_buffered_ = true;
  }

  loader_->Start(
      base::Bind(&BufferedDataSource::StartCallback, this),
      base::Bind(&BufferedDataSource::LoadingStateChangedCallback, this),
      base::Bind(&BufferedDataSource::ProgressCallback, this),
      frame_);
}

void BufferedDataSource::SetPreload(Preload preload) {
  DCHECK(MessageLoop::current() == render_loop_);
  preload_ = preload;
}

bool BufferedDataSource::HasSingleOrigin() {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(init_cb_.is_null() && loader_.get())
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
    init_cb_.Reset();
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
    loader_->UpdateDeferStrategy(BufferedResourceLoader::kCapacityDefer);
  } else if (media_has_played_ && playback_rate == 0) {
    // If the playback has started (at which point the preload value is ignored)
    // and we're paused, then try to load as much as possible (the loader will
    // fall back to kCapacityDefer if it knows the current response won't be
    // useful from the cache in the future).
    loader_->UpdateDeferStrategy(BufferedResourceLoader::kNeverDefer);
  } else {
    // If media is currently playing or the page indicated preload=auto,
    // use threshold strategy to enable/disable deferring when the buffer
    // is full/depleted.
    loader_->UpdateDeferStrategy(BufferedResourceLoader::kCapacityDefer);
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

/////////////////////////////////////////////////////////////////////////////
// BufferedResourceLoader callback methods.
void BufferedDataSource::StartCallback(
    BufferedResourceLoader::Status status) {
  DCHECK(MessageLoop::current() == render_loop_);
  DCHECK(loader_.get());

  bool init_cb_is_null = false;
  {
    base::AutoLock auto_lock(lock_);
    init_cb_is_null = init_cb_.is_null();
  }
  if (init_cb_is_null) {
    loader_->Stop();
    return;
  }

  // All responses must be successful. Resources that are assumed to be fully
  // buffered must have a known content length.
  bool success = status == BufferedResourceLoader::kOk &&
      (!assume_fully_buffered_ ||
       loader_->instance_size() != kPositionNotSpecified);

  if (success) {
    total_bytes_ = loader_->instance_size();
    streaming_ = !assume_fully_buffered_ &&
        (total_bytes_ == kPositionNotSpecified || !loader_->range_supported());
  } else {
    loader_->Stop();
  }

  // TODO(scherkus): we shouldn't have to lock to signal host(), see
  // http://crbug.com/113712 for details.
  scoped_refptr<BufferedDataSource> destruction_guard(this);
  {
    base::AutoLock auto_lock(lock_);
    if (stop_signal_received_)
      return;

    if (success)
      UpdateHostState_Locked();

    base::ResetAndReturn(&init_cb_).Run(success);
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

  // TODO(scherkus): we shouldn't have to lock to signal host(), see
  // http://crbug.com/113712 for details.
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

      // Recreate a loader starting from where we last left off until the
      // end of the resource.
      loader_.reset(CreateResourceLoader(
          last_read_start_, kPositionNotSpecified));
      loader_->Start(
          base::Bind(&BufferedDataSource::PartialReadStartCallback, this),
          base::Bind(&BufferedDataSource::LoadingStateChangedCallback, this),
          base::Bind(&BufferedDataSource::ProgressCallback, this),
          frame_);
      return;
    }

    // Fall through to signal a read error.
    bytes_read = kReadError;
  }

  // TODO(scherkus): we shouldn't have to lock to signal host(), see
  // http://crbug.com/113712 for details.
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

void BufferedDataSource::LoadingStateChangedCallback(
    BufferedResourceLoader::LoadingState state) {
  DCHECK(MessageLoop::current() == render_loop_);

  if (assume_fully_buffered_)
    return;

  bool is_downloading_data;
  switch (state) {
    case BufferedResourceLoader::kLoading:
      is_downloading_data = true;
      break;
    case BufferedResourceLoader::kLoadingDeferred:
    case BufferedResourceLoader::kLoadingFinished:
      is_downloading_data = false;
      break;

    // TODO(scherkus): we don't signal network activity changes when loads
    // fail to preserve existing behaviour when deferring is toggled, however
    // we should consider changing DownloadingCB to also propagate loading
    // state. For example there isn't any signal today to notify the client that
    // loading has failed (we only get errors on subsequent reads).
    case BufferedResourceLoader::kLoadingFailed:
      return;
  }

  downloading_cb_.Run(is_downloading_data);
}

void BufferedDataSource::ProgressCallback(int64 position) {
  DCHECK(MessageLoop::current() == render_loop_);

  if (assume_fully_buffered_)
    return;

  // TODO(scherkus): we shouldn't have to lock to signal host(), see
  // http://crbug.com/113712 for details.
  base::AutoLock auto_lock(lock_);
  if (stop_signal_received_)
    return;

  if (position > last_read_start_)
    ReportOrQueueBufferedBytes(last_read_start_, position);
}

void BufferedDataSource::ReportOrQueueBufferedBytes(int64 start, int64 end) {
  if (host())
    host()->AddBufferedByteRange(start, end);
  else
    queued_buffered_byte_ranges_.Add(start, end);
}

void BufferedDataSource::UpdateHostState_Locked() {
  lock_.AssertAcquired();

  if (!host())
    return;

  for (size_t i = 0; i < queued_buffered_byte_ranges_.size(); ++i) {
    host()->AddBufferedByteRange(queued_buffered_byte_ranges_.start(i),
                                 queued_buffered_byte_ranges_.end(i));
  }
  queued_buffered_byte_ranges_.clear();

  if (total_bytes_ == kPositionNotSpecified)
    return;

  host()->SetTotalBytes(total_bytes_);

  if (assume_fully_buffered_)
    host()->AddBufferedByteRange(0, total_bytes_);
}

}  // namespace webkit_media
