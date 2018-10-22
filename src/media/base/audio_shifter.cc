// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_shifter.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_bus.h"

namespace media {

// return true if x is between a and b.
static bool between(double x, double a, double b) {
  if (b < a)
    return b <= x && x <= a;
  return a <= x && x <= b;
}

class ClockSmoother {
 public:
  explicit ClockSmoother(base::TimeDelta clock_accuracy) :
      clock_accuracy_(clock_accuracy),
      inaccuracy_delta_(clock_accuracy * 10) {
    inaccuracies_.push_back(std::make_pair(inaccuracy_sum_, inaccuracy_delta_));
  }

  base::TimeTicks Smooth(base::TimeTicks t,
                         base::TimeDelta delta) {
    base::TimeTicks ret = t;
    if (!previous_.is_null()) {
      base::TimeDelta actual_delta = t - previous_;
      base::TimeDelta new_fraction_off = actual_delta - delta;
      inaccuracy_sum_ += new_fraction_off;
      inaccuracy_delta_ += actual_delta;
      inaccuracies_.push_back(std::make_pair(new_fraction_off, actual_delta));
      if (inaccuracies_.size() > 1000) {
        inaccuracy_sum_ -= inaccuracies_.front().first;
        inaccuracy_delta_ -= inaccuracies_.front().second;
        inaccuracies_.pop_front();
      }
      // 0.01 means 1% faster than regular clock.
      // -0.02 means 2% slower than regular clock.
      double fraction_off = inaccuracy_sum_.InSecondsF() /
          inaccuracy_delta_.InSecondsF();

      double delta_seconds = delta.InSecondsF();
      delta_seconds += delta_seconds * fraction_off;
      base::TimeTicks expected = previous_ +
          base::TimeDelta::FromSecondsD(delta_seconds);
      base::TimeDelta diff = t - expected;
      if (diff < clock_accuracy_ && diff > -clock_accuracy_) {
        ret = t + diff / 1000;
      }
    }
    previous_ = ret;
    return ret;
  }

  // 1.01 means 1% faster than regular clock.
  // -0.98 means 2% slower than regular clock.
  double Rate() const {
    return 1.0 + inaccuracy_sum_.InSecondsF() /
          inaccuracy_delta_.InSecondsF();
  }

