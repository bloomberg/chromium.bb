// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/buffered_data_source.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop_proxy.h"
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

class BufferedDataSource::ReadOperation {
 public:
  ReadOperation(int64 position, int size, uint8* data,
                const media::DataSource::ReadCB& callback);
  ~ReadOperation();

  // Runs |callback_| with the given |result|, deleting the operation
  // afterwards.
  static void Run(scoped_ptr<ReadOperation> read_op, int result);

  // State for the number of times this read operation has been retried.
  int retries() { return retries_; }
  void IncrementRetries() { ++retries_; }

  int64 position() { return position_; }
  int size() { return size_; }
  uint8* data() { return data_; }

 private:
  int retries_;

  const int64 position_;
  const int size_;
  uint8* data_;
  media::DataSource::ReadCB callback_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ReadOperation);
};

BufferedDataSource::ReadOperation::ReadOperation(
    int64 position, int size, uint8* data,
    const media::DataSource::ReadCB& callback)
    : retries_(0),
      position_(position),
      size_(size),
      data_(data),
      callback_(callback) {
  DCHECK(!callback_.is_null());
}

BufferedDataSource::ReadOperation::~ReadOperation() {
  DCHECK(callback_.is_null());
}

// static
void BufferedDataSource::ReadOperation::Run(
    scoped_ptr<ReadOperation> read_op, int result) {
  base::ResetAndReturn(&read_op->callback_).Run(result);
}

BufferedDataSource::BufferedDataSource(
    const scoped_refptr<base::MessageLoopProxy>& render_loop,
    WebFrame* frame,
    media::MediaLog* media_log,
    const DownloadingCB& downloading_cb)
    : cors_mode_(BufferedResourceLoader::kUnspecified),
      total_bytes_(kPositionNotSpecified),
      assume_fully_buffered_(false),
      streaming_(false),
      frame_(frame),
      intermediate_read_buffer_(new uint8[kInitialReadBufferSize]),
      intermediate_read_buffer_size_(kInitialReadBufferSize),
      render_loop_(render_loop),
      stop_signal_received_(false),
      media_has_played_(false),
      preload_(AUTO),
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
  DCHECK(render_loop_->BelongsToCurrentThread());

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
  DCHECK(render_loop_->BelongsToCurrentThread());
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
  DCHECK(render_loop_->BelongsToCurrentThread());
  preload_ = preload;
}

bool BufferedDataSource::HasSingleOrigin() {
  DCHECK(render_loop_->BelongsToCurrentThread());
  DCHECK(init_cb_.is_null() && loader_.get())
      << "Initialize() must complete before calling HasSingleOrigin()";
  return loader_->HasSingleOrigin();
}

bool BufferedDataSource::DidPassCORSAccessCheck() const {
  return loader_.get() && loader_->DidPassCORSAccessCheck();
}

void BufferedDataSource::Abort() {
  DCHECK(render_loop_->BelongsToCurrentThread());
  {
    base::AutoLock auto_lock(lock_);
    StopInternal_Locked();
  }
  StopLoader();
  frame_ = NULL;
}

/////////////////////////////////////////////////////////////////////////////
// media::DataSource implementation.
void BufferedDataSource::Stop(const base::Closure& closure) {
  {
    base::AutoLock auto_lock(lock_);
    StopInternal_Locked();
  }
  closure.Run();

  render_loop_->PostTask(FROM_HERE,
      base::Bind(&BufferedDataSource::StopLoader, this));
}

void BufferedDataSource::SetPlaybackRate(float playback_rate) {
  render_loop_->PostTask(FROM_HERE, base::Bind(
      &BufferedDataSource::SetPlaybackRateTask, this, playback_rate));
}

void BufferedDataSource::SetBitrate(int bitrate) {
  render_loop_->PostTask(FROM_HERE, base::Bind(
      &BufferedDataSource::SetBitrateTask, this, bitrate));
}

