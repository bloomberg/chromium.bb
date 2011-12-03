// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/audio_decoder.h"

#include <vector>
#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/time.h"
#include "media/base/limits.h"
#include "media/filters/audio_file_reader.h"
#include "media/filters/in_memory_url_protocol.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebAudioBus.h"

using media::AudioFileReader;
using media::InMemoryUrlProtocol;
using std::vector;
using WebKit::WebAudioBus;

namespace webkit_media {

// Decode in-memory audio file data.
bool DecodeAudioFileData(
    WebKit::WebAudioBus* destination_bus,
    const char* data, size_t data_size, double sample_rate) {
  DCHECK(destination_bus);
  if (!destination_bus)
    return false;

  // Uses the FFmpeg library for audio file reading.
  InMemoryUrlProtocol url_protocol(reinterpret_cast<const uint8*>(data),
                                   data_size, false);
  AudioFileReader reader(&url_protocol);

  if (!reader.Open())
    return false;

  size_t number_of_channels = reader.channels();
  double file_sample_rate = reader.sample_rate();
  double duration = reader.duration().InSecondsF();
  size_t number_of_frames = static_cast<size_t>(reader.number_of_frames());

  // Apply sanity checks to make sure crazy values aren't coming out of
  // FFmpeg.
  if (!number_of_channels ||
      number_of_channels > static_cast<size_t>(media::Limits::kMaxChannels) ||
      file_sample_rate < media::Limits::kMinSampleRate ||
      file_sample_rate > media::Limits::kMaxSampleRate)
    return false;

  // TODO(crogers) : do sample-rate conversion with FFmpeg.
  // For now, we're ignoring the requested 'sample_rate' and returning
  // the WebAudioBus at the file's sample-rate.
  // double destination_sample_rate =
  //   (sample_rate != 0.0) ? sample_rate : file_sample_rate;
  double destination_sample_rate = file_sample_rate;

  DLOG(INFO) << "Decoding file data -"
      << " data: " << data
      << " data size: " << data_size
      << " duration: " << duration
      << " number of frames: " << number_of_frames
      << " sample rate: " << file_sample_rate
      << " number of channels: " << number_of_channels;

  // Change to destination sample-rate.
  number_of_frames = static_cast<size_t>(number_of_frames *
      (destination_sample_rate / file_sample_rate));

  // Allocate and configure the output audio channel data.
  destination_bus->initialize(number_of_channels,
                              number_of_frames,
                              destination_sample_rate);

  // Wrap the channel pointers which will receive the decoded PCM audio.
  vector<float*> audio_data;
  audio_data.reserve(number_of_channels);
  for (size_t i = 0; i < number_of_channels; ++i) {
    audio_data.push_back(destination_bus->channelData(i));
  }

  // Decode the audio file data.
  return reader.Read(audio_data, number_of_frames);
}

}  // namespace webkit_media
