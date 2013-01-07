// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/audio_encoder_speex.h"

#include <string>
#include <sstream>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "remoting/proto/audio.pb.h"
#include "third_party/speex/include/speex/speex_callbacks.h"
#include "third_party/speex/include/speex/speex_stereo.h"

namespace {
// A quality of 8 in wide band mode corresponds to 27,800 bits per second.
const int kSpeexHighQuality = 8;
const int kEncodedDataBufferSize = 0xFF;
}  // namespace

namespace remoting {

AudioEncoderSpeex::AudioEncoderSpeex()
    : leftover_frames_(0) {
  // Create and initialize the Speex structures.
  speex_bits_.reset(new SpeexBits());
  speex_bits_init(speex_bits_.get());
  speex_state_ = speex_encoder_init(&speex_wb_mode);

  // Set the encoding quality.
  int quality = kSpeexHighQuality;
  speex_encoder_ctl(speex_state_, SPEEX_SET_QUALITY, &quality);

  // Get the frame size and construct the input buffer accordingly.
  int result = speex_encoder_ctl(speex_state_,
                                 SPEEX_GET_FRAME_SIZE,
                                 &speex_frame_size_);
  CHECK_EQ(result, 0);

  leftover_buffer_.reset(
      new int16[speex_frame_size_ * AudioPacket::CHANNELS_STEREO]);
}

AudioEncoderSpeex::~AudioEncoderSpeex() {
  speex_encoder_destroy(speex_state_);
  speex_bits_destroy(speex_bits_.get());
}

scoped_ptr<AudioPacket> AudioEncoderSpeex::Encode(
    scoped_ptr<AudioPacket> packet) {
  DCHECK_EQ(AudioPacket::ENCODING_RAW, packet->encoding());
  DCHECK_EQ(1, packet->data_size());
  DCHECK_EQ(AudioPacket::BYTES_PER_SAMPLE_2, packet->bytes_per_sample());
  DCHECK_NE(AudioPacket::SAMPLING_RATE_INVALID, packet->sampling_rate());
  DCHECK_EQ(AudioPacket::CHANNELS_STEREO, packet->channels());

  int frames_left =
      packet->data(0).size() / packet->bytes_per_sample() / packet->channels();
  const int16* next_sample =
      reinterpret_cast<const int16*>(packet->data(0).data());

  // Create a new packet of encoded data.
  scoped_ptr<AudioPacket> encoded_packet(new AudioPacket());
  encoded_packet->set_encoding(AudioPacket::ENCODING_SPEEX);
  encoded_packet->set_sampling_rate(packet->sampling_rate());
  encoded_packet->set_bytes_per_sample(packet->bytes_per_sample());
  encoded_packet->set_channels(packet->channels());

  while (leftover_frames_ + frames_left >= speex_frame_size_) {
    int16* unencoded_buffer = NULL;
    int frames_consumed = 0;

    if (leftover_frames_ > 0) {
      unencoded_buffer = leftover_buffer_.get();
      frames_consumed = speex_frame_size_ - leftover_frames_;

      memcpy(leftover_buffer_.get() + leftover_frames_ * packet->channels(),
             next_sample,
             frames_consumed * packet->bytes_per_sample() * packet->channels());

      leftover_frames_ = 0;
    } else {
      unencoded_buffer = const_cast<int16*>(next_sample);
      frames_consumed = speex_frame_size_;
    }

    // Transform stereo to mono.
    speex_encode_stereo_int(unencoded_buffer,
                            speex_frame_size_,
                            speex_bits_.get());

    // Encode the frame, treating all samples as integers.
    speex_encode_int(speex_state_,
                     unencoded_buffer,
                     speex_bits_.get());

    next_sample += frames_consumed * packet->channels();
    frames_left -= frames_consumed;

    std::string* new_data = encoded_packet->add_data();
    new_data->resize(speex_bits_nbytes(speex_bits_.get()));

    // Copy the encoded data from the bits structure into the buffer.
    int bytes_written = speex_bits_write(speex_bits_.get(),
                                         string_as_array(new_data),
                                         new_data->size());

    // Expect that the bytes are all written.
    DCHECK_EQ(bytes_written, static_cast<int>(new_data->size()));

    // Reset the bits structure for this frame.
    speex_bits_reset(speex_bits_.get());
  }

  // Store the leftover samples.
  if (frames_left > 0) {
    CHECK_LE(leftover_frames_ + frames_left, speex_frame_size_);
    memcpy(leftover_buffer_.get() + leftover_frames_ * packet->channels(),
           next_sample,
           frames_left * packet->bytes_per_sample() * packet->channels());
    leftover_frames_ += frames_left;
  }

  // Return NULL if there's nothing in the packet.
  if (encoded_packet->data_size() == 0)
    return scoped_ptr<AudioPacket>();

  return encoded_packet.Pass();
}

}  // namespace remoting
