/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_TEMPLATE_LIBVPX_VP8_ADAPTER_H_
#define API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_TEMPLATE_LIBVPX_VP8_ADAPTER_H_

#include <memory>
#include <vector>

#include "modules/video_coding/codecs/vp8/include/vp8.h"

namespace webrtc {
struct LibvpxVp8EncoderTemplateAdapter {
  static std::vector<SdpVideoFormat> SupportedFormats() {
    return {SdpVideoFormat("VP8")};
  }

  static std::unique_ptr<VideoEncoder> CreateEncoder(
      const SdpVideoFormat& format) {
    return VP8Encoder::Create();
  }

  static bool IsScalabilityModeSupported(
      const absl::string_view scalability_mode) {
    return VP8Encoder::SupportsScalabilityMode(scalability_mode);
  }
};
}  // namespace webrtc

#endif  // API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_TEMPLATE_LIBVPX_VP8_ADAPTER_H_
