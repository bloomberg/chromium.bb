// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/buffering_mixer_source.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/media/cma/backend/stream_mixer.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/decoder_buffer.h"

#define POST_TASK_TO_CALLER_THREAD(task, ...)                               \
  shim_task_runner_->PostTask(                                              \
      FROM_HERE, base::BindOnce(&PostTaskShim, caller_task_runner_,         \
                                base::BindOnce(&BufferingMixerSource::task, \
                                               weak_this_, ##__VA_ARGS__)));

namespace chromecast {
namespace media {

namespace {

const int kNumOutputChannels = 2;
const int64_t kInputQueueMs = 90;
const int kFadeTimeMs = 5;
const int kAudioReadyForPlaybackThresholdMs = 70;

// Special queue size and start threshold for "communications" streams to avoid
// issues with voice calling.
const int64_t kCommsInputQueueMs = 200;
const int64_t kCommsStartThresholdMs = 150;

void PostTaskShim(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                  base::OnceClosure task) {
  task_runner->PostTask(FROM_HERE, std::move(task));
}

std::string AudioContentTypeToString(media::AudioContentType type) {
  switch (type) {
    case media::AudioContentType::kAlarm:
      return "alarm";
    case media::AudioContentType::kCommunication:
      return "communication";
    default:
      return "media";
  }
}

int MsToSamples(int64_t ms, int sample_rate) {
  return ::media::AudioTimestampHelper::TimeToFrames(
      base::TimeDelta::FromMilliseconds(ms), sample_rate);
}

int64_t SamplesToMicroseconds(int64_t samples, int sample_rate) {
  return ::media::AudioTimestampHelper::FramesToTime(samples, sample_rate)
      .InMicroseconds();
}

int MaxQueuedFrames(const std::string& device_id, int sample_rate) {
  if (device_id == ::media::AudioDeviceDescription::kCommunicationsDeviceId) {
    return MsToSamples(kCommsInputQueueMs, sample_rate);
  }
  return MsToSamples(kInputQueueMs, sample_rate);
}

int StartThreshold(const std::string& device_id, int sample_rate) {
  if (device_id == ::media::AudioDeviceDescription::kCommunicationsDeviceId) {
    return MsToSamples(kCommsStartThresholdMs, sample_rate);
  }
  return MsToSamples(kAudioReadyForPlaybackThresholdMs, sample_rate);
}

}  // namespace

BufferingMixerSource::LockedMembers::Members::Members(
    BufferingMixerSource* source,
    int input_samples_per_second,
    int num_channels,
    int64_t playback_start_timestamp)
    : state_(State::kUninitialized),
      paused_(false),
      mixer_error_(false),
      queued_frames_(0),
      extra_delay_frames_(0),
      current_buffer_offset_(0),
      fader_(source,
             num_channels,
             MsToSamples(kFadeTimeMs, input_samples_per_second)),
      zero_fader_frames_(false),
      started_(false),
      playback_start_timestamp_(playback_start_timestamp) {}

BufferingMixerSource::LockedMembers::Members::~Members() = default;

BufferingMixerSource::LockedMembers::AcquiredLock::AcquiredLock(
    LockedMembers* members)
    : locked_(members) {
  DCHECK(locked_);
  locked_->member_lock_.Acquire();
}

BufferingMixerSource::LockedMembers::AcquiredLock::~AcquiredLock() {
  locked_->member_lock_.Release();
}

BufferingMixerSource::LockedMembers::AssertedLock::AssertedLock(
    LockedMembers* members)
    : locked_(members) {
  DCHECK(locked_);
  locked_->member_lock_.AssertAcquired();
}

BufferingMixerSource::LockedMembers::LockedMembers(
    BufferingMixerSource* source,
    int input_samples_per_second,
    int num_channels,
    int64_t playback_start_timestamp)
    : members_(source,
               input_samples_per_second,
               num_channels,
               playback_start_timestamp) {}

BufferingMixerSource::LockedMembers::~LockedMembers() = default;

BufferingMixerSource::LockedMembers::AcquiredLock
BufferingMixerSource::LockedMembers::Lock() {
  return AcquiredLock(this);
}

BufferingMixerSource::LockedMembers::AssertedLock
BufferingMixerSource::LockedMembers::AssertAcquired() {
  return AssertedLock(this);
}

BufferingMixerSource::BufferingMixerSource(Delegate* delegate,
                                           int input_samples_per_second,
                                           bool primary,
                                           const std::string& device_id,
                                           AudioContentType content_type,
                                           int playout_channel,
                                           int64_t playback_start_pts,
                                           bool start_playback_asap)
    : delegate_(delegate),
      num_channels_(kNumOutputChannels),
      input_samples_per_second_(input_samples_per_second),
      primary_(primary),
      device_id_(device_id),
      content_type_(content_type),
      playout_channel_(playout_channel),
      mixer_(StreamMixer::Get()),
      caller_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      shim_task_runner_(mixer_->shim_task_runner()),
      max_queued_frames_(MaxQueuedFrames(device_id, input_samples_per_second)),
      start_threshold_frames_(
          StartThreshold(device_id, input_samples_per_second)),
      playback_start_pts_(playback_start_pts),
      locked_members_(this,
                      input_samples_per_second,
                      num_channels_,
                      start_playback_asap ? INT64_MIN : INT64_MAX),
      weak_factory_(this) {
  LOG(INFO) << "Create " << device_id_ << " (" << this
            << "), content type = " << AudioContentTypeToString(content_type_)
            << ", playback_start_pts=" << playback_start_pts;
  DCHECK(delegate_);
  DCHECK(mixer_);
  DCHECK_LE(start_threshold_frames_, max_queued_frames_);
  weak_this_ = weak_factory_.GetWeakPtr();

  mixer_->AddInput(this);
}

void BufferingMixerSource::StartPlaybackAt(int64_t playback_start_timestamp) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(audio_ready_for_playback_fired_);
  LOG(INFO) << __func__
            << " playback_start_timestamp=" << playback_start_timestamp;

  auto locked = locked_members_.Lock();
  DCHECK(!locked->started_);
  DCHECK(locked->playback_start_timestamp_ == INT64_MAX);
  locked->playback_start_timestamp_ = playback_start_timestamp;
}

void BufferingMixerSource::RestartPlaybackAt(int64_t timestamp, int64_t pts) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  LOG(INFO) << __func__ << " timestamp=" << timestamp << " pts=" << pts;

  bool post_pcm_completion = false;
  {
    auto locked = locked_members_.Lock();
    DCHECK(locked->started_);

    playback_start_pts_ = pts;

    locked->playback_start_timestamp_ = timestamp;
    locked->started_ = false;

    locked->queue_.clear();
    locked->current_buffer_offset_ = 0;
    locked->queued_frames_ = 0;
    locked->mixer_rendering_delay_ = RenderingDelay();
    if (locked->pending_data_) {
      locked->pending_data_.reset();
      post_pcm_completion = true;
    }
  }
  if (post_pcm_completion) {
    POST_TASK_TO_CALLER_THREAD(PostPcmCompletion, RenderingDelay());
  }
}

float BufferingMixerSource::SetAvSyncPlaybackRate(float rate) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  LOG(INFO) << __func__ << " rate=" << rate;

  auto locked = locked_members_.Lock();
  return locked->audio_resampler_.SetMediaClockRate(rate);
}

BufferingMixerSource::~BufferingMixerSource() {
  LOG(INFO) << "Destroy " << device_id_ << " (" << this << ")";
}

int BufferingMixerSource::num_channels() {
  return num_channels_;
}
int BufferingMixerSource::input_samples_per_second() {
  return input_samples_per_second_;
}
bool BufferingMixerSource::primary() {
  return primary_;
}
const std::string& BufferingMixerSource::device_id() {
  return device_id_;
}
AudioContentType BufferingMixerSource::content_type() {
  return content_type_;
}
int BufferingMixerSource::desired_read_size() {
  return input_samples_per_second_ / 100;
}
int BufferingMixerSource::playout_channel() {
  return playout_channel_;
}

void BufferingMixerSource::WritePcm(scoped_refptr<DecoderBufferBase> data) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  auto locked = locked_members_.Lock();
  if (locked->state_ == State::kUninitialized ||
      locked->queued_frames_ + locked->fader_.buffered_frames() >=
          max_queued_frames_) {
    DCHECK(!locked->pending_data_);
    locked->pending_data_ = std::move(data);
    return;
  }
  RenderingDelay delay = QueueData(std::move(data));
  PostPcmCompletion(delay);
}

BufferingMixerSource::RenderingDelay BufferingMixerSource::QueueData(
    scoped_refptr<DecoderBufferBase> data) {
  auto locked = locked_members_.AssertAcquired();
  if (data->end_of_stream()) {
    LOG(INFO) << "End of stream for " << device_id_ << " (" << this << ")";
    locked->state_ = State::kGotEos;
  } else {
    // TODO(almasrymina): this drops 1 more buffer than necessary. What we
    // should do here is only drop if the playback_start_pts_ is not found in
    // the buffer, and use current_buffer_offset_ to effectively drop the
    // partial buffer.
    if (!locked->started_ && data->timestamp() < playback_start_pts_) {
      DCHECK(locked->queue_.empty());
      DVLOG(4) << "Dropping buffer with ts=" << data->timestamp()
               << ", playback_start_pts=" << playback_start_pts_
               << " difference=" << playback_start_pts_ - data->timestamp();
    } else {
      LOG_IF(INFO, (!locked->started_ &&
                    (data->timestamp() - playback_start_pts_) < 100000 &&
                    locked->playback_start_timestamp_ != INT64_MIN))
          << "Queueing pts diff=" << data->timestamp() - playback_start_pts_
          << " current buffered data=" << GetCurrentBufferedDataInUs() / 1000;

      scoped_refptr<DecoderBufferBase> buffer =
          locked->audio_resampler_.ResampleBuffer(std::move(data));

      const int frames = DataToFrames(buffer->data_size());
      locked->queued_frames_ += frames;
      locked->queue_.push_back(std::move(buffer));

      // TODO(almasrymina): this needs to be called outside the lock.
      // POST_TASK_TO_CALLER_THREAD should also probably DCHECK that the lock
      // is not held before executing.
      if (!locked->started_ &&
          GetCurrentBufferedDataInUs() >=
              kAudioReadyForPlaybackThresholdMs * 1000 &&
          locked->playback_start_timestamp_ != INT64_MIN) {
        POST_TASK_TO_CALLER_THREAD(PostAudioReadyForPlayback);
      }
    }
  }

  RenderingDelay delay;
  if (locked->started_ && !locked->paused_) {
    delay = locked->mixer_rendering_delay_;
    delay.delay_microseconds += SamplesToMicroseconds(
        locked->queued_frames_ + locked->extra_delay_frames_,
        input_samples_per_second_);
  }
  return delay;
}

void BufferingMixerSource::SetPaused(bool paused) {
  LOG(INFO) << (paused ? "Pausing " : "Unpausing ") << device_id_ << " ("
            << this << ")";
  auto locked = locked_members_.Lock();
  locked->mixer_rendering_delay_ = RenderingDelay();
  locked->paused_ = paused;
}

void BufferingMixerSource::SetVolumeMultiplier(float multiplier) {
  mixer_->SetVolumeMultiplier(this, multiplier);
}

void BufferingMixerSource::InitializeAudioPlayback(
    int read_size,
    RenderingDelay initial_rendering_delay) {
  // Start accepting buffers into the queue.
  bool queued_data = false;
  RenderingDelay pending_buffer_delay;
  {
    auto locked = locked_members_.Lock();
    locked->mixer_rendering_delay_ = initial_rendering_delay;
    if (locked->state_ == State::kUninitialized) {
      locked->state_ = State::kNormalPlayback;
    } else {
      DCHECK(locked->state_ == State::kRemoved);
    }

    if (locked->pending_data_ &&
        locked->queued_frames_ + locked->fader_.buffered_frames() <
            max_queued_frames_) {
      pending_buffer_delay = QueueData(std::move(locked->pending_data_));
      queued_data = true;
    }
  }

  if (queued_data) {
    POST_TASK_TO_CALLER_THREAD(PostPcmCompletion, pending_buffer_delay);
  }
}

void BufferingMixerSource::CheckAndStartPlaybackIfNecessary(
    int num_frames,
    int64_t playback_absolute_timestamp) {
  auto locked = locked_members_.AssertAcquired();

  DCHECK(locked->state_ == State::kNormalPlayback && !locked->started_);

  if (locked->queued_frames_ >= start_threshold_frames_ &&
      locked->queued_frames_ >=
          locked->fader_.FramesNeededFromSource(num_frames) &&
      (locked->playback_start_timestamp_ == INT64_MIN ||
       playback_absolute_timestamp +
               SamplesToMicroseconds(num_frames, input_samples_per_second_) >=
           locked->playback_start_timestamp_)) {
    // - playback_start_timestamp_ == INT64_MIN indicates the playback should
    // start ASAP.
    // - playback start timestamp_ == INT64_MAX indicates playback is AV
    // sync'd, but we don't have a timestamp to start playback at.
    //
    // We do not queue silence in either of those cases.
    if (!locked->started_ && locked->playback_start_timestamp_ != INT64_MIN &&
        locked->playback_start_timestamp_ != INT64_MAX) {
      DCHECK(!locked->queue_.empty()) << "We were supposed to start playback "
                                         "now but the queue is empty...";

      // - We should start playing at PTS playback_start_pts_, but we will
      // actually be starting at front()->timestamp().
      //
      // - We want the playback_start_pts_ sample to play out at
      // playback_start_timestamp_.
      //
      // - Therefore, we should start playing the data that we have (that
      // doesn't necessarily have the desired PTS) at:
      //
      // playback_start_timestamp_ +
      // (front()->timestamp() - playback_start_pts_)
      //
      // so that the sample at playback_start_pts_ plays out at exactly (ie,
      // the PTS of the first buffer we play might not match
      // playback_start_pts_).
      uint64_t pts_corrected_playback_start_timestamp =
          locked->playback_start_timestamp_ +
          (locked->queue_.front()->timestamp() - playback_start_pts_);

      int64_t silence_duration =
          pts_corrected_playback_start_timestamp - playback_absolute_timestamp;

      if (silence_duration <= 0) {
        DropAudio(::media::AudioTimestampHelper::TimeToFrames(
            base::TimeDelta::FromMicroseconds(-silence_duration),
            input_samples_per_second_));
      } else {
        LOG(INFO) << "Adding silence. Duration=" << silence_duration;
        remaining_silence_frames_ = ::media::AudioTimestampHelper::TimeToFrames(
            base::TimeDelta::FromMicroseconds(silence_duration),
            input_samples_per_second_);
      }
    }

    locked->started_ = true;
  }
}

int BufferingMixerSource::FillAudioPlaybackFrames(
    int num_frames,
    RenderingDelay rendering_delay,
    ::media::AudioBus* buffer) {
  DCHECK(buffer);
  DCHECK_EQ(num_channels_, buffer->channels());
  DCHECK_GE(buffer->frames(), num_frames);

  int64_t playback_absolute_timestamp = rendering_delay.delay_microseconds +
                                        rendering_delay.timestamp_microseconds;

  int filled = 0;
  bool queued_more_data = false;
  bool signal_eos = false;
  bool remove_self = false;
  RenderingDelay pending_buffer_delay;
  {
    auto locked = locked_members_.Lock();

    // Playback start check.
    if (locked->state_ == State::kNormalPlayback && !locked->started_) {
      CheckAndStartPlaybackIfNecessary(num_frames, playback_absolute_timestamp);
    }

    // In normal playback, don't pass data to the fader if we can't satisfy the
    // full request. This will allow us to buffer up more data so we can fully
    // fade in.
    if (locked->state_ == State::kNormalPlayback && locked->started_ &&
        locked->queued_frames_ <
            locked->fader_.FramesNeededFromSource(num_frames)) {
      LOG_IF(INFO, locked->started_ && !locked->zero_fader_frames_)
          << "Stream underrun for " << device_id_ << " (" << this << ")";
      locked->zero_fader_frames_ = true;
    } else {
      LOG_IF(INFO, locked->started_ && locked->zero_fader_frames_)
          << "Stream underrun recovered for " << device_id_ << " (" << this
          << ")";
      locked->zero_fader_frames_ = false;
    }

    DCHECK_GE(remaining_silence_frames_, 0);
    if (remaining_silence_frames_ >= num_frames) {
      remaining_silence_frames_ -= num_frames;
      return 0;
    }

    int write_offset = 0;
    if (remaining_silence_frames_ > 0) {
      buffer->ZeroFramesPartial(0, remaining_silence_frames_);
      filled += remaining_silence_frames_;
      num_frames -= remaining_silence_frames_;
      write_offset = remaining_silence_frames_;
      remaining_silence_frames_ = 0;
    }

    filled = locked->fader_.FillFrames(num_frames, buffer, write_offset);

    locked->mixer_rendering_delay_ = rendering_delay;
    locked->extra_delay_frames_ = num_frames + locked->fader_.buffered_frames();

    // See if we can accept more data into the queue.
    if (locked->pending_data_ &&
        locked->queued_frames_ + locked->fader_.buffered_frames() <
            max_queued_frames_) {
      pending_buffer_delay = QueueData(std::move(locked->pending_data_));
      queued_more_data = true;
    }

    // Check if we have played out EOS.
    if (locked->state_ == State::kGotEos && locked->queued_frames_ == 0 &&
        locked->fader_.buffered_frames() == 0) {
      signal_eos = true;
      locked->state_ = State::kSignaledEos;
    }

    // If the caller has removed this source, delete once we have faded out.
    if (locked->state_ == State::kRemoved &&
        locked->fader_.buffered_frames() == 0) {
      remove_self = true;
    }
  }

  if (queued_more_data) {
    POST_TASK_TO_CALLER_THREAD(PostPcmCompletion, pending_buffer_delay);
  }
  if (signal_eos) {
    POST_TASK_TO_CALLER_THREAD(PostEos);
  }

  if (remove_self) {
    mixer_->RemoveInput(this);
  }
  return filled;
}

bool BufferingMixerSource::CanDropFrames(int64_t frames_to_drop) {
  int64_t duration_of_frames_us =
      SamplesToMicroseconds(frames_to_drop, input_samples_per_second_);

  return (GetCurrentBufferedDataInUs() - duration_of_frames_us) >=
         (kAudioReadyForPlaybackThresholdMs * 1000);
}

int64_t BufferingMixerSource::DataToFrames(int64_t size_in_bytes) {
  return size_in_bytes / (num_channels_ * sizeof(float));
}

void BufferingMixerSource::DropAudio(int64_t frames_to_drop) {
  auto locked = locked_members_.AssertAcquired();

  LOG(INFO) << "Dropping audio duration="
            << SamplesToMicroseconds(frames_to_drop, input_samples_per_second_);

  DCHECK(!locked->queue_.empty());

  int64_t first_buffer_frames =
      DataToFrames(locked->queue_.front()->data_size());
  while (frames_to_drop >= first_buffer_frames &&
         CanDropFrames(first_buffer_frames)) {
    locked->queued_frames_ -= first_buffer_frames;
    frames_to_drop -= first_buffer_frames;

    locked->queue_.pop_front();
    DCHECK(!locked->queue_.empty());
    first_buffer_frames = DataToFrames(locked->queue_.front()->data_size());
  }

  if (CanDropFrames(frames_to_drop)) {
    locked->current_buffer_offset_ += frames_to_drop;
    locked->queued_frames_ -= frames_to_drop;
  } else {
    LOG(WARNING) << "Starting audio with error="
                 << SamplesToMicroseconds(frames_to_drop,
                                          input_samples_per_second_);
  }

  DCHECK(!locked->queue_.empty());
  DCHECK(locked->current_buffer_offset_ <=
         DataToFrames(locked->queue_.front()->data_size()));
  DCHECK(locked->queued_frames_ >= 0);
}

int BufferingMixerSource::FillFaderFrames(::media::AudioBus* dest,
                                          int frame_offset,
                                          int num_frames) {
  DCHECK(dest);
  DCHECK_EQ(num_channels_, dest->channels());
  auto locked = locked_members_.AssertAcquired();

  if (locked->zero_fader_frames_ || !locked->started_ || locked->paused_ ||
      locked->state_ == State::kRemoved) {
    return 0;
  }

  int num_filled = 0;
  while (num_frames) {
    if (locked->queue_.empty()) {
      return num_filled;
    }

    DecoderBufferBase* buffer = locked->queue_.front().get();
    const int buffer_frames = DataToFrames(buffer->data_size());
    const int frames_to_copy =
        std::min(num_frames, buffer_frames - locked->current_buffer_offset_);
    DCHECK(frames_to_copy >= 0 && frames_to_copy <= num_frames)
        << " frames_to_copy=" << frames_to_copy << " num_frames=" << num_frames
        << " buffer_frames=" << buffer_frames
        << " locked->current_buffer_offset_=" << locked->current_buffer_offset_
        << " buffer=" << buffer->data_size();

    DCHECK_LE(frames_to_copy + frame_offset, dest->frames())
        << " frames_to_copy=" << frames_to_copy
        << " dest->frames()=" << dest->frames()
        << " frame_offset=" << frame_offset;

    const float* buffer_samples =
        reinterpret_cast<const float*>(buffer->data());
    for (int c = 0; c < num_channels_; ++c) {
      const float* buffer_channel = buffer_samples + (buffer_frames * c);
      memcpy(dest->channel(c) + frame_offset,
             buffer_channel + locked->current_buffer_offset_,
             frames_to_copy * sizeof(float));
    }

    num_frames -= frames_to_copy;
    locked->queued_frames_ -= frames_to_copy;
    frame_offset += frames_to_copy;
    num_filled += frames_to_copy;

    locked->current_buffer_offset_ += frames_to_copy;
    if (locked->current_buffer_offset_ == buffer_frames) {
      locked->queue_.pop_front();
      locked->current_buffer_offset_ = 0;
    }
  }

  return num_filled;
}

void BufferingMixerSource::PostPcmCompletion(RenderingDelay delay) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  delegate_->OnWritePcmCompletion(delay);
}

