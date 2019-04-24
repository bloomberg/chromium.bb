// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_video_decoder_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "content/renderer/media/webrtc/rtc_video_decoder.h"
#include "content/renderer/media/webrtc/rtc_video_decoder_adapter.h"
#include "media/base/media_switches.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/common_video/h264/profile_level_id.h"
#include "third_party/webrtc/media/base/codec.h"
#include "third_party/webrtc/media/base/vp9_profile.h"

namespace content {

namespace {

const int kDefaultFps = 30;

// Translate from media::VideoDecodeAccelerator::SupportedProfile to
// webrtc::SdpVideoFormat, or return nothing if the profile isn't supported.
base::Optional<webrtc::SdpVideoFormat> VDAToWebRTCFormat(
    const media::VideoDecodeAccelerator::SupportedProfile& profile) {
  if (profile.profile >= media::VP8PROFILE_MIN &&
      profile.profile <= media::VP8PROFILE_MAX) {
    return webrtc::SdpVideoFormat("VP8");
  } else if (profile.profile >= media::VP9PROFILE_MIN &&
             profile.profile <= media::VP9PROFILE_MAX) {
    webrtc::VP9Profile vp9_profile;
    switch (profile.profile) {
      case media::VP9PROFILE_PROFILE0:
        vp9_profile = webrtc::VP9Profile::kProfile0;
        break;
      case media::VP9PROFILE_PROFILE2:
        vp9_profile = webrtc::VP9Profile::kProfile2;
        break;
      default:
        // Unsupported H264 profile in WebRTC.
        return base::nullopt;
    }
    return webrtc::SdpVideoFormat(
        "VP9",
        {{webrtc::kVP9FmtpProfileId, webrtc::VP9ProfileToString(vp9_profile)}});
  } else if (profile.profile >= media::H264PROFILE_MIN &&
             profile.profile <= media::H264PROFILE_MAX) {
    webrtc::H264::Profile h264_profile;
    switch (profile.profile) {
      case media::H264PROFILE_BASELINE:
        h264_profile = webrtc::H264::kProfileBaseline;
        break;
      case media::H264PROFILE_MAIN:
        h264_profile = webrtc::H264::kProfileMain;
        break;
      case media::H264PROFILE_HIGH:
        h264_profile = webrtc::H264::kProfileHigh;
        break;
      default:
        // Unsupported H264 profile in WebRTC.
        return base::nullopt;
    }

    const int width = profile.max_resolution.width();
    const int height = profile.max_resolution.height();

    const absl::optional<webrtc::H264::Level> h264_level =
        webrtc::H264::SupportedLevel(width * height, kDefaultFps);
    const webrtc::H264::ProfileLevelId profile_level_id(
        h264_profile, h264_level.value_or(webrtc::H264::kLevel1));

    webrtc::SdpVideoFormat format("H264");
    format.parameters = {
        {cricket::kH264FmtpProfileLevelId,
         *webrtc::H264::ProfileLevelIdToString(profile_level_id)},
        {cricket::kH264FmtpLevelAsymmetryAllowed, "1"},
        {cricket::kH264FmtpPacketizationMode, "1"}};
    return format;
  }
  return base::nullopt;
}

// Due to https://crbug.com/345569, HW decoders do not distinguish between
// Constrained Baseline(CBP) and Baseline(BP) profiles. Since CBP is a subset of
// BP, we can report support for both. It is safe to do so when SW fallback is
// available.
// TODO(emircan): Remove this when the bug referred above is fixed.
void MapBaselineProfile(
    std::vector<webrtc::SdpVideoFormat>* supported_formats) {
  for (const auto& format : *supported_formats) {
    const absl::optional<webrtc::H264::ProfileLevelId> profile_level_id =
        webrtc::H264::ParseSdpProfileLevelId(format.parameters);
    if (profile_level_id &&
        profile_level_id->profile == webrtc::H264::kProfileBaseline) {
      webrtc::SdpVideoFormat cbp_format = format;
      webrtc::H264::ProfileLevelId cbp_profile = *profile_level_id;
      cbp_profile.profile = webrtc::H264::kProfileConstrainedBaseline;
      cbp_format.parameters[cricket::kH264FmtpProfileLevelId] =
          *webrtc::H264::ProfileLevelIdToString(cbp_profile);
      supported_formats->push_back(cbp_format);
      return;
    }
  }
}

// This extra indirection is needed so that we can delete the decoder on the
// correct thread.
class ScopedVideoDecoder : public webrtc::VideoDecoder {
 public:
  ScopedVideoDecoder(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      std::unique_ptr<webrtc::VideoDecoder> decoder)
      : task_runner_(task_runner), decoder_(std::move(decoder)) {}

