// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/audio_decoder.h"

#include <vector>
#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/time.h"
#include "media/base/audio_bus.h"
#include "media/base/limits.h"
#include "media/filters/audio_file_reader.h"
#include "media/filters/in_memory_url_protocol.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebAudioBus.h"

using media::AudioBus;
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
  size_t number_of_frames = static_cast<size_t>(reader.number_of_frames());

  // Apply sanity checks to make sure crazy values aren't coming out of
  // FFmpeg.
  if (!number_of_channels ||
      number_of_channels > static_cast<size_t>(media::limits::kMaxChannels) ||
      file_sample_rate < media::limits::kMinSampleRate ||
      file_sample_rate > media::limits::kMaxSampleRate)
    return false;

  // Allocate and configure the output audio channel data.
  destination_bus->initialize(number_of_channels,
                              number_of_frames,
                              file_sample_rate);

  // Wrap the channel pointers which will receive the decoded PCM audio.
  vector<float*> audio_data;
  audio_data.reserve(number_of_channels);
  for (size_t i = 0; i < number_of_channels; ++i) {
    audio_data.push_back(destination_bus->channelData(i));
  }

  scoped_ptr<AudioBus> audio_bus = AudioBus::WrapVector(
      number_of_frames, audio_data);

  // Decode the audio file data.
  // TODO(crogers): If our estimate was low, then we still may fail to read
  // all of the data from the file.
  size_t actual_frames = reader.Read(audio_bus.get());

  // Adjust WebKit's bus to account for the actual file length
  // and valid data read.
  if (actual_frames != number_of_frames) {
    DCHECK_LE(actual_frames, number_of_frames);
    destination_bus->resizeSmaller(actual_frames);
  }

  double duration = actual_frames / file_sample_rate;

  DVLOG(1) << "Decoded file data -"
           << " data: " << data
           << " data size: " << data_size
           << " duration: " << duration
           << " number of frames: " << actual_frames
           << " sample rate: " << file_sample_rate
           << " number of channels: " << number_of_channels;

  return actual_frames > 0;
}

}  // namespace webkit_media
