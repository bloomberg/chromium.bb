// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_REDIRECTED_AUDIO_OUTPUT_H_
#define CHROMECAST_PUBLIC_MEDIA_REDIRECTED_AUDIO_OUTPUT_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "chromecast_export.h"
#include "volume_control.h"

namespace chromecast {
namespace media {

// Config for audio output redirection.
struct AudioOutputRedirectionConfig {
  AudioOutputRedirectionConfig();
  AudioOutputRedirectionConfig(const AudioOutputRedirectionConfig&);
  ~AudioOutputRedirectionConfig();

  // The number of output channels to send to the redirected output.
  int num_output_channels = 2;

  // The order of this redirector (used to determine which output receives the
  // audio stream, if more than one redirection applies to a single stream).
  int order = 0;

  // Whether or not to apply the normal volume attenuation to the stream
  // that is being redirected.
  bool apply_volume = false;

  // Any extra delay to apply to the timestamps sent to the redirected output.
  // Note that the delayed timestamp will be used internally for AV sync.
  int64_t extra_delay_microseconds = 0;

  // Patterns to determine which audio streams should be redirected. If a stream
  // has the same content type and the device ID matches the glob-style
  // device ID pattern of any entry in this list, that stream will be
  // redirected.
  std::vector<std::pair<AudioContentType, std::string /* device ID pattern */>>
      stream_match_patterns;
};

inline AudioOutputRedirectionConfig::AudioOutputRedirectionConfig() = default;
inline AudioOutputRedirectionConfig::AudioOutputRedirectionConfig(
    const AudioOutputRedirectionConfig&) = default;
inline AudioOutputRedirectionConfig::~AudioOutputRedirectionConfig() = default;

// Interface for redirected audio output. Audio from specific streams can be
// redirected to a different output (audio from those streams will not play out
// of the device's normal output).
class RedirectedAudioOutput {
 public:
  virtual ~RedirectedAudioOutput() = default;

  // Called when the mixer is starting to output data at the given
  // |output_samples_per_second| sample rate. WriteBuffer() will be not be
  // called until after Start() has been called.
  virtual void Start(int output_samples_per_second) = 0;

  // Called when the mixer is stopping audio output. No further calls to
  // WriteBuffer() will be made until Start() is called again (potentially with
  // a different sample rate).
  virtual void Stop() = 0;

  // Writes an audio buffer to the redirected output. |num_inputs| is the number
  // of input streams that were mixed together into the buffer (may be 0).
  // |channel data| contains the actual audio data (one buffer per channel),
  // with each buffer containing |num_frames| floats. Note that the number of
  // channels is determined by the |num_output_channels| construction arg.
  virtual void WriteBuffer(int num_inputs,
                           float** channel_data,
                           int num_frames,
                           int64_t output_timestamp) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_REDIRECTED_AUDIO_OUTPUT_H_
