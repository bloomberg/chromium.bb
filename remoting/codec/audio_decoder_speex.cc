// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/audio_decoder_speex.h"

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "remoting/proto/audio.pb.h"
#include "third_party/speex/include/speex/speex_callbacks.h"
#include "third_party/speex/include/speex/speex_stereo.h"

namespace remoting {

namespace {

// Hosts will never generate more than 100 frames in a single packet.
const int kMaxFramesPerPacket = 100;

}  // namespace

AudioDecoderSpeex::AudioDecoderSpeex() {
  // Create and initialize the Speex structures.
  speex_bits_.reset(new SpeexBits());
  speex_bits_init(speex_bits_.get());
  speex_state_ = speex_decoder_init(&speex_wb_mode);

  // Create and initialize the Speex stereo state.
  speex_stereo_state_ = speex_stereo_state_init();

  // Create and initialize the stereo callback.
  speex_callback_.reset(new SpeexCallback());
  speex_callback_->callback_id = SPEEX_INBAND_STEREO;
  speex_callback_->func = speex_std_stereo_request_handler;
  speex_callback_->data = speex_stereo_state_;

  int result;

  // Turn on perceptual enhancer, which will make the audio sound better,
  // at the price of further distorting the decoded samples.
  int enhancer = 1;
  result = speex_decoder_ctl(speex_state_, SPEEX_SET_ENH, &enhancer);
  CHECK_EQ(result, 0);

  // Get the frame size, so that we know the size of output when we decode
  // frame by frame.
  result = speex_decoder_ctl(speex_state_,
                             SPEEX_GET_FRAME_SIZE,
                             &speex_frame_size_);
  CHECK_EQ(result, 0);

  // Set the stereo callback, so that the Speex decoder can get the intensity
  // stereo information.
  result = speex_decoder_ctl(speex_state_,
                             SPEEX_SET_HANDLER,
                             speex_callback_.get());
  CHECK_EQ(result, 0);
}

AudioDecoderSpeex::~AudioDecoderSpeex() {
  speex_stereo_state_destroy(speex_stereo_state_);
  speex_decoder_destroy(speex_state_);
  speex_bits_destroy(speex_bits_.get());
}

scoped_ptr<AudioPacket> AudioDecoderSpeex::Decode(
    scoped_ptr<AudioPacket> packet) {
  if ((packet->encoding() != AudioPacket::ENCODING_SPEEX) ||
      (packet->bytes_per_sample() != AudioPacket::BYTES_PER_SAMPLE_2) ||
      (packet->sampling_rate() == AudioPacket::SAMPLING_RATE_INVALID) ||
      (packet->channels() != AudioPacket::CHANNELS_STEREO)) {
    LOG(WARNING) << "Received an unsupported packet.";
    return scoped_ptr<AudioPacket>(NULL);
  }
  if (packet->data_size() > kMaxFramesPerPacket) {
    LOG(WARNING) << "Received an packet with too many frames.";
    return scoped_ptr<AudioPacket>();
  }

  // Create a new packet of decoded data.
  scoped_ptr<AudioPacket> decoded_packet(new AudioPacket());
  decoded_packet->set_encoding(AudioPacket::ENCODING_RAW);
  decoded_packet->set_sampling_rate(packet->sampling_rate());
  decoded_packet->set_bytes_per_sample(packet->bytes_per_sample());
  decoded_packet->set_channels(packet->channels());

  std::string* decoded_data = decoded_packet->add_data();
  decoded_data->resize(packet->data_size() *
                       speex_frame_size_ *
                       packet->bytes_per_sample() *
                       packet->channels());
  int16* samples = reinterpret_cast<int16*>(string_as_array(decoded_data));

  for (int i = 0; i < packet->data_size(); ++i) {
    // Read the bytes into the bits structure.
    speex_bits_read_from(speex_bits_.get(),
                         string_as_array(packet->mutable_data(i)),
                         packet->data(i).size());

    // Decode the frame and store it in the buffer.
    int status = speex_decode_int(speex_state_, speex_bits_.get(), samples);
    if (status < 0) {
      LOG(ERROR) << "Error in decoding Speex data.";
      return scoped_ptr<AudioPacket>(NULL);
    }
    // Transform mono to stereo.
    speex_decode_stereo_int(samples, speex_frame_size_, speex_stereo_state_);

    samples += (speex_frame_size_ * packet->channels());
  }

  return decoded_packet.Pass();
}

}  // namespace remoting