 private:
  base::TimeDelta clock_accuracy_;
  base::circular_deque<std::pair<base::TimeDelta, base::TimeDelta>>
      inaccuracies_;
  base::TimeDelta inaccuracy_sum_;
  base::TimeDelta inaccuracy_delta_;
  base::TimeTicks previous_;
};

AudioShifter::AudioQueueEntry::AudioQueueEntry(
    base::TimeTicks target_playout_time_,
    std::unique_ptr<AudioBus> audio_)
    : target_playout_time(target_playout_time_), audio(audio_.release()) {}

AudioShifter::AudioQueueEntry::AudioQueueEntry(const AudioQueueEntry& other) =
    default;

AudioShifter::AudioQueueEntry::~AudioQueueEntry() = default;

AudioShifter::AudioShifter(base::TimeDelta max_buffer_size,
                           base::TimeDelta clock_accuracy,
                           base::TimeDelta adjustment_time,
                           int rate,
                           int channels)
    : max_buffer_size_(max_buffer_size),
      clock_accuracy_(clock_accuracy),
      adjustment_time_(adjustment_time),
      rate_(rate),
      channels_(channels),
      input_clock_smoother_(new ClockSmoother(clock_accuracy)),
      output_clock_smoother_(new ClockSmoother(clock_accuracy)),
      running_(false),
      position_(0),
      previous_requested_samples_(0),
      resampler_(
          channels,
          1.0,
          96,
          base::Bind(&AudioShifter::ResamplerCallback, base::Unretained(this))),
      current_ratio_(1.0) {}

AudioShifter::~AudioShifter() = default;

void AudioShifter::Push(std::unique_ptr<AudioBus> input,
                        base::TimeTicks playout_time) {
  TRACE_EVENT1("audio", "AudioShifter::Push", "time (ms)",
               (playout_time - base::TimeTicks()).InMillisecondsF());
  if (!queue_.empty()) {
    playout_time = input_clock_smoother_->Smooth(
        playout_time,
        base::TimeDelta::FromSeconds(queue_.back().audio->frames()) / rate_);
  }
  queue_.push_back(AudioQueueEntry(playout_time, std::move(input)));
  while (!queue_.empty() &&
         queue_.back().target_playout_time -
         queue_.front().target_playout_time > max_buffer_size_) {
    DVLOG(1) << "AudioShifter: Audio overflow!";
    queue_.pop_front();
    position_ = 0;
  }
}

void AudioShifter::Pull(AudioBus* output,
                        base::TimeTicks playout_time) {
  TRACE_EVENT1("audio", "AudioShifter::Pull", "time (ms)",
               (playout_time - base::TimeTicks()).InMillisecondsF());
  // Add the kernel size since we incur some internal delay in
  // resampling. All resamplers incur some delay, and for the
  // SincResampler (used by MultiChannelResampler), this is
  // (currently) kKernalSize / 2 frames.
  playout_time += base::TimeDelta::FromSeconds(
      SincResampler::kKernelSize) / rate_ / 2;
  playout_time = output_clock_smoother_->Smooth(
      playout_time,
      base::TimeDelta::FromSeconds(previous_requested_samples_) / rate_);
  previous_requested_samples_ = output->frames();

  base::TimeTicks stream_time;
  base::TimeTicks buffer_end_time;
  if (queue_.empty()) {
    DCHECK_EQ(position_, 0UL);
    stream_time = end_of_last_consumed_audiobus_;
    buffer_end_time = end_of_last_consumed_audiobus_;
  } else {
    stream_time = queue_.front().target_playout_time;
    buffer_end_time = queue_.back().target_playout_time;
  }
  stream_time += base::TimeDelta::FromSecondsD(
      (position_ - resampler_.BufferedFrames()) / rate_);

  if (!running_ &&
      base::TimeDelta::FromSeconds(output->frames() * 2) / rate_ +
      clock_accuracy_ > buffer_end_time - stream_time) {
    // We're not running right now, and we don't really have enough data
    // to satisfy output reliably. Wait.
    Zero(output);
    return;
  }
  if (playout_time < stream_time -
      base::TimeDelta::FromSeconds(output->frames()) / rate_ / 2 -
      (running_ ? clock_accuracy_ : base::TimeDelta())) {
    // |playout_time| is too far before the earliest known audio sample.
    Zero(output);
    return;
  }

  if (buffer_end_time < playout_time) {
    // If the "playout_time" is actually capture time, then
    // the entire queue will be in the past. Since we cannot
    // play audio in the past. We add one buffer size to the
    // bias to avoid buffer underruns in the future.
    if (bias_.is_zero()) {
      bias_ = playout_time - stream_time +
          clock_accuracy_ +
          base::TimeDelta::FromSeconds(output->frames()) / rate_;
    }
    stream_time += bias_;
  } else {
    // Normal case, some part of the queue is
    // ahead of the scheduled playout time.

    // Skip any data that is simply too old, if we have
    // better data somewhere in the qeueue.

    // Reset bias
    bias_ = base::TimeDelta();

    while (!queue_.empty() &&
           playout_time - stream_time > clock_accuracy_) {
      queue_.pop_front();
      position_ = 0;
      resampler_.Flush();
      if (queue_.empty()) {
        Zero(output);
        return;
      }
      stream_time = queue_.front().target_playout_time;
    }
  }

  running_ = true;
  double steady_ratio = output_clock_smoother_->Rate() /
      input_clock_smoother_->Rate();
  double time_difference = (playout_time - stream_time).InSecondsF();
  double adjustment_time = adjustment_time_.InSecondsF();
  // This is the ratio we would need to get perfect sync after
  // |adjustment_time| has passed.
  double slow_ratio = steady_ratio + time_difference / adjustment_time;
  slow_ratio = std::max(0.9, std::min(1.1, slow_ratio));
  adjustment_time = output->frames() / static_cast<double>(rate_);
  // This is ratio we we'd need get perfect sync at the end of the
  // current output audiobus.
  double fast_ratio = steady_ratio + time_difference / adjustment_time;
  fast_ratio = std::max(0.9, std::min(1.1, fast_ratio));

  // If the current ratio is somewhere between the slow and the fast
  // ratio, then keep it. This means we don't have to recalculate the
  // tables very often and also allows us to converge on good sync faster.
  if (!between(current_ratio_, slow_ratio, fast_ratio)) {
    // Check if the direction has changed.
    if ((current_ratio_ < steady_ratio) == (slow_ratio < steady_ratio)) {
      // Two possible scenarios:
      // Either we're really close to perfect sync, but the current ratio
      // would overshoot, or the current ratio is insufficient to get to
      // perfect sync in the alloted time. Clamp.
      double max_ratio = std::max(fast_ratio, slow_ratio);
      double min_ratio = std::min(fast_ratio, slow_ratio);
      current_ratio_ = std::min(max_ratio,
                                std::max(min_ratio, current_ratio_));
    } else {
      // The "direction" has changed. (From speed up to slow down or
      // vice versa, so we just take the slow ratio.
      current_ratio_ = slow_ratio;
    }
    resampler_.SetRatio(current_ratio_);
  }
  resampler_.Resample(output->frames(), output);
}

void AudioShifter::ResamplerCallback(int frame_delay, AudioBus* destination) {
  // TODO(hubbe): Use frame_delay
  int pos = 0;
  while (pos < destination->frames() && !queue_.empty()) {
    size_t to_copy = std::min<size_t>(
        queue_.front().audio->frames() - position_,
        destination->frames() - pos);
    CHECK_GT(to_copy, 0UL);
    queue_.front().audio->CopyPartialFramesTo(position_,
                                              to_copy,
                                              pos,
                                              destination);
    pos += to_copy;
    position_ += to_copy;
    if (position_ >= static_cast<size_t>(queue_.front().audio->frames())) {
      end_of_last_consumed_audiobus_ = queue_.front().target_playout_time +
          base::TimeDelta::FromSeconds(queue_.front().audio->frames()) / rate_;
      position_ -= queue_.front().audio->frames();
      queue_.pop_front();
    }
  }

  if (pos < destination->frames()) {
    // Underflow
    running_ = false;
    position_ = 0;
    previous_playout_time_ = base::TimeTicks();
    bias_ = base::TimeDelta();
    destination->ZeroFramesPartial(pos, destination->frames() - pos);
  }
}

void AudioShifter::Zero(AudioBus* output) {
  output->Zero();
  running_ = false;
  previous_playout_time_ = base::TimeTicks();
  bias_ = base::TimeDelta();
}

}  // namespace media
