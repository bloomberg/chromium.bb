// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_AUDIO_DECODER_H_
#define REMOTING_CODEC_AUDIO_DECODER_H_

#include "base/memory/scoped_ptr.h"

namespace remoting {

namespace protocol {
class SessionConfig;
}  // namespace protocol

class AudioPacket;

class AudioDecoder {
 public:
  static scoped_ptr<AudioDecoder> CreateAudioDecoder(
      const protocol::SessionConfig& config);

  virtual ~AudioDecoder() {}

  // Returns the decoded packet. If the packet is invalid, then a NULL
  // scoped_ptr is returned.
  virtual scoped_ptr<AudioPacket> Decode(scoped_ptr<AudioPacket> packet) = 0;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_AUDIO_DECODER_H_
