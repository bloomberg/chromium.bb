// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/audio_player.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"

// The number of channels in the audio stream (only supporting stereo audio
// for now).
const int kChannels = 2;
const int kSampleSizeBytes = 2;

// If queue grows bigger than 150ms we start dropping packets.
const int kMaxQueueLatencyMs = 150;

namespace remoting {

AudioPlayer::AudioPlayer()
    : sampling_rate_(AudioPacket::SAMPLING_RATE_INVALID),
      start_failed_(false),
      queued_bytes_(0),
      bytes_consumed_(0) {
}

AudioPlayer::~AudioPlayer() {
  base::AutoLock auto_lock(lock_);
  ResetQueue();
}

void AudioPlayer::ProcessAudioPacket(scoped_ptr<AudioPacket> packet) {
  CHECK_EQ(1, packet->data_size());
  DCHECK_EQ(AudioPacket::ENCODING_RAW, packet->encoding());
  DCHECK_NE(AudioPacket::SAMPLING_RATE_INVALID, packet->sampling_rate());
  DCHECK_EQ(kSampleSizeBytes, packet->bytes_per_sample());
  DCHECK_EQ(static_cast<int>(kChannels), packet->channels());
  DCHECK_EQ(packet->data(0).size() % (kChannels * kSampleSizeBytes), 0u);

  // No-op if the Pepper player won't start.
  if (start_failed_) {
    return;
  }

  // Start the Pepper audio player if this is the first packet.
  if (sampling_rate_ != packet->sampling_rate()) {
    // Drop all packets currently in the queue, since they are sampled at the
    // wrong rate.
    {
      base::AutoLock auto_lock(lock_);
      ResetQueue();
    }

    sampling_rate_ = packet->sampling_rate();
    bool success = ResetAudioPlayer(sampling_rate_);
    if (!success) {
      start_failed_ = true;
      return;
    }
  }

  base::AutoLock auto_lock(lock_);

  queued_bytes_ += packet->data(0).size();
  queued_packets_.push_back(packet.release());

  int max_buffer_size_ =
      kMaxQueueLatencyMs * sampling_rate_ * kSampleSizeBytes * kChannels /
      base::Time::kMillisecondsPerSecond;
  while (queued_bytes_ > max_buffer_size_) {
    queued_bytes_ -= queued_packets_.front()->data(0).size() - bytes_consumed_;
    DCHECK_GE(queued_bytes_, 0);
    delete queued_packets_.front();
    queued_packets_.pop_front();
    bytes_consumed_ = 0;
  }
}

// static
void AudioPlayer::AudioPlayerCallback(void* samples,
                                      uint32 buffer_size,
                                      void* data) {
  AudioPlayer* audio_player = static_cast<AudioPlayer*>(data);
  audio_player->FillWithSamples(samples, buffer_size);
}

void AudioPlayer::ResetQueue() {
  lock_.AssertAcquired();
  STLDeleteElements(&queued_packets_);
  queued_bytes_ = 0;
  bytes_consumed_ = 0;
}

void AudioPlayer::FillWithSamples(void* samples, uint32 buffer_size) {
  base::AutoLock auto_lock(lock_);

  const size_t bytes_needed = kChannels * kSampleSizeBytes *
      GetSamplesPerFrame();

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
    queued_bytes_ -= bytes_to_copy;
    DCHECK_GE(queued_bytes_, 0);
  }
}

}  // namespace remoting
