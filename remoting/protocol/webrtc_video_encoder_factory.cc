// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_encoder_factory.h"

#include <algorithm>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/lock.h"

namespace remoting {

WebrtcVideoEncoder::WebrtcVideoEncoder(webrtc::VideoCodecType codec)
    : state_(kUninitialized), video_codec_type_(codec) {
  VLOG(1) << "video codecType " << video_codec_type_;
}

WebrtcVideoEncoder::~WebrtcVideoEncoder() {}

int32_t WebrtcVideoEncoder::InitEncode(const webrtc::VideoCodec* codec_settings,
                                       int32_t number_of_cores,
                                       size_t max_payload_size) {
  base::AutoLock lock(lock_);
  DCHECK(codec_settings);
  VLOG(1) << "video codecType " << codec_settings->codecType << " width "
          << codec_settings->width << " height " << codec_settings->height
          << " startBitrate " << codec_settings->startBitrate << " maxBitrate "
          << codec_settings->maxBitrate << " minBitrate "
          << codec_settings->minBitrate << " targetBitrate "
          << codec_settings->targetBitrate << " maxFramerate "
          << codec_settings->maxFramerate;

  int streamCount = codec_settings->numberOfSimulcastStreams;
  // Validate request is to support a single stream.
  if (streamCount > 1) {
    for (int i = 0; i < streamCount; ++i) {
      if (codec_settings->simulcastStream[i].maxBitrate != 0) {
        LOG(ERROR) << "Simulcast unsupported";
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
      }
    }
  }
  state_ = kInitialized;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcVideoEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback) {
  base::AutoLock lock(lock_);
  DCHECK(callback);
  encoded_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcVideoEncoder::Release() {
  base::AutoLock lock(lock_);
  encoded_callback_ = nullptr;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcVideoEncoder::Encode(
    const webrtc::VideoFrame& frame,
    const webrtc::CodecSpecificInfo* codec_specific_info,
    const std::vector<webrtc::FrameType>* frame_types) {
  base::AutoLock lock(lock_);
  if (!key_frame_request_.is_null())
    key_frame_request_.Run();
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcVideoEncoder::SetChannelParameters(uint32_t packet_loss,
                                                 int64_t rtt) {
  VLOG(1) << "WebrtcVideoEncoder::SetChannelParameters "
          << "loss:RTT " << packet_loss << ":" << rtt;
  // Unused right now.
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcVideoEncoder::SetRates(uint32_t bitrate, uint32_t framerate) {
  VLOG(1) << "WebrtcVideoEncoder::SetRates bitrate:framerate " << bitrate << ":"
          << framerate;
  if (!target_bitrate_cb_.is_null())
    target_bitrate_cb_.Run(bitrate);
  // framerate is not expected to be valid given we never report captured
  // frames
  return WEBRTC_VIDEO_CODEC_OK;
}

int WebrtcVideoEncoder::SendEncodedFrame(std::unique_ptr<VideoPacket> frame) {
  uint8_t* buffer =
      reinterpret_cast<uint8_t*>(const_cast<char*>(frame->data().data()));
  size_t buffer_size = frame->data().size();
  base::AutoLock lock(lock_);
  if (state_ == kUninitialized) {
    LOG(ERROR) << "encoder interface uninitialized";
    return -1;
  }

  webrtc::EncodedImage encoded_image(buffer, buffer_size, buffer_size);
  encoded_image._encodedWidth = frame->format().screen_width();
  encoded_image._encodedHeight = frame->format().screen_height();
  encoded_image._completeFrame = true;
  encoded_image._frameType =
      frame->key_frame() ? webrtc::kVideoFrameKey : webrtc::kVideoFrameDelta;
  encoded_image.capture_time_ms_ = frame->capture_time_ms();
  encoded_image._timeStamp =
      static_cast<uint32_t>(frame->capture_time_ms() * 90);
  encoded_image.playout_delay_.min_ms = 0;
  encoded_image.playout_delay_.max_ms = 0;

  webrtc::CodecSpecificInfo codec_specific_info;
  memset(&codec_specific_info, 0, sizeof(codec_specific_info));
  codec_specific_info.codecType = webrtc::kVideoCodecVP8;

  webrtc::RTPFragmentationHeader header;
  memset(&header, 0, sizeof(header));

  codec_specific_info.codecSpecific.VP8.simulcastIdx = 0;
  codec_specific_info.codecSpecific.VP8.temporalIdx = webrtc::kNoTemporalIdx;
  codec_specific_info.codecSpecific.VP8.tl0PicIdx = webrtc::kNoTl0PicIdx;
  codec_specific_info.codecSpecific.VP8.pictureId = webrtc::kNoPictureId;

  header.VerifyAndAllocateFragmentationHeader(1);
  header.fragmentationOffset[0] = 0;
  header.fragmentationLength[0] = buffer_size;
  header.fragmentationPlType[0] = 0;
  header.fragmentationTimeDiff[0] = 0;

  int result =
      encoded_callback_->Encoded(encoded_image, &codec_specific_info, &header);
  if (result < 0) {
    LOG(ERROR) << "Encoded callback failed: " << result;
  } else if (result > 0) {
    VLOG(1) << "Drop request from webrtc";
  }
  return result;
}

void WebrtcVideoEncoder::SetKeyFrameRequestCallback(
    const base::Closure& key_frame_request) {
  base::AutoLock lock(lock_);
  key_frame_request_ = key_frame_request;
}

void WebrtcVideoEncoder::SetTargetBitrateCallback(
    const TargetBitrateCallback& target_bitrate_cb) {
  base::AutoLock lock(lock_);
  target_bitrate_cb_ = target_bitrate_cb;
}

WebrtcVideoEncoderFactory::WebrtcVideoEncoderFactory() {
  // TODO(isheriff): These do not really affect anything internally
  // in webrtc.
  codecs_.push_back(cricket::WebRtcVideoEncoderFactory::VideoCodec(
      webrtc::kVideoCodecVP8, "VP8", 1280, 720, 30));
}

WebrtcVideoEncoderFactory::~WebrtcVideoEncoderFactory() {
  DCHECK(encoders_.empty());
}

webrtc::VideoEncoder* WebrtcVideoEncoderFactory::CreateVideoEncoder(
    webrtc::VideoCodecType type) {
  VLOG(2) << "WebrtcVideoEncoderFactory::CreateVideoEncoder " << type;
  DCHECK(type == webrtc::kVideoCodecVP8);
  WebrtcVideoEncoder* encoder = new WebrtcVideoEncoder(type);
  base::AutoLock lock(lock_);
  encoder->SetKeyFrameRequestCallback(key_frame_request_);
  encoder->SetTargetBitrateCallback(target_bitrate_cb_);
  VLOG(1) << "Created " << encoder;
  encoders_.push_back(base::WrapUnique(encoder));
  return encoder;
}

const std::vector<cricket::WebRtcVideoEncoderFactory::VideoCodec>&
WebrtcVideoEncoderFactory::codecs() const {
  VLOG(2) << "WebrtcVideoEncoderFactory::codecs";
  return codecs_;
}

bool WebrtcVideoEncoderFactory::EncoderTypeHasInternalSource(
    webrtc::VideoCodecType type) const {
  VLOG(2) << "WebrtcVideoEncoderFactory::EncoderTypeHasInternalSource";
  return true;
}

void WebrtcVideoEncoderFactory::DestroyVideoEncoder(
    webrtc::VideoEncoder* encoder) {
  VLOG(2) << "WebrtcVideoEncoderFactory::DestroyVideoEncoder";
  if (encoder == nullptr) {
    LOG(ERROR) << "Attempting to destroy null encoder";
    return;
  }
  for (auto pos = encoders_.begin(); pos != encoders_.end(); ++pos) {
    if ((*pos).get() == encoder) {
      encoders_.erase(pos);
      return;
    }
  }
  DCHECK(false) << "Asked to remove encoder not owned by factory";
}

int WebrtcVideoEncoderFactory::SendEncodedFrame(
    std::unique_ptr<VideoPacket> frame) {
  if (encoders_.size() != 1) {
    LOG(ERROR) << "Unexpected number of encoders " << encoders_.size();
    return -1;
  }
  return encoders_.front()->SendEncodedFrame(std::move(frame));
}

void WebrtcVideoEncoderFactory::SetKeyFrameRequestCallback(
    const base::Closure& key_frame_request) {
  base::AutoLock lock(lock_);
  key_frame_request_ = key_frame_request;
  if (encoders_.size() == 1) {
    encoders_.front()->SetKeyFrameRequestCallback(key_frame_request);
  } else {
    LOG(ERROR) << "Dropping key frame request callback with unexpected"
                  " number of encoders: "
               << encoders_.size();
  }
}

void WebrtcVideoEncoderFactory::SetTargetBitrateCallback(
    const TargetBitrateCallback& target_bitrate_cb) {
  base::AutoLock lock(lock_);
  target_bitrate_cb_ = target_bitrate_cb;
  if (encoders_.size() == 1) {
    encoders_.front()->SetTargetBitrateCallback(target_bitrate_cb);
  } else {
    LOG(ERROR) << "Dropping target bitrate request callback with unexpected"
                  " number of encoders: "
               << encoders_.size();
  }
}
}  // namespace remoting
