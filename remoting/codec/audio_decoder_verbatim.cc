// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/audio_decoder_verbatim.h"

#include "base/logging.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

AudioDecoderVerbatim::AudioDecoderVerbatim() {
}

AudioDecoderVerbatim::~AudioDecoderVerbatim() {
}

scoped_ptr<AudioPacket> AudioDecoderVerbatim::Decode(
    scoped_ptr<AudioPacket> packet) {
  DCHECK_EQ(AudioPacket::ENCODING_RAW, packet->encoding());
  return packet.Pass();
}

}  // namespace remoting
