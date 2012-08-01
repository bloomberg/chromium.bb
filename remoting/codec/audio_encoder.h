// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_AUDIO_ENCODER_H_
#define REMOTING_CODEC_AUDIO_ENCODER_H_

#include "base/memory/scoped_ptr.h"

namespace remoting {

class AudioPacket;

class AudioEncoder {
 public:
  virtual ~AudioEncoder() {}

  virtual scoped_ptr<AudioPacket> Encode(scoped_ptr<AudioPacket> packet) = 0;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_AUDIO_ENCODER_H_
