// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/audio_decoder.h"

#include "base/logging.h"
#include "remoting/codec/audio_decoder_opus.h"
#include "remoting/codec/audio_decoder_verbatim.h"
#include "remoting/protocol/session_config.h"

namespace remoting {

scoped_ptr<AudioDecoder> AudioDecoder::CreateAudioDecoder(
    const protocol::SessionConfig& config) {
  const protocol::ChannelConfig& audio_config = config.audio_config();

  if (audio_config.codec == protocol::ChannelConfig::CODEC_VERBATIM) {
    return scoped_ptr<AudioDecoder>(new AudioDecoderVerbatim());
  } else if (audio_config.codec == protocol::ChannelConfig::CODEC_OPUS) {
    return scoped_ptr<AudioDecoder>(new AudioDecoderOpus());
  }

  NOTIMPLEMENTED();
  return scoped_ptr<AudioDecoder>();
}

}  // namespace remoting
