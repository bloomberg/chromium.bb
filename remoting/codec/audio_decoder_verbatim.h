// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_AUDIO_DECODER_VERBATIM_H_
#define REMOTING_CODEC_AUDIO_DECODER_VERBATIM_H_

#include "remoting/codec/audio_decoder.h"

#include "base/memory/scoped_ptr.h"

namespace remoting {

class AudioPacket;

// The decoder for raw audio streams.
class AudioDecoderVerbatim : public AudioDecoder {
 public:
  AudioDecoderVerbatim();
  virtual ~AudioDecoderVerbatim();

  virtual scoped_ptr<AudioPacket> Decode(
      scoped_ptr<AudioPacket> packet) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDecoderVerbatim);
};

}  // namespace remoting

#endif  // REMOTING_CODEC_AUDIO_DECODER_VERBATIM_H_
