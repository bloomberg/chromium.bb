// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_VIDEO_ENCODER_FACTORY_H_
#define REMOTING_PROTOCOL_WEBRTC_VIDEO_ENCODER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/synchronization/lock.h"
#include "remoting/proto/video.pb.h"
#include "third_party/webrtc/media/engine/webrtcvideoencoderfactory.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"

namespace remoting {

using TargetBitrateCallback = base::Callback<void(int)>;

// This is the interface between the Webrtc engine and the external encoder
// provided by remoting. Webrtc provides feedback on network bandwidth, latency
// & RTT and in turn remoting passes encoded frames as they get encoded
// through the capture pipeline.
class WebrtcVideoEncoder : public webrtc::VideoEncoder {
 public:
  enum State { kUninitialized = 0, kInitialized };
  explicit WebrtcVideoEncoder(webrtc::VideoCodecType codec);
  ~WebrtcVideoEncoder() override;

  // webrtc::VideoEncoder overrides
  int32_t InitEncode(const webrtc::VideoCodec* codec_settings,
                     int32_t number_of_cores,
                     size_t max_payload_size) override;
  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) override;
  int32_t Release() override;
  int32_t Encode(const webrtc::VideoFrame& frame,
                 const webrtc::CodecSpecificInfo* codec_specific_info,
                 const std::vector<webrtc::FrameType>* frame_types) override;
  int32_t SetChannelParameters(uint32_t packet_loss, int64_t rtt) override;
  int32_t SetRates(uint32_t bitrate, uint32_t framerate) override;

  int SendEncodedFrame(std::unique_ptr<VideoPacket> pkt);
  void SetKeyFrameRequestCallback(const base::Closure& key_frame_request);
  void SetTargetBitrateCallback(const TargetBitrateCallback& target_bitrate_cb);

 private:
  // Protects |encoded_callback_|, |key_frame_request_|,
  // |target_bitrate_cb_| and |state_|.
  base::Lock lock_;
  State state_;
  webrtc::EncodedImageCallback* encoded_callback_;

  base::Closure key_frame_request_;
  TargetBitrateCallback target_bitrate_cb_;
  webrtc::VideoCodecType video_codec_type_;
};

// This is the external encoder factory implementation that is passed to
// WebRTC at the time of creation of peer connection. The external encoder
// factory primarily manages creation and destruction of encoder.
class WebrtcVideoEncoderFactory : public cricket::WebRtcVideoEncoderFactory {
 public:
  WebrtcVideoEncoderFactory();
  ~WebrtcVideoEncoderFactory() override;

  webrtc::VideoEncoder* CreateVideoEncoder(
      webrtc::VideoCodecType type) override;
  const std::vector<cricket::WebRtcVideoEncoderFactory::VideoCodec>& codecs()
      const override;

  // Returns true to directly provide encoded frames to webrtc
  bool EncoderTypeHasInternalSource(webrtc::VideoCodecType type) const override;
  void DestroyVideoEncoder(webrtc::VideoEncoder* encoder) override;

  int SendEncodedFrame(std::unique_ptr<VideoPacket> pkt);

  void SetKeyFrameRequestCallback(const base::Closure& key_frame_request);
  void SetTargetBitrateCallback(const TargetBitrateCallback& target_bitrate_cb);

 private:
  // Protects |key_frame_request_| and |target_bitrate_cb_|.
  base::Lock lock_;
  base::Closure key_frame_request_;
  TargetBitrateCallback target_bitrate_cb_;
  std::vector<cricket::WebRtcVideoEncoderFactory::VideoCodec> codecs_;
  std::vector<std::unique_ptr<WebrtcVideoEncoder>> encoders_;
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_VIDEO_ENCODER_FACTORY_H_