void BufferedDataSource::Read(
    int64 position, int size, uint8* data,
    const media::DataSource::ReadCB& read_cb) {
  DVLOG(1) << "Read: " << position << " offset, " << size << " bytes";
  DCHECK(!read_cb.is_null());

  {
    base::AutoLock auto_lock(lock_);
    DCHECK(!read_op_);

    if (stop_signal_received_) {
      read_cb.Run(kReadError);
      return;
    }

    read_op_.reset(new ReadOperation(position, size, data, read_cb));
  }

  render_loop_->PostTask(FROM_HERE, base::Bind(
      &BufferedDataSource::ReadTask, this));
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
void BufferedDataSource::ReadTask() {
  DCHECK(render_loop_->BelongsToCurrentThread());
  ReadInternal();
}

void BufferedDataSource::StopInternal_Locked() {
  lock_.AssertAcquired();
  if (stop_signal_received_)
    return;

  stop_signal_received_ = true;

  // Initialize() isn't part of the DataSource interface so don't call it in
  // response to Stop().
  init_cb_.Reset();

  if (read_op_)
    ReadOperation::Run(read_op_.Pass(), kReadError);
}

void BufferedDataSource::StopLoader() {
  DCHECK(render_loop_->BelongsToCurrentThread());

  if (loader_.get())
    loader_->Stop();
}

void BufferedDataSource::SetPlaybackRateTask(float playback_rate) {
  DCHECK(render_loop_->BelongsToCurrentThread());
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
  DCHECK(render_loop_->BelongsToCurrentThread());
  DCHECK(loader_.get());

  bitrate_ = bitrate;
  loader_->SetBitrate(bitrate);
}

// This method is the place where actual read happens, |loader_| must be valid
// prior to make this method call.
void BufferedDataSource::ReadInternal() {
  DCHECK(render_loop_->BelongsToCurrentThread());
  int64 position = 0;
  int size = 0;
  {
    base::AutoLock auto_lock(lock_);
    if (stop_signal_received_)
      return;

    position = read_op_->position();
    size = read_op_->size();
  }

  // First we prepare the intermediate read buffer for BufferedResourceLoader
  // to write to.
  if (size > intermediate_read_buffer_size_) {
    intermediate_read_buffer_.reset(new uint8[size]);
  }

  // Perform the actual read with BufferedResourceLoader.
  loader_->Read(
      position, size, intermediate_read_buffer_.get(),
      base::Bind(&BufferedDataSource::ReadCallback, this));
}


/////////////////////////////////////////////////////////////////////////////
// BufferedResourceLoader callback methods.
void BufferedDataSource::StartCallback(
    BufferedResourceLoader::Status status) {
  DCHECK(render_loop_->BelongsToCurrentThread());
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
  DCHECK(render_loop_->BelongsToCurrentThread());
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
  ReadOperation::Run(read_op_.Pass(), kReadError);
}

void BufferedDataSource::ReadCallback(
    BufferedResourceLoader::Status status,
    int bytes_read) {
  DCHECK(render_loop_->BelongsToCurrentThread());

  // TODO(scherkus): we shouldn't have to lock to signal host(), see
  // http://crbug.com/113712 for details.
  base::AutoLock auto_lock(lock_);
  if (stop_signal_received_)
    return;

  if (status != BufferedResourceLoader::kOk) {
    // Stop the resource load if it failed.
    loader_->Stop();

    if (status == BufferedResourceLoader::kCacheMiss &&
        read_op_->retries() < kNumCacheMissRetries) {
      read_op_->IncrementRetries();

      // Recreate a loader starting from where we last left off until the
      // end of the resource.
      loader_.reset(CreateResourceLoader(
          read_op_->position(), kPositionNotSpecified));
      loader_->Start(
          base::Bind(&BufferedDataSource::PartialReadStartCallback, this),
          base::Bind(&BufferedDataSource::LoadingStateChangedCallback, this),
          base::Bind(&BufferedDataSource::ProgressCallback, this),
          frame_);
      return;
    }

    ReadOperation::Run(read_op_.Pass(), kReadError);
    return;
  }

  if (bytes_read > 0) {
    memcpy(read_op_->data(), intermediate_read_buffer_.get(), bytes_read);
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
  ReadOperation::Run(read_op_.Pass(), bytes_read);
}

void BufferedDataSource::LoadingStateChangedCallback(
    BufferedResourceLoader::LoadingState state) {
  DCHECK(render_loop_->BelongsToCurrentThread());

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
  DCHECK(render_loop_->BelongsToCurrentThread());

  if (assume_fully_buffered_)
    return;

  // TODO(scherkus): we shouldn't have to lock to signal host(), see
  // http://crbug.com/113712 for details.
  base::AutoLock auto_lock(lock_);
  if (stop_signal_received_)
    return;

  ReportOrQueueBufferedBytes(loader_->first_byte_position(), position);
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