  int32_t InitDecode(const webrtc::VideoCodec* codec_settings,
                     int32_t number_of_cores) override {
    return decoder_->InitDecode(codec_settings, number_of_cores);
  }
  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override {
    return decoder_->RegisterDecodeCompleteCallback(callback);
  }
  int32_t Release() override { return decoder_->Release(); }
  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 const webrtc::CodecSpecificInfo* codec_specific_info,
                 int64_t render_time_ms) override {
    return decoder_->Decode(input_image, missing_frames, codec_specific_info,
                            render_time_ms);
  }
  bool PrefersLateDecoding() const override {
    return decoder_->PrefersLateDecoding();
  }
  const char* ImplementationName() const override {
    return decoder_->ImplementationName();
  }

  // Runs on Chrome_libJingle_WorkerThread. The child thread is blocked while
  // this runs.
  ~ScopedVideoDecoder() override {
    task_runner_->DeleteSoon(FROM_HERE, decoder_.release());
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<webrtc::VideoDecoder> decoder_;
};

}  // namespace

RTCVideoDecoderFactory::RTCVideoDecoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : gpu_factories_(gpu_factories) {
  DVLOG(2) << __func__;

  // RTCVideoDecoderAdapter does not use |supported_formats_|.
  if (base::FeatureList::IsEnabled(media::kRTCVideoDecoderAdapter))
    return;

  const media::VideoDecodeAccelerator::SupportedProfiles profiles =
      gpu_factories_->GetVideoDecodeAcceleratorCapabilities()
          .supported_profiles;
  for (const auto& profile : profiles) {
    base::Optional<webrtc::SdpVideoFormat> format = VDAToWebRTCFormat(profile);
    if (format)
      supported_formats_.push_back(std::move(*format));
  }
  MapBaselineProfile(&supported_formats_);
}

std::vector<webrtc::SdpVideoFormat>
RTCVideoDecoderFactory::GetSupportedFormats() const {
  DCHECK(!base::FeatureList::IsEnabled(media::kRTCVideoDecoderAdapter));
  return supported_formats_;
}

RTCVideoDecoderFactory::~RTCVideoDecoderFactory() {
  DVLOG(2) << __func__;
}

std::unique_ptr<webrtc::VideoDecoder>
RTCVideoDecoderFactory::CreateVideoDecoder(
    const webrtc::SdpVideoFormat& format) {
  DVLOG(2) << __func__;
  std::unique_ptr<webrtc::VideoDecoder> decoder;
  if (base::FeatureList::IsEnabled(media::kRTCVideoDecoderAdapter)) {
    decoder = RTCVideoDecoderAdapter::Create(gpu_factories_, format);
  } else {
    decoder = RTCVideoDecoder::Create(format, gpu_factories_);
  }
  // ScopedVideoDecoder uses the task runner to make sure the decoder is
  // destructed on the correct thread.
  return decoder ? std::make_unique<ScopedVideoDecoder>(
                       gpu_factories_->GetTaskRunner(), std::move(decoder))
                 : nullptr;
}

}  // namespace content
