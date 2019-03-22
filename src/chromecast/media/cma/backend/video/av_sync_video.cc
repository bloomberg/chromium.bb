// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/video/av_sync_video.h"

#include <algorithm>
#include <iomanip>
#include <limits>

#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromecast/media/cma/backend/audio_decoder_for_mixer.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_for_mixer.h"
#include "chromecast/media/cma/backend/video_decoder_for_mixer.h"

#define LIMITED_CAST_MEDIA_LOG(level, count, max)                             \
  LOG_IF(level, (count) < (max) && ((count)++ || true))                       \
      << (((count) == (max)) ? "(Log limit reached. Further similar entries " \
                               "may be suppressed): "                         \
                             : "")

namespace chromecast {
namespace media {

namespace {

// Threshold where the audio and video pts are far enough apart such that we
// want to do a hard correction.
const int kHardCorrectionThresholdUs = 100000;

// Length of time after which data is forgotten from our linear regression
// models.
const int kLinearRegressionDataLifetimeUs =
    base::TimeDelta::FromMinutes(1).InMicroseconds();

// Time interval between AV sync upkeeps.
constexpr base::TimeDelta kAvSyncUpkeepInterval =
    base::TimeDelta::FromMilliseconds(16);

// Time interval between checking playbacks statistics.
constexpr base::TimeDelta kPlaybackStatisticsCheckInterval =
    base::TimeDelta::FromSeconds(5);

// This is the threshold for which we consider the rate of playback variation
// to be valid. If we measure a rate of playback variation worse than this, we
// consider the linear regression measurement invalid, we flush the linear
// regression and let AvSync collect samples all over again.
const double kExpectedSlopeVariance = 0.005;

// The threshold after which LIMITED_CAST_MEDIA_LOG will no longer write the
// logs.
const int kCastMediaLogThreshold = 3;

// We don't AV sync content with frame rate less than this. This low framerate
// indicates that the content happens to be audio-centric, with a dummy video
// stream.
const int kAvSyncFpsThreshold = 10;

// Every time we upkeep the audio rate of playback, we aim to recover from our
// existing AV sync drift in this duration.
const float kReSyncDurationUs =
    base::TimeDelta::FromSeconds(30).InMicroseconds();

// We don't start AV syncing until this amount after the playback has started
// or resumed.
const int64_t kAvSyncStartGracePeriodUs =
    base::TimeDelta::FromSeconds(5).InMicroseconds();
}  // namespace

std::unique_ptr<AvSync> AvSync::Create(
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaPipelineBackendForMixer* const backend) {
  return std::make_unique<AvSyncVideo>(task_runner, backend);
}

AvSyncVideo::AvSyncVideo(
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaPipelineBackendForMixer* const backend)
    : audio_pts_(
          new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs)),
      video_pts_(
          new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs)),
      backend_(backend) {
  DCHECK(backend_);
}

