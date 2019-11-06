// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_RTC_VIDEO_ENCODER_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_RTC_VIDEO_ENCODER_FACTORY_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "media/base/video_codecs.h"
#include "third_party/webrtc/api/video_codecs/video_encoder_factory.h"

namespace media {
class GpuVideoAcceleratorFactories;
}  // namespace media

namespace content {

// This class creates RTCVideoEncoder instances (each wrapping a
// media::VideoEncodeAccelerator) on behalf of the WebRTC stack.
class CONTENT_EXPORT RTCVideoEncoderFactory
    : public webrtc::VideoEncoderFactory {
 public:
  explicit RTCVideoEncoderFactory(
      media::GpuVideoAcceleratorFactories* gpu_factories);
  ~RTCVideoEncoderFactory() override;

  // webrtc::VideoEncoderFactory implementation.
  std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
      const webrtc::SdpVideoFormat& format) override;
  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;
  webrtc::VideoEncoderFactory::CodecInfo QueryVideoEncoder(
      const webrtc::SdpVideoFormat& format) const override;

 private:
  media::GpuVideoAcceleratorFactories* gpu_factories_;

  // List of supported webrtc::SdpVideoFormat. |profiles_| and
  // |supported_formats_| have the same length and the profile for
  // |supported_formats_[i]| is |profiles_[i]|.
  std::vector<media::VideoCodecProfile> profiles_;
  std::vector<webrtc::SdpVideoFormat> supported_formats_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoEncoderFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_RTC_VIDEO_ENCODER_FACTORY_H_
