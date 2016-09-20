// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_DUMMY_WEBRTC_VIDEO_ENCODER_H_
#define REMOTING_PROTOCOL_DUMMY_WEBRTC_VIDEO_ENCODER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/synchronization/lock.h"
#include "remoting/codec/webrtc_video_encoder.h"
#include "third_party/webrtc/media/engine/webrtcvideoencoderfactory.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"

namespace remoting {
namespace protocol {

using TargetBitrateCallback = base::Callback<void(int)>;

// WebrtcDummyVideoEncoder implements webrtc::VideoEncoder interface for WebRTC.
// It's responsible for getting  feedback on network bandwidth, latency & RTT
// and passing this information to the WebrtcVideoStream through the callbacks
// in WebrtcDummyVideoEncoderFactory. Video frames are captured encoded outside
// of this dummy encoder (in WebrtcVideoEncoded called from WebrtcVideoStream).
// They are passed to SendEncodedFrame() to be delivered to the network layer.
class WebrtcDummyVideoEncoder : public webrtc::VideoEncoder {
 public:
  enum State { kUninitialized = 0, kInitialized };
  explicit WebrtcDummyVideoEncoder(webrtc::VideoCodecType codec);
  ~WebrtcDummyVideoEncoder() override;

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

  webrtc::EncodedImageCallback::Result SendEncodedFrame(
      const WebrtcVideoEncoder::EncodedFrame& frame,
      base::TimeTicks capture_time);
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
class WebrtcDummyVideoEncoderFactory
    : public cricket::WebRtcVideoEncoderFactory {
 public:
  WebrtcDummyVideoEncoderFactory();
  ~WebrtcDummyVideoEncoderFactory() override;

  webrtc::VideoEncoder* CreateVideoEncoder(
      webrtc::VideoCodecType type) override;
  const std::vector<cricket::WebRtcVideoEncoderFactory::VideoCodec>& codecs()
      const override;

  // Returns true to directly provide encoded frames to webrtc
  bool EncoderTypeHasInternalSource(webrtc::VideoCodecType type) const override;
  void DestroyVideoEncoder(webrtc::VideoEncoder* encoder) override;

  webrtc::EncodedImageCallback::Result SendEncodedFrame(
      const WebrtcVideoEncoder::EncodedFrame& packet,
      base::TimeTicks capture_time);

  void SetKeyFrameRequestCallback(const base::Closure& key_frame_request);
  void SetTargetBitrateCallback(const TargetBitrateCallback& target_bitrate_cb);

 private:
  // Protects |key_frame_request_| and |target_bitrate_cb_|.
  base::Lock lock_;
  base::Closure key_frame_request_;
  TargetBitrateCallback target_bitrate_cb_;
  std::vector<cricket::WebRtcVideoEncoderFactory::VideoCodec> codecs_;
  std::vector<std::unique_ptr<WebrtcDummyVideoEncoder>> encoders_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_DUMMY_WEBRTC_VIDEO_ENCODER_H_