void AvSyncVideo::UpkeepAvSync() {
  if (backend_->MonotonicClockNow() - kAvSyncStartGracePeriodUs <
      playback_start_timestamp_us_) {
    return;
  }

  if (!backend_->video_decoder() || !backend_->audio_decoder()) {
    DVLOG(4) << "No video decoder available.";
    return;
  }

  int64_t now = backend_->MonotonicClockNow();

  int64_t new_raw_vpts = 0;
  int64_t new_vpts_timestamp = 0;
  if (!backend_->video_decoder()->GetCurrentPts(&new_vpts_timestamp,
                                                &new_raw_vpts)) {
    LIMITED_CAST_MEDIA_LOG(ERROR, spammy_log_count_, kCastMediaLogThreshold)
        << "Failed to get VPTS.";
    return;
  }

  if (new_raw_vpts != last_vpts_value_recorded_) {
    video_pts_->AddSample(new_vpts_timestamp, new_raw_vpts, 1.0);
    last_vpts_value_recorded_ = new_raw_vpts;
  }

  int64_t new_raw_apts = 0;
  int64_t new_apts_timestamp = 0;

  if (!backend_->audio_decoder()->GetTimestampedPts(&new_apts_timestamp,
                                                    &new_raw_apts)) {
    LIMITED_CAST_MEDIA_LOG(ERROR, spammy_log_count_, kCastMediaLogThreshold)
        << "Failed to get APTS.";
    return;
  }

  if (new_raw_apts != last_apts_value_recorded_) {
    audio_pts_->AddSample(new_apts_timestamp, new_raw_apts, 1.0);
    last_apts_value_recorded_ = new_raw_apts;
  }

  if (video_pts_->num_samples() < 10 || audio_pts_->num_samples() < 20) {
    DVLOG(4) << "Linear regression samples too little."
             << " video_pts_->num_samples()=" << video_pts_->num_samples()
             << " audio_pts_->num_samples()=" << audio_pts_->num_samples();
    return;
  }

  int64_t linear_regression_vpts = 0;
  int64_t linear_regression_apts = 0;
  double vpts_slope = 0.0;
  double apts_slope = 0.0;
  double vpts_slope_error = 0.0;
  double apts_slope_error = 0.0;
  double error = 0.0;

  if (!video_pts_->EstimateY(now, &linear_regression_vpts, &error) ||
      !audio_pts_->EstimateY(now, &linear_regression_apts, &error) ||
      !video_pts_->EstimateSlope(&vpts_slope, &vpts_slope_error) ||
      !audio_pts_->EstimateSlope(&apts_slope, &apts_slope_error)) {
    DVLOG(3) << "Failed to get linear regression estimate.";
    return;
  }

  // Positive difference means it looks like the audio started playing before
  // the video, which means the audio is ahead the video.
  int64_t linear_regression_difference =
      linear_regression_apts - linear_regression_vpts;

  // TODO(almasrymina): Hard correction don't seem to work in playback where the
  // playback rate has been changed. I.e. after the hard correction the
  // difference is either pretty much the same or increased, and we get stuck
  // in a loop doing hard corrections.
  //
  // Note hard corrections need to be done based on the
  // linear_regression_difference and not the raw_difference. This is because
  // if there is even 1 bad apts sample or vpts sample and we follow the
  // raw_difference, we will do a hard correction and ruin the playback.
  if (abs(linear_regression_difference) > kHardCorrectionThresholdUs &&
      current_media_playback_rate_ == 1.0) {
    HardCorrection(now, new_raw_vpts, new_vpts_timestamp,
                   linear_regression_difference);
    return;
  }

  if (vpts_slope_error > 0.00001) {
    DVLOG(3) << "vpts slope estimate error too big=" << vpts_slope_error;
    return;
  }

  if (apts_slope_error > 0.00001 || audio_pts_->num_samples() < 1000) {
    DVLOG(3) << "apts slope estimate error too big. error=" << apts_slope_error
             << " num_samples=" << audio_pts_->num_samples();
    return;
  }

  if (abs(vpts_slope - current_media_playback_rate_) > kExpectedSlopeVariance) {
    LOG(ERROR) << "Calculated bad vpts_slope=" << vpts_slope
               << ". Expected value close to=" << current_media_playback_rate_
               << ". Flushing...";
    FlushVideoPts();
    return;
  }

  if (abs(apts_slope - (current_media_playback_rate_ *
                        current_av_sync_audio_playback_rate_)) >
      kExpectedSlopeVariance) {
    LOG(ERROR) << "Calculated bad apts_slope=" << apts_slope
               << ". Expected value close to="
               << (current_media_playback_rate_ *
                   current_av_sync_audio_playback_rate_)
               << ". Flushing...";
    FlushAudioPts();
    return;
  }

  if (!first_video_pts_received_) {
    LOG(INFO) << "Video starting at difference="
              << (new_vpts_timestamp - new_raw_vpts) -
                     (playback_start_timestamp_us_ - playback_start_pts_us_);
    first_video_pts_received_ = true;
  }

  if (!first_audio_pts_received_) {
    LOG(INFO) << "Audio starting at difference="
              << (new_apts_timestamp - new_raw_apts) -
                     (playback_start_timestamp_us_ - playback_start_pts_us_);
    first_audio_pts_received_ = true;
  }

  DVLOG(3) << "Pts_monitor."
           << " linear_regression_difference=" << linear_regression_difference
           << " apts_slope=" << apts_slope
           << " current_av_sync_audio_playback_rate_="
           << current_av_sync_audio_playback_rate_
           << " new_raw_vpts=" << new_raw_vpts
           << " new_raw_apts=" << new_raw_apts
           << " current_time=" << backend_->MonotonicClockNow()
           << " vpts_slope=" << vpts_slope
           << " vpts_slope_error=" << vpts_slope_error
           << " apts_slope=" << apts_slope
           << " apts_slope_error=" << apts_slope_error
           << " video_pts_->num_samples()=" << video_pts_->num_samples()
           << " audio_pts_->num_samples()=" << audio_pts_->num_samples();

  av_sync_difference_sum_ += linear_regression_difference;
  ++av_sync_difference_count_;

  if (GetContentFrameRate() < kAvSyncFpsThreshold) {
    DVLOG(3) << "Content frame rate=" << GetContentFrameRate()
             << ". Not AV syncing.";
  }

  AudioRateUpkeep(now, new_raw_vpts, new_raw_apts, apts_slope, vpts_slope,
                  linear_regression_difference);
}

