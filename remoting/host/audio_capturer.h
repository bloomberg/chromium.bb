// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AUDIO_CAPTURER_H_
#define REMOTING_HOST_AUDIO_CAPTURER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

class AudioCapturer {
 public:
  typedef base::Callback<void(scoped_ptr<AudioPacket> packet)>
      PacketCapturedCallback;

  virtual ~AudioCapturer() {}

  static scoped_ptr<AudioCapturer> Create();

  // Capturers should sample at a 44.1 kHz sampling rate, in uncompressed PCM
  // stereo format. Capturers may choose the number of frames per packet.
  virtual void Start(const PacketCapturedCallback& callback) = 0;
  virtual void Stop() = 0;
  virtual bool IsRunning() = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_AUDIO_CAPTURER_H_
