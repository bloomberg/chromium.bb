// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_AUDIO_CONSUMER_H_
#define REMOTING_CLIENT_AUDIO_CONSUMER_H_

#include <memory>

namespace remoting {

class AudioPacket;

class AudioConsumer {
 public:
  virtual void AddAudioPacket(std::unique_ptr<AudioPacket> packet) = 0;

 protected:
  virtual ~AudioConsumer() {}
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_AUDIO_CONSUMER_H_
