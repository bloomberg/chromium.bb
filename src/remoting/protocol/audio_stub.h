// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_AUDIO_STUB_H_
#define REMOTING_PROTOCOL_AUDIO_STUB_H_

#include <memory>

#include "base/callback_forward.h"

namespace remoting {

class AudioPacket;

namespace protocol {

class AudioStub {
 public:
  AudioStub(const AudioStub&) = delete;
  AudioStub& operator=(const AudioStub&) = delete;

  virtual ~AudioStub() { }

  virtual void ProcessAudioPacket(std::unique_ptr<AudioPacket> audio_packet,
                                  base::OnceClosure done) = 0;

 protected:
  AudioStub() { }
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_AUDIO_STUB_H_
