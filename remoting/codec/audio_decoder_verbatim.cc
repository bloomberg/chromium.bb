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
  // Return a null scoped_ptr if we get a corrupted packet.
  if ((packet->encoding() != AudioPacket::ENCODING_RAW) ||
      (packet->data_size() != 1) ||
      (packet->sampling_rate() == AudioPacket::SAMPLING_RATE_INVALID) ||
      (packet->bytes_per_sample() != AudioPacket::BYTES_PER_SAMPLE_2) ||
      (packet->channels() != AudioPacket::CHANNELS_STEREO) ||
      (packet->data(0).size() %
       (AudioPacket::CHANNELS_STEREO * AudioPacket::BYTES_PER_SAMPLE_2) != 0)) {
    LOG(WARNING) << "Verbatim decoder received an invalid packet.";
    return scoped_ptr<AudioPacket>();
  }
  return packet.Pass();
}

}  // namespace remoting
