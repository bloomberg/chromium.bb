// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_audio_player.h"

#include <algorithm>

#include "base/stl_util.h"
#include "remoting/proto/audio.pb.h"

namespace {
// Constants used to create an audio configuration resource.
// The sample count we will request from the browser.
const uint32_t kSampleFrameCount = 4096u;
// The number of channels in the audio stream (only supporting stereo audio
// for now).
const uint32_t kChannels = 2u;
const int kSampleSizeBytes = 2;
}  // namespace

namespace remoting {

bool PepperAudioPlayer::IsRunning() const {
  return running_;
}

PepperAudioPlayer::PepperAudioPlayer(pp::Instance* instance)
      : samples_per_frame_(kSampleFrameCount),
        bytes_consumed_(0),
        running_(false) {
  // Ask the browser/device for an appropriate sample frame count size.
  samples_per_frame_ =
      pp::AudioConfig::RecommendSampleFrameCount(instance,
                                                 PP_AUDIOSAMPLERATE_44100,
                                                 kSampleFrameCount);

  // Create an audio configuration resource.
  pp::AudioConfig audio_config = pp::AudioConfig(instance,
                                                 PP_AUDIOSAMPLERATE_44100,
                                                 samples_per_frame_);

  // Create an audio resource.
  audio_ = pp::Audio(instance, audio_config, PepperAudioPlayerCallback, this);
}

PepperAudioPlayer::~PepperAudioPlayer() { }

bool PepperAudioPlayer::Start() {
  running_ = audio_.StartPlayback();
  return running_;
}

void PepperAudioPlayer::ProcessAudioPacket(scoped_ptr<AudioPacket> packet) {
  // TODO(kxing): Limit the size of the queue so that latency doesn't grow
  // too large.
  if (packet->data().size() % (kChannels * kSampleSizeBytes) != 0) {
    LOG(WARNING) << "Received corrupted packet.";
    return;
  }
  base::AutoLock auto_lock(lock_);

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
    if (queued_packets_.front()->data().size() == bytes_consumed_) {
      delete queued_packets_.front();
      queued_packets_.pop_front();
      bytes_consumed_ = 0;
      continue;
    }

    const std::string& packet_data = queued_packets_.front()->data();
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
