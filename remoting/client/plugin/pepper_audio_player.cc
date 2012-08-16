// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_audio_player.h"

#include <algorithm>

#include "base/stl_util.h"

namespace {
// Constants used to create an audio configuration resource.
// The sample count we will request from the browser.
const uint32_t kSampleFrameCount = 4096u;
// The number of channels in the audio stream (only supporting stereo audio
// for now).
const uint32_t kChannels = 2u;
const int kSampleSizeBytes = 2;

PP_AudioSampleRate ConvertToPepperSampleRate(
    remoting::AudioPacket::SamplingRate sampling_rate) {
  switch (sampling_rate) {
    case remoting::AudioPacket::SAMPLING_RATE_44100:
      return PP_AUDIOSAMPLERATE_44100;
    case remoting::AudioPacket::SAMPLING_RATE_48000:
      return PP_AUDIOSAMPLERATE_48000;
    default:
      NOTREACHED();
  }
  return PP_AUDIOSAMPLERATE_NONE;
}

}  // namespace

namespace remoting {

PepperAudioPlayer::PepperAudioPlayer(pp::Instance* instance)
    : instance_(instance),
      sampling_rate_(AudioPacket::SAMPLING_RATE_INVALID),
      samples_per_frame_(kSampleFrameCount),
      bytes_consumed_(0),
      start_failed_(false) {
}

PepperAudioPlayer::~PepperAudioPlayer() {}

bool PepperAudioPlayer::ResetAudioPlayer(
    AudioPacket::SamplingRate sampling_rate) {
  sampling_rate_ = sampling_rate;
  PP_AudioSampleRate sample_rate =
      ConvertToPepperSampleRate(sampling_rate);

  // Ask the browser/device for an appropriate sample frame count size.
  samples_per_frame_ =
      pp::AudioConfig::RecommendSampleFrameCount(instance_,
                                                 sample_rate,
                                                 kSampleFrameCount);

  // Create an audio configuration resource.
  pp::AudioConfig audio_config = pp::AudioConfig(instance_,
                                                 sample_rate,
                                                 samples_per_frame_);

  // Create an audio resource.
  audio_ = pp::Audio(instance_, audio_config, PepperAudioPlayerCallback, this);

  // Immediately start the player.
  bool success = audio_.StartPlayback();
  if (!success)
    LOG(ERROR) << "Failed to start Pepper audio player";
  return success;
}

void PepperAudioPlayer::ProcessAudioPacket(scoped_ptr<AudioPacket> packet) {
  // TODO(kxing): Limit the size of the queue so that latency doesn't grow
  // too large.

  // Drop null packets.
  if (!packet.get())
    return;

  CHECK_EQ(1, packet->data_size());
  DCHECK_EQ(AudioPacket::ENCODING_RAW, packet->encoding());
  if (packet->data(0).size() % (kChannels * kSampleSizeBytes) != 0) {
    LOG(WARNING) << "Received corrupted packet.";
    return;
  }
  base::AutoLock auto_lock(lock_);

  // No-op if the Pepper player won't start.
  if (start_failed_) {
    return;
  }

  // Start the Pepper audio player if this is the first packet.
  if (sampling_rate_ != packet->sampling_rate()) {
    // Drop all packets currently in the queue, since they are sampled at the
    // wrong rate.
    STLDeleteElements(&queued_packets_);

    bool success = ResetAudioPlayer(packet->sampling_rate());
    if (!success) {
      start_failed_ = true;
      return;
    }
  }

  queued_packets_.push_back(packet.release());
}

// static
void PepperAudioPlayer::PepperAudioPlayerCallback(void* samples,
                                                  uint32_t buffer_size,
                                                  void* data) {
  PepperAudioPlayer* audio_player = static_cast<PepperAudioPlayer*>(data);
  audio_player->FillWithSamples(samples, buffer_size);
}

void PepperAudioPlayer::FillWithSamples(void* samples, uint32_t buffer_size) {
  base::AutoLock auto_lock(lock_);

  const size_t bytes_needed = kChannels * samples_per_frame_ *
      kSampleSizeBytes;

  // Make sure we don't overrun the buffer.
  CHECK_EQ(buffer_size, bytes_needed);

  char* next_sample = static_cast<char*>(samples);
  size_t bytes_extracted = 0;

  while (bytes_extracted < bytes_needed) {
    // Check if we've run out of samples for this packet.
    if (queued_packets_.empty()) {
      memset(next_sample, 0, bytes_needed - bytes_extracted);
      return;
    }

    // Pop off the packet if we've already consumed all its bytes.
    if (queued_packets_.front()->data(0).size() == bytes_consumed_) {
      delete queued_packets_.front();
      queued_packets_.pop_front();
      bytes_consumed_ = 0;
      continue;
    }

    const std::string& packet_data = queued_packets_.front()->data(0);
    size_t bytes_to_copy = std::min(
        packet_data.size() - bytes_consumed_,
        bytes_needed - bytes_extracted);
    memcpy(next_sample, packet_data.data() + bytes_consumed_, bytes_to_copy);

    next_sample += bytes_to_copy;
    bytes_consumed_ += bytes_to_copy;
    bytes_extracted += bytes_to_copy;
  }
}

}  // namespace remoting
