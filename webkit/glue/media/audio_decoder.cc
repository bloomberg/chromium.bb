// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/media/audio_decoder.h"

#include <vector>
#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/time.h"
#include "media/filters/audio_file_reader.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAudioBus.h"

using media::AudioFileReader;
using media::InMemoryDataReader;
using std::vector;
using WebKit::WebAudioBus;

namespace webkit_glue {

// Decode in-memory audio file data.
bool DecodeAudioFileData(
    WebKit::WebAudioBus* destination_bus,
    const char* data, size_t data_size, double sample_rate) {
  DCHECK(destination_bus);
  if (!destination_bus)
    return false;

  // Uses the FFmpeg library for audio file reading.
  InMemoryDataReader data_reader(data, data_size);
  AudioFileReader reader(&data_reader);

  if (!reader.Open())
    return false;

  size_t number_of_channels = reader.channels();
  double file_sample_rate = reader.sample_rate();
  double duration = reader.duration().InSecondsF();
  size_t number_of_frames = static_cast<size_t>(reader.number_of_frames());

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

}  // namespace webkit_glue
