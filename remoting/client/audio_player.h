// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_AUDIO_PLAYER_H_
#define REMOTING_CLIENT_AUDIO_PLAYER_H_

#include <list>

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

class AudioPlayer {
 public:
  virtual ~AudioPlayer();

  void ProcessAudioPacket(scoped_ptr<AudioPacket> packet);

 protected:
  AudioPlayer();

  // Return the recommended number of samples to include in a frame.
  virtual uint32 GetSamplesPerFrame() = 0;

  // Resets the audio player and starts playback.
  // Returns true on success.
  virtual bool ResetAudioPlayer(AudioPacket::SamplingRate sampling_rate) = 0;

  // Function called by the browser when it needs more audio samples.
  static void AudioPlayerCallback(void* samples,
                                  uint32 buffer_size,
                                  void* data);

 private:
  friend class AudioPlayerTest;

  typedef std::list<AudioPacket*> AudioPacketQueue;

  void ResetQueue();
  void FillWithSamples(void* samples, uint32 buffer_size);

  AudioPacket::SamplingRate sampling_rate_;

  bool start_failed_;

  // Protects |queued_packets_|, |queued_samples_ and |bytes_consumed_|. This is
  // necessary to prevent races, because Pepper will call the  callback on a
  // separate thread.
  base::Lock lock_;

  AudioPacketQueue queued_packets_;
  int queued_bytes_;

  // The number of bytes from |queued_packets_| that have been consumed.
  size_t bytes_consumed_;

  DISALLOW_COPY_AND_ASSIGN(AudioPlayer);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_AUDIO_PLAYER_H_
