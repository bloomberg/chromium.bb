// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_AUDIO_ENCODER_VERBATIM_H_
#define REMOTING_CODEC_AUDIO_ENCODER_VERBATIM_H_

#include "remoting/codec/audio_encoder.h"

namespace remoting {

class AudioPacket;

class AudioEncoderVerbatim : public AudioEncoder {
 public:
  AudioEncoderVerbatim();
  virtual ~AudioEncoderVerbatim();

  // AudioEncoder implementation.
  virtual scoped_ptr<AudioPacket> Encode(
      scoped_ptr<AudioPacket> packet) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioEncoderVerbatim);
};

}  // namespace remoting

#endif  // REMOTING_CODEC_AUDIO_ENCODER_VERBATIM_H_
