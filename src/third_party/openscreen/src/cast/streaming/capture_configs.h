// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_CAPTURE_CONFIGS_H_
#define CAST_STREAMING_CAPTURE_CONFIGS_H_

#include <string>
#include <vector>

#include "cast/streaming/constants.h"

namespace openscreen {
namespace cast {

// A configuration set that can be used by the sender to capture audio, and the
// receiver to playback audio. Used by Cast Streaming to provide an offer to the
// receiver.
struct AudioCaptureConfig {
  // Audio codec represented by this configuration.
  AudioCodec codec = AudioCodec::kOpus;

  // Number of channels used by this configuration.
  int channels = kDefaultAudioChannels;

  // Average bit rate in bits per second used by this configuration. A value
  // of "zero" suggests that the bitrate should be automatically selected by
  // the sender.
  int bit_rate = 0;

  // Sample rate for audio RTP timebase.
  int sample_rate = kDefaultAudioSampleRate;

  // Target playout delay in milliseconds.
  std::chrono::milliseconds target_playout_delay = kDefaultTargetPlayoutDelay;
};

// Display resolution in pixels.
struct DisplayResolution {
  // Width in pixels.
  int width = 1920;

  // Height in pixels.
  int height = 1080;
};

// Frame rates are expressed as a rational number, and must be positive.
struct FrameRate {
  // For simple cases, the frame rate may be provided by simply setting the
  // number to the desired value, e.g. 30 or 60FPS. Some common frame rates like
  // 23.98 FPS (for NTSC compatibility) are represented as fractions, in this
  // case 24000/1001.
  int numerator = kDefaultFrameRate;
  int denominator = 1;
};

// A configuration set that can be used by the sender to capture video, as
// well as the receiver to playback video. Used by Cast Streaming to provide an
// offer to the receiver.
struct VideoCaptureConfig {
  // Video codec represented by this configuration.
  VideoCodec codec = VideoCodec::kVp8;

  // Maximum frame rate in frames per second.
  FrameRate max_frame_rate;

  // Number specifying the maximum bit rate for this stream. A value of
  // zero means that the maximum bit rate should be automatically selected by
  // the sender.
  int max_bit_rate = 0;

  // Resolutions to be offered to the receiver. At least one resolution
  // must be provided.
  std::vector<DisplayResolution> resolutions;

  // Target playout delay in milliseconds.
  std::chrono::milliseconds target_playout_delay = kDefaultTargetPlayoutDelay;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_CAPTURE_CONFIGS_H_
