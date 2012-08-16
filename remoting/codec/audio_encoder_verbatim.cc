// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/audio_encoder_verbatim.h"

#include "base/logging.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

AudioEncoderVerbatim::AudioEncoderVerbatim() {}

AudioEncoderVerbatim::~AudioEncoderVerbatim() {}

scoped_ptr<AudioPacket> AudioEncoderVerbatim::Encode(
    scoped_ptr<AudioPacket> packet) {
  DCHECK_EQ(AudioPacket::ENCODING_RAW, packet->encoding());
  DCHECK_EQ(1, packet->data_size());
  return packet.Pass();
}

}  // namespace remoting
