// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_encoder_factory.h"

#include "base/check.h"
#include "base/threading/thread_task_runner_handle.h"
#include "remoting/protocol/video_channel_state_observer.h"
#include "remoting/protocol/webrtc_video_encoder_wrapper.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/api/video_codecs/vp9_profile.h"

#if defined(USE_H264_ENCODER)
#include "remoting/codec/webrtc_video_encoder_gpu.h"
#endif

namespace remoting::protocol {

WebrtcVideoEncoderFactory::WebrtcVideoEncoderFactory()
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  formats_.push_back(webrtc::SdpVideoFormat("VP8"));
  formats_.push_back(webrtc::SdpVideoFormat("VP9"));
  formats_.push_back(
      webrtc::SdpVideoFormat("VP9", {{webrtc::kVP9FmtpProfileId, "1"}}));
#if defined(USE_H264_ENCODER)
  // This call will query the underlying media classes to determine whether
  // hardware encoding is supported or not. We use a default resolution and
  // framerate so the call doesn't fail due to invalid params.
  if (WebrtcVideoEncoderGpu::IsSupportedByH264({{1920, 1080}, 30})) {
    formats_.push_back(webrtc::SdpVideoFormat("H264"));
  }
#endif
#if defined(USE_AV1_ENCODER)
  formats_.push_back(webrtc::SdpVideoFormat("AV1"));
#endif
}

WebrtcVideoEncoderFactory::~WebrtcVideoEncoderFactory() = default;

std::unique_ptr<webrtc::VideoEncoder>
WebrtcVideoEncoderFactory::CreateVideoEncoder(
    const webrtc::SdpVideoFormat& format) {
  return std::make_unique<WebrtcVideoEncoderWrapper>(
      format, main_task_runner_, video_channel_state_observer_);
}

std::vector<webrtc::SdpVideoFormat>
WebrtcVideoEncoderFactory::GetSupportedFormats() const {
  return formats_;
}

void WebrtcVideoEncoderFactory::SetVideoChannelStateObserver(
    base::WeakPtr<VideoChannelStateObserver> video_channel_state_observer) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  video_channel_state_observer_ = video_channel_state_observer;
}

}  // namespace remoting::protocol
