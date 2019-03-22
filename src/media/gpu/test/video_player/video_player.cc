// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/video_player.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "media/gpu/test/video_player/video.h"
#include "media/gpu/test/video_player/video_decoder_client.h"

#define DVLOGF(level) DVLOG(level) << __func__ << "(): "

namespace media {
namespace test {

VideoPlayer::VideoPlayer(const Video* video)
    : video_(video),
      video_player_state_(VideoPlayerState::kUninitialized),
      event_cv_(&event_lock_),
      video_player_event_counts_{} {}

VideoPlayer::~VideoPlayer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  Destroy();
}

// static
std::unique_ptr<VideoPlayer> VideoPlayer::Create(
    const Video* video,
    FrameRenderer* frame_renderer) {
  DCHECK(video);
  auto video_player = base::WrapUnique(new VideoPlayer(video));
  if (!video_player->Initialize(frame_renderer)) {
    return nullptr;
  }
  return video_player;
}

bool VideoPlayer::Initialize(FrameRenderer* frame_renderer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(video_player_state_, VideoPlayerState::kUninitialized);
  DCHECK(frame_renderer);
  DVLOGF(4);

  EventCallback event_cb =
      base::BindRepeating(&VideoPlayer::NotifyEvent, base::Unretained(this));

  decoder_client_ = VideoDecoderClient::Create(event_cb, frame_renderer);
  CHECK(decoder_client_) << "Failed to create decoder client";

  video_player_state_ = VideoPlayerState::kIdle;

  return true;
}

void VideoPlayer::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(video_player_state_, VideoPlayerState::kDestroyed);
  DVLOGF(4);

  decoder_client_.reset();
  video_player_state_ = VideoPlayerState::kDestroyed;
}

void VideoPlayer::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(video_player_state_, VideoPlayerState::kIdle);
  DVLOGF(4);

  // Create a decoder for the specified video.
  VideoDecodeAccelerator::Config decoder_config(video_->GetProfile());
  decoder_client_->CreateDecoder(decoder_config, video_->GetData());

  // Start decoding the video.
  video_player_state_ = VideoPlayerState::kDecoding;
  decoder_client_->DecodeNextFragment();
}

void VideoPlayer::Stop() {
  NOTIMPLEMENTED();
}

void VideoPlayer::Reset() {
  NOTIMPLEMENTED();
}

void VideoPlayer::Flush() {
  NOTIMPLEMENTED();
}

base::TimeDelta VideoPlayer::GetCurrentTime() const {
  NOTIMPLEMENTED();
  return base::TimeDelta();
}

size_t VideoPlayer::GetCurrentFrame() const {
  NOTIMPLEMENTED();
  return 0;
}

VideoPlayerState VideoPlayer::GetState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock auto_lock(event_lock_);
  return video_player_state_;
}

size_t VideoPlayer::GetEventCount(VideoPlayerEvent event) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock auto_lock(event_lock_);
  return video_player_event_counts_[static_cast<size_t>(event)];
}

bool VideoPlayer::WaitForEvent(VideoPlayerEvent event,
                               base::TimeDelta max_wait) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4) << "Event ID: " << static_cast<size_t>(event);

  static size_t event_id = 0;
  base::TimeDelta time_waiting;

  base::AutoLock auto_lock(event_lock_);
  while (true) {
    const base::TimeTicks start_time = base::TimeTicks::Now();
    event_cv_.TimedWait(max_wait);
    time_waiting += base::TimeTicks::Now() - start_time;

    // Go through list of events since last wait, looking for the event we're
    // interested in.
    for (; event_id < video_player_events_.size(); ++event_id) {
      if (video_player_events_[event_id] == event) {
        event_id++;
        return true;
      }
    }

    // Check whether we've exceeded the maximum time we're allowed to wait.
    if (time_waiting >= max_wait)
      return false;
  }
}

void VideoPlayer::NotifyEvent(VideoPlayerEvent event) {
  base::AutoLock auto_lock(event_lock_);
  if (event == VideoPlayerEvent::kFlushDone)
    video_player_state_ = VideoPlayerState::kIdle;
  video_player_events_.push_back(event);
  video_player_event_counts_[static_cast<size_t>(event)]++;
  event_cv_.Signal();
}

}  // namespace test
}  // namespace media