int AvSyncVideo::GetContentFrameRate() {
  const std::deque<WeightedMovingLinearRegression::Sample>& video_samples =
      video_pts_->samples();

  if (video_pts_->num_samples() < 2) {
    return INT_MAX;
  }
  int duration = video_samples.back().x - video_samples.front().x;

  if (duration <= 0) {
    return INT_MAX;
  }

  return std::round(static_cast<float>(video_pts_->num_samples() * 1000000) /
                    static_cast<float>(duration));
}

void AvSyncVideo::FlushAudioPts() {
  audio_pts_.reset(
      new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs));
}

void AvSyncVideo::FlushVideoPts() {
  video_pts_.reset(
      new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs));
}

void AvSyncVideo::HardCorrection(int64_t now,
                                 int64_t new_raw_vpts,
                                 int64_t new_vpts_timestamp,
                                 int64_t linear_regression_difference) {
  LOG(INFO) << "Hard correction."
            << " linear_regression_difference=" << linear_regression_difference
            << " new_raw_vpts=" << new_raw_vpts
            << " current_av_sync_audio_playback_rate_="
            << current_av_sync_audio_playback_rate_;

  backend_->audio_decoder()->RestartPlaybackAt(new_vpts_timestamp,
                                               new_raw_vpts);

  audio_pts_.reset(
      new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs));
}

// We calculate the desired audio playback rate, and set the rate to that if
// it's different from the current rate. The desired rate of audio playback is
// that one which will bring us to 0 sync difference in kReSyncDurationUs.
void AvSyncVideo::AudioRateUpkeep(int64_t now,
                                  int64_t new_raw_vpts,
                                  int64_t new_raw_apts,
                                  double apts_slope,
                                  double vpts_slope,
                                  int64_t linear_regression_difference) {
  // Positive difference means the audio is ahead the video, so the audio
  // needs to slow down, and factor will be < 1.0. Negative difference is the
  // opposite.
  double factor = 1.0 - (static_cast<double>(linear_regression_difference) /
                         kReSyncDurationUs);

  // Don't update  the audio playback rate unless the apts_slope is off by 1ppm
  // from the desired value.
  if (abs(apts_slope - (vpts_slope * factor)) >= 0.000001) {
    current_av_sync_audio_playback_rate_ =
        backend_->audio_decoder()->SetAvSyncPlaybackRate(
            current_av_sync_audio_playback_rate_ * vpts_slope * factor /
            apts_slope);

    audio_pts_.reset(
        new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs));

    LOG(INFO) << "Audio upkeep."
              << " linear_regression_difference="
              << linear_regression_difference << " apts_slope=" << apts_slope
              << " vpts_slope=" << vpts_slope
              << " new_raw_apts=" << new_raw_apts
              << " new_raw_vpts=" << new_raw_vpts << " factor=" << factor
              << " current_av_sync_audio_playback_rate_="
              << current_av_sync_audio_playback_rate_;
  }
}

