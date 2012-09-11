// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AUDIO_CAPTURER_H_
#define REMOTING_HOST_AUDIO_CAPTURER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"

namespace remoting {

class AudioPacket;

class AudioCapturer {
 public:
  typedef base::Callback<void(scoped_ptr<AudioPacket> packet)>
      PacketCapturedCallback;

  virtual ~AudioCapturer() {}

  // Returns true if audio capturing is supported on this platform. If this
  // returns true, then Create() must not return NULL.
  static bool IsSupported();
  static scoped_ptr<AudioCapturer> Create();

  // Capturers should sample at a 44.1 or 48 kHz sampling rate, in uncompressed
  // PCM stereo format. Capturers may choose the number of frames per packet.
  // Returns true on success.
  virtual bool Start(const PacketCapturedCallback& callback) = 0;
  // Stops the audio capturer, and frees the OS-specific audio capture
  // resources.
  virtual void Stop() = 0;
  // Returns true if the audio capturer is running.
  virtual bool IsStarted() = 0;

  static bool IsValidSampleRate(int sample_rate);
};

}  // namespace remoting

#endif  // REMOTING_HOST_AUDIO_CAPTURER_H_
