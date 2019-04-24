/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/vp9/include/vp9.h"

#include "absl/memory/memory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "modules/video_coding/codecs/vp9/vp9_impl.h"
#include "rtc_base/checks.h"
#include "vpx/vp8cx.h"
#include "vpx/vp8dx.h"
#include "vpx/vpx_codec.h"

namespace webrtc {

std::vector<SdpVideoFormat> SupportedVP9Codecs() {
#ifdef RTC_ENABLE_VP9
  // Profile 2 might not be available on some platforms until
  // https://bugs.chromium.org/p/webm/issues/detail?id=1544 is solved.
  static bool vpx_supports_high_bit_depth =
      (vpx_codec_get_caps(vpx_codec_vp9_cx()) & VPX_CODEC_CAP_HIGHBITDEPTH) !=
          0 &&
      (vpx_codec_get_caps(vpx_codec_vp9_dx()) & VPX_CODEC_CAP_HIGHBITDEPTH) !=
          0;

  std::vector<SdpVideoFormat> supported_formats{SdpVideoFormat(
      cricket::kVp9CodecName,
      {{kVP9FmtpProfileId, VP9ProfileToString(VP9Profile::kProfile0)}})};
  if (vpx_supports_high_bit_depth) {
    supported_formats.push_back(SdpVideoFormat(
        cricket::kVp9CodecName,
        {{kVP9FmtpProfileId, VP9ProfileToString(VP9Profile::kProfile2)}}));
  }
  return supported_formats;
#else
  return std::vector<SdpVideoFormat>();
#endif
}

std::unique_ptr<VP9Encoder> VP9Encoder::Create() {
#ifdef RTC_ENABLE_VP9
  return absl::make_unique<VP9EncoderImpl>(cricket::VideoCodec());
#else
  RTC_NOTREACHED();
  return nullptr;
#endif
}

std::unique_ptr<VP9Encoder> VP9Encoder::Create(
    const cricket::VideoCodec& codec) {
#ifdef RTC_ENABLE_VP9
  return absl::make_unique<VP9EncoderImpl>(codec);
#else
  RTC_NOTREACHED();
  return nullptr;
#endif
}

std::unique_ptr<VP9Decoder> VP9Decoder::Create() {
#ifdef RTC_ENABLE_VP9
  return absl::make_unique<VP9DecoderImpl>();
#else
  RTC_NOTREACHED();
  return nullptr;
#endif
}

}  // namespace webrtc
