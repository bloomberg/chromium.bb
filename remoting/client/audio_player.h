// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_AUDIO_PLAYER_H_
#define REMOTING_CLIENT_AUDIO_PLAYER_H_

#include "base/memory/scoped_ptr.h"

namespace pp {
class Instance;
}  // namespace pp

namespace remoting {

class AudioPacket;

class AudioPlayer {
 public:
  virtual ~AudioPlayer() {}

  // Returns true if successful, false otherwise.
  virtual bool Start() = 0;

  virtual void ProcessAudioPacket(scoped_ptr<AudioPacket> packet) = 0;

  virtual bool IsRunning() const = 0;

 protected:
  AudioPlayer() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioPlayer);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_AUDIO_PLAYER_H_