void BufferingMixerSource::PostEos() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  delegate_->OnEos();
}

void BufferingMixerSource::PostAudioReadyForPlayback() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate_);
  if (!audio_ready_for_playback_fired_) {
    audio_ready_for_playback_fired_ = true;
    delegate_->OnAudioReadyForPlayback();
  }
}

void BufferingMixerSource::OnAudioPlaybackError(MixerError error) {
  if (error == MixerError::kInputIgnored) {
    LOG(INFO) << "Mixer input " << device_id_ << " (" << this << ")"
              << " now being ignored due to output sample rate change";
  }

  POST_TASK_TO_CALLER_THREAD(PostError, error);

  auto locked = locked_members_.Lock();
  locked->mixer_error_ = true;
  if (locked->state_ == State::kRemoved) {
    mixer_->RemoveInput(this);
  }
}

void BufferingMixerSource::PostError(MixerError error) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  delegate_->OnMixerError(error);
}

void BufferingMixerSource::Remove() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  weak_factory_.InvalidateWeakPtrs();

  LOG(INFO) << "Remove " << device_id_ << " (" << this << ")";
  bool remove_self = false;
  {
    auto locked = locked_members_.Lock();
    locked->pending_data_ = nullptr;
    locked->state_ = State::kRemoved;
    remove_self = locked->mixer_error_;
  }

  if (remove_self) {
    mixer_->RemoveInput(this);
  }
}

void BufferingMixerSource::FinalizeAudioPlayback() {
  delete this;
}

int64_t BufferingMixerSource::GetCurrentBufferedDataInUs() {
  auto locked = locked_members_.AssertAcquired();

  int64_t buffered_data = 0;
  for (auto buffer : locked->queue_) {
    buffered_data += buffer->data_size();
  }

  int buffered_frames = DataToFrames(buffered_data);
  buffered_frames -= locked->current_buffer_offset_;

  return SamplesToMicroseconds(buffered_frames, input_samples_per_second_);
}

}  // namespace media
}  // namespace chromecast
