// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_AUDIO_DECODER_VERBATIM_H_
#define REMOTING_CODEC_AUDIO_DECODER_VERBATIM_H_

#include <memory>

#include "base/macros.h"
#include "remoting/codec/audio_decoder.h"

namespace remoting {

class AudioPacket;

// The decoder for raw audio streams.
class AudioDecoderVerbatim : public AudioDecoder {
 public:
  AudioDecoderVerbatim();
  ~AudioDecoderVerbatim() override;

  std::unique_ptr<AudioPacket> Decode(
      std::unique_ptr<AudioPacket> packet) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDecoderVerbatim);
};

}  // namespace remoting

#endif  // REMOTING_CODEC_AUDIO_DECODER_VERBATIM_H_
