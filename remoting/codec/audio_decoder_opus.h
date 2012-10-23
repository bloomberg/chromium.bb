// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_AUDIO_DECODER_OPUS_H_
#define REMOTING_CODEC_AUDIO_DECODER_OPUS_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/codec/audio_decoder.h"

struct OpusDecoder;

namespace remoting {

class AudioPacket;

class AudioDecoderOpus : public AudioDecoder {
 public:
  AudioDecoderOpus();
  virtual ~AudioDecoderOpus();

  // AudioDecoder interface.
  virtual scoped_ptr<AudioPacket> Decode(
      scoped_ptr<AudioPacket> packet) OVERRIDE;

 private:
  void InitDecoder();
  void DestroyDecoder();
  bool ResetForPacket(AudioPacket* packet);

  int sampling_rate_;
  int channels_;
  OpusDecoder* decoder_;

  DISALLOW_COPY_AND_ASSIGN(AudioDecoderOpus);
};

}  // namespace remoting

#endif  // REMOTING_CODEC_AUDIO_DECODER_OPUS_H_
