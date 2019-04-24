// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/als_samples.h"

#include <numeric>

#include "base/logging.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

AmbientLightSampleBuffer::AmbientLightSampleBuffer(base::TimeDelta horizon)
    : horizon_(horizon) {
  DCHECK(!horizon_.is_zero());
}

AmbientLightSampleBuffer::~AmbientLightSampleBuffer() = default;

void AmbientLightSampleBuffer::SaveToBuffer(
    const AmbientLightSampleBuffer::Sample& sample) {
  samples_.push_back(sample);
  Prune(sample.sample_time);
}

base::Optional<double> AmbientLightSampleBuffer::AverageAmbient(
    base::TimeTicks now) {
  const auto add_lux = [](double lux, const Sample& sample) {
    return lux + sample.lux;
  };

  Prune(now);

  if (samples_.empty()) {
    return base::nullopt;
  } else {
    return std::accumulate(samples_.begin(), samples_.end(), 0.0, add_lux) /
           samples_.size();
  }
}

size_t AmbientLightSampleBuffer::NumberOfSamples(base::TimeTicks now) {
  Prune(now);
  return samples_.size();
}

size_t AmbientLightSampleBuffer::NumberOfSamplesForTesting() const {
  return samples_.size();
}

void AmbientLightSampleBuffer::Prune(base::TimeTicks now) {
  while (!samples_.empty()) {
    if (now - samples_.front().sample_time < horizon_)
      return;

    samples_.pop_front();
  }
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
