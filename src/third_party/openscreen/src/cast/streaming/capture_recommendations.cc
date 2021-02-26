// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/capture_recommendations.h"

#include <algorithm>
#include <utility>

#include "cast/streaming/answer_messages.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {
namespace capture_recommendations {
namespace {

bool DoubleEquals(double a, double b) {
  // Choice of epsilon for double comparison allows for proper comparison
  // for both aspect ratios and frame rates. For frame rates, it is based on the
  // broadcast rate of 29.97fps, which is actually 29.976. For aspect ratios, it
  // allows for a one-pixel difference at a 4K resolution, we want it to be
  // relatively high to avoid false negative comparison results.
  const double kEpsilon = .0001;
  return std::abs(a - b) < kEpsilon;
}

void ApplyDisplay(const DisplayDescription& description,
                  Recommendations* recommendations) {
  recommendations->video.supports_scaling =
      (description.aspect_ratio_constraint &&
       (description.aspect_ratio_constraint.value() ==
        AspectRatioConstraint::kVariable));

  // We should never exceed the display's resolution, since it will always
  // force scaling.
  if (description.dimensions) {
    const double frame_rate =
        static_cast<double>(description.dimensions->frame_rate);
    recommendations->video.maximum =
        Resolution{description.dimensions->width,
                   description.dimensions->height, frame_rate};
    recommendations->video.bit_rate_limits.maximum =
        recommendations->video.maximum.effective_bit_rate();
    recommendations->video.minimum.set_minimum(recommendations->video.maximum);
  }

  // If the receiver gives us an aspect ratio that doesn't match the display
  // resolution they give us, the behavior is undefined from the spec.
  // Here we prioritize the aspect ratio, and the receiver can scale the frame
  // as they wish.
  double aspect_ratio = 0.0;
  if (description.aspect_ratio) {
    aspect_ratio = static_cast<double>(description.aspect_ratio->width) /
                   description.aspect_ratio->height;
#if OSP_DCHECK_IS_ON()
    if (description.dimensions) {
      const double from_dims =
          static_cast<double>(description.dimensions->width) /
          description.dimensions->height;
      if (!DoubleEquals(from_dims, aspect_ratio)) {
        OSP_DLOG_WARN << "Received mismatched aspect ratio from the receiver.";
      }
    }
#endif
    recommendations->video.maximum.width =
        recommendations->video.maximum.height * aspect_ratio;
  } else if (description.dimensions) {
    aspect_ratio = static_cast<double>(description.dimensions->width) /
                   description.dimensions->height;
  } else {
    return;
  }
  recommendations->video.minimum.width =
      recommendations->video.minimum.height * aspect_ratio;
}

Resolution ToResolution(const Dimensions& dims) {
  return {dims.width, dims.height, static_cast<double>(dims.frame_rate)};
}

void ApplyConstraints(const Constraints& constraints,
                      Recommendations* recommendations) {
  // Audio has no fields in the display description, so we can safely
  // ignore the current recommendations when setting values here.
  recommendations->audio.max_delay = constraints.audio.max_delay;
  recommendations->audio.max_channels = constraints.audio.max_channels;
  recommendations->audio.max_sample_rate = constraints.audio.max_sample_rate;

  recommendations->audio.bit_rate_limits = BitRateLimits{
      std::max(constraints.audio.min_bit_rate, kDefaultAudioMinBitRate),
      std::max(constraints.audio.max_bit_rate, kDefaultAudioMinBitRate)};

  // With video, we take the intersection of values of the constraints and
  // the display description.
  recommendations->video.max_delay = constraints.video.max_delay;
  recommendations->video.max_pixels_per_second =
      constraints.video.max_pixels_per_second;
  recommendations->video.bit_rate_limits =
      BitRateLimits{std::max(constraints.video.min_bit_rate,
                             recommendations->video.bit_rate_limits.minimum),
                    std::min(constraints.video.max_bit_rate,
                             recommendations->video.bit_rate_limits.maximum)};
  Resolution max = ToResolution(constraints.video.max_dimensions);
  if (max <= kDefaultMinResolution) {
    recommendations->video.maximum = kDefaultMinResolution;
  } else if (max < recommendations->video.maximum) {
    recommendations->video.maximum = std::move(max);
  }
  // Implicit else: maximum = kDefaultMaxResolution.

  if (constraints.video.min_dimensions) {
    Resolution min = ToResolution(constraints.video.min_dimensions.value());
    if (kDefaultMinResolution < min) {
      recommendations->video.minimum = std::move(min);
    }
  }
}

}  // namespace

bool BitRateLimits::operator==(const BitRateLimits& other) const {
  return std::tie(minimum, maximum) == std::tie(other.minimum, other.maximum);
}

bool Audio::operator==(const Audio& other) const {
  return std::tie(bit_rate_limits, max_delay, max_channels, max_sample_rate) ==
         std::tie(other.bit_rate_limits, other.max_delay, other.max_channels,
                  other.max_sample_rate);
}

bool Resolution::operator==(const Resolution& other) const {
  return (std::tie(width, height) == std::tie(other.width, other.height)) &&
         DoubleEquals(frame_rate, other.frame_rate);
}

bool Resolution::operator<(const Resolution& other) const {
  return effective_bit_rate() < other.effective_bit_rate();
}

bool Resolution::operator<=(const Resolution& other) const {
  return (*this == other) || (*this < other);
}

void Resolution::set_minimum(const Resolution& other) {
  if (other < *this) {
    *this = other;
  }
}

bool Video::operator==(const Video& other) const {
  return std::tie(bit_rate_limits, minimum, maximum, supports_scaling,
                  max_delay, max_pixels_per_second) ==
         std::tie(other.bit_rate_limits, other.minimum, other.maximum,
                  other.supports_scaling, other.max_delay,
                  other.max_pixels_per_second);
}

bool Recommendations::operator==(const Recommendations& other) const {
  return std::tie(audio, video) == std::tie(other.audio, other.video);
}

Recommendations GetRecommendations(const Answer& answer) {
  Recommendations recommendations;
  if (answer.display.has_value() && answer.display->IsValid()) {
    ApplyDisplay(answer.display.value(), &recommendations);
  }
  if (answer.constraints.has_value() && answer.constraints->IsValid()) {
    ApplyConstraints(answer.constraints.value(), &recommendations);
  }
  return recommendations;
}

}  // namespace capture_recommendations
}  // namespace cast
}  // namespace openscreen