void AvSyncVideo::GatherPlaybackStatistics() {
  DCHECK(backend_);
  if (!backend_->video_decoder() || av_sync_difference_count_ == 0) {
    return;
  }

  double average_av_sync_difference =
      static_cast<double>(av_sync_difference_sum_) /
      static_cast<double>(av_sync_difference_count_);

  av_sync_difference_sum_ = 0;
  av_sync_difference_count_ = 0;

  int64_t frame_rate_difference =
      (backend_->video_decoder()->GetCurrentContentRefreshRate() -
       backend_->video_decoder()->GetOutputRefreshRate()) /
      1000;

  int64_t expected_dropped_frames_per_second =
      std::max<int64_t>(frame_rate_difference, 0);

  int64_t expected_repeated_frames_per_second =
      std::max<int64_t>(-frame_rate_difference, 0);

  int64_t current_time = backend_->MonotonicClockNow();

  int64_t expected_dropped_frames =
      std::round(static_cast<float>(expected_dropped_frames_per_second) *
                 (static_cast<float>(current_time - last_gather_timestamp_us_) /
                  1000000.0));

  int64_t expected_repeated_frames =
      std::round(static_cast<float>(expected_repeated_frames_per_second) *
                 (static_cast<float>(current_time - last_gather_timestamp_us_) /
                  1000000.0));

  int64_t dropped_frames = backend_->video_decoder()->GetDroppedFrames();
  int64_t repeated_frames = backend_->video_decoder()->GetRepeatedFrames();

  int64_t unexpected_dropped_frames =
      (dropped_frames - last_dropped_frames_) - expected_dropped_frames;
  int64_t unexpected_repeated_frames =
      (repeated_frames - last_repeated_frames_) - expected_repeated_frames;

  int64_t accurate_vpts = 0;
  int64_t accurate_vpts_timestamp = 0;
  backend_->video_decoder()->GetCurrentPts(&accurate_vpts_timestamp,
                                           &accurate_vpts);

  int64_t accurate_apts = 0;
  int64_t accurate_apts_timestamp = 0;
  backend_->video_decoder()->GetCurrentPts(&accurate_apts_timestamp,
                                           &accurate_apts);

  LOG(INFO) << "Playback diagnostics:"
            << " average_av_sync_difference=" << average_av_sync_difference
            << " content fps=" << GetContentFrameRate()
            << " unexpected_dropped_frames=" << unexpected_dropped_frames
            << " unexpected_repeated_frames=" << unexpected_repeated_frames;

  int64_t linear_regression_vpts = 0;
  int64_t linear_regression_apts = 0;
  double error = 0.0;
  if (!video_pts_->EstimateY(current_time, &linear_regression_vpts, &error) ||
      !audio_pts_->EstimateY(current_time, &linear_regression_apts, &error)) {
    DVLOG(3) << "Failed to get linear regression estimate.";
    return;
  }

  if (delegate_) {
    delegate_->NotifyAvSyncPlaybackStatistics(
        unexpected_dropped_frames, unexpected_repeated_frames,
        average_av_sync_difference, linear_regression_apts,
        linear_regression_vpts, 0, 0);
  }

  last_gather_timestamp_us_ = current_time;
  last_repeated_frames_ = repeated_frames;
  last_dropped_frames_ = dropped_frames;
}

void AvSyncVideo::NotifyStart(int64_t timestamp, int64_t pts) {
  playback_start_timestamp_us_ = timestamp;

  current_media_playback_rate_ = 1.0;
  current_av_sync_audio_playback_rate_ = 1.0;

  playback_start_pts_us_ = pts;
  LOG(INFO) << __func__
            << " playback_start_timestamp_us_=" << playback_start_timestamp_us_
            << " playback_start_pts_us_=" << playback_start_pts_us_;

  StartAvSync();
}

void AvSyncVideo::NotifyStop() {
  LOG(INFO) << __func__;
  StopAvSync();
  playback_start_timestamp_us_ = INT64_MAX;
  playback_start_pts_us_ = INT64_MIN;
}

void AvSyncVideo::NotifyPause() {
  StopAvSync();
}

void AvSyncVideo::NotifyResume() {
  int64_t now = backend_->MonotonicClockNow();

  // If for some reason we get a resume before we hit the playback start time,
  // we need to retain it.
  if (now > playback_start_timestamp_us_) {
    playback_start_timestamp_us_ = now;
  }
  LOG(INFO) << __func__
            << " playback_start_timestamp_us_=" << playback_start_timestamp_us_;
  StartAvSync();
}

void AvSyncVideo::NotifyPlaybackRateChange(float rate) {
  DCHECK(backend_->video_decoder());
  DCHECK(backend_->audio_decoder());

  current_media_playback_rate_ = rate;

  backend_->video_decoder()->SetPlaybackRate(current_media_playback_rate_);

  FlushAudioPts();
  FlushVideoPts();

  LOG(INFO) << __func__
            << " current_media_playback_rate_=" << current_media_playback_rate_
            << " current_av_sync_audio_playback_rate_="
            << current_av_sync_audio_playback_rate_;
}

void AvSyncVideo::StartAvSync() {
  audio_pts_.reset(
      new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs));
  video_pts_.reset(
      new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs));

  first_audio_pts_received_ = false;
  first_video_pts_received_ = false;

  spammy_log_count_ = 0;

  upkeep_av_sync_timer_.Start(FROM_HERE, kAvSyncUpkeepInterval, this,
                              &AvSyncVideo::UpkeepAvSync);
  // TODO(almasrymina): if this logic turns out to be useful for metrics
  // recording, keep it and remove this TODO. Otherwise remove it.
  playback_statistics_timer_.Start(FROM_HERE, kPlaybackStatisticsCheckInterval,
                                   this,
                                   &AvSyncVideo::GatherPlaybackStatistics);
}

void AvSyncVideo::StopAvSync() {
  upkeep_av_sync_timer_.Stop();
  playback_statistics_timer_.Stop();
}

AvSyncVideo::~AvSyncVideo() = default;

}  // namespace media
}  // namespace chromecast
