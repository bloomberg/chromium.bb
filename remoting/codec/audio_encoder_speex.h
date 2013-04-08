// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_AUDIO_ENCODER_SPEEX_H_
#define REMOTING_CODEC_AUDIO_ENCODER_SPEEX_H_

#include <list>
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/codec/audio_encoder.h"

struct SpeexBits;

namespace remoting {

class AudioPacket;

class AudioEncoderSpeex : public AudioEncoder {
 public:
  AudioEncoderSpeex();
  virtual ~AudioEncoderSpeex();

  // AudioEncoder implementation.
  virtual scoped_ptr<AudioPacket> Encode(
      scoped_ptr<AudioPacket> packet) OVERRIDE;

 private:
  scoped_ptr<SpeexBits> speex_bits_;
  void* speex_state_;
  int speex_frame_size_;

  scoped_ptr<int16[]> leftover_buffer_;
  // We may have some left-over unencoded frames from the previous AudioPacket.
  int leftover_frames_;

  DISALLOW_COPY_AND_ASSIGN(AudioEncoderSpeex);
};

}  // namespace remoting

#endif  // REMOTING_CODEC_AUDIO_ENCODER_SPEEX_H_
