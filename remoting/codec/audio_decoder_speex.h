// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_AUDIO_DECODER_SPEEX_H_
#define REMOTING_CODEC_AUDIO_DECODER_SPEEX_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/codec/audio_decoder.h"

struct SpeexBits;
struct SpeexCallback;
struct SpeexStereoState;

namespace remoting {

class AudioPacket;

class AudioDecoderSpeex : public AudioDecoder {
 public:
  AudioDecoderSpeex();
  virtual ~AudioDecoderSpeex();

  // AudioDecoder implementation.
  virtual scoped_ptr<AudioPacket> Decode(
      scoped_ptr<AudioPacket> packet) OVERRIDE;

 private:
  scoped_ptr<SpeexBits> speex_bits_;
  void* speex_state_;
  int speex_frame_size_;
  SpeexStereoState* speex_stereo_state_;
  scoped_ptr<SpeexCallback> speex_callback_;

  DISALLOW_COPY_AND_ASSIGN(AudioDecoderSpeex);
};

}  // namespace remoting

#endif  // REMOTING_CODEC_AUDIO_DECODER_SPEEX_H_
