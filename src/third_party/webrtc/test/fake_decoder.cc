/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/fake_decoder.h"

#include <string.h>

#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/time_utils.h"

namespace webrtc {
namespace test {

namespace {
const int kDefaultWidth = 320;
const int kDefaultHeight = 180;
}  // namespace

FakeDecoder::FakeDecoder()
    : callback_(NULL), width_(kDefaultWidth), height_(kDefaultHeight) {}

int32_t FakeDecoder::InitDecode(const VideoCodec* config,
                                int32_t number_of_cores) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeDecoder::Decode(const EncodedImage& input,
                            bool missing_frames,
                            const CodecSpecificInfo* codec_specific_info,
                            int64_t render_time_ms) {
  if (input._encodedWidth > 0 && input._encodedHeight > 0) {
    width_ = input._encodedWidth;
    height_ = input._encodedHeight;
  }

  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(I420Buffer::Create(width_, height_))
          .set_rotation(webrtc::kVideoRotation_0)
          .set_timestamp_ms(render_time_ms)
          .build();
  frame.set_timestamp(input.Timestamp());
  frame.set_ntp_time_ms(input.ntp_time_ms_);

  callback_->Decoded(frame);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeDecoder::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeDecoder::Release() {
  return WEBRTC_VIDEO_CODEC_OK;
}

const char* FakeDecoder::kImplementationName = "fake_decoder";
const char* FakeDecoder::ImplementationName() const {
  return kImplementationName;
}

int32_t FakeH264Decoder::Decode(const EncodedImage& input,
                                bool missing_frames,
                                const CodecSpecificInfo* codec_specific_info,
                                int64_t render_time_ms) {
  uint8_t value = 0;
  for (size_t i = 0; i < input.size(); ++i) {
    uint8_t kStartCode[] = {0, 0, 0, 1};
    if (i < input.size() - sizeof(kStartCode) &&
        !memcmp(&input.data()[i], kStartCode, sizeof(kStartCode))) {
      i += sizeof(kStartCode) + 1;  // Skip start code and NAL header.
    }
    if (input.data()[i] != value) {
      RTC_CHECK_EQ(value, input.data()[i])
          << "Bitstream mismatch between sender and receiver.";
      return -1;
    }
    ++value;
  }
  return FakeDecoder::Decode(input, missing_frames, codec_specific_info,
                             render_time_ms);
}

}  // namespace test
}  // namespace webrtc
