// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_dummy_video_encoder.h"

#include <algorithm>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/lock.h"

namespace remoting {
namespace protocol {

WebrtcDummyVideoEncoder::WebrtcDummyVideoEncoder(webrtc::VideoCodecType codec)
    : state_(kUninitialized), video_codec_type_(codec) {
  VLOG(1) << "video codecType " << video_codec_type_;
}

WebrtcDummyVideoEncoder::~WebrtcDummyVideoEncoder() {}

int32_t WebrtcDummyVideoEncoder::InitEncode(
    const webrtc::VideoCodec* codec_settings,
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

int32_t WebrtcDummyVideoEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback) {
  base::AutoLock lock(lock_);
  DCHECK(callback);
  encoded_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcDummyVideoEncoder::Release() {
  base::AutoLock lock(lock_);
  encoded_callback_ = nullptr;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcDummyVideoEncoder::Encode(
    const webrtc::VideoFrame& frame,
    const webrtc::CodecSpecificInfo* codec_specific_info,
    const std::vector<webrtc::FrameType>* frame_types) {
  base::AutoLock lock(lock_);
  if (!key_frame_request_.is_null())
    key_frame_request_.Run();
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcDummyVideoEncoder::SetChannelParameters(uint32_t packet_loss,
                                                      int64_t rtt) {
  VLOG(1) << "WebrtcDummyVideoEncoder::SetChannelParameters "
          << "loss:RTT " << packet_loss << ":" << rtt;
  // Unused right now.
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcDummyVideoEncoder::SetRates(uint32_t bitrate,
                                          uint32_t framerate) {
  VLOG(1) << "WebrtcDummyVideoEncoder::SetRates bitrate:framerate " << bitrate
          << ":" << framerate;
  if (!target_bitrate_cb_.is_null())
    target_bitrate_cb_.Run(bitrate);
  // framerate is not expected to be valid given we never report captured
  // frames
  return WEBRTC_VIDEO_CODEC_OK;
}

webrtc::EncodedImageCallback::Result WebrtcDummyVideoEncoder::SendEncodedFrame(
    const WebrtcVideoEncoder::EncodedFrame& frame,
    base::TimeTicks capture_time) {
  uint8_t* buffer =
      reinterpret_cast<uint8_t*>(const_cast<char*>(frame.data.data()));
  size_t buffer_size = frame.data.size();
  base::AutoLock lock(lock_);
  if (state_ == kUninitialized) {
    LOG(ERROR) << "encoder interface uninitialized";
    return webrtc::EncodedImageCallback::Result(
        webrtc::EncodedImageCallback::Result::ERROR_SEND_FAILED);
  }

  webrtc::EncodedImage encoded_image(buffer, buffer_size, buffer_size);
  encoded_image._encodedWidth = frame.size.width();
  encoded_image._encodedHeight = frame.size.height();
  encoded_image._completeFrame = true;
  encoded_image._frameType =
      frame.key_frame ? webrtc::kVideoFrameKey : webrtc::kVideoFrameDelta;
  int64_t capture_time_ms = (capture_time - base::TimeTicks()).InMilliseconds();
  encoded_image.capture_time_ms_ = capture_time_ms;
  encoded_image._timeStamp = static_cast<uint32_t>(capture_time_ms * 90);
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

  return encoded_callback_->OnEncodedImage(encoded_image, &codec_specific_info,
                                           &header);
}

void WebrtcDummyVideoEncoder::SetKeyFrameRequestCallback(
    const base::Closure& key_frame_request) {
  base::AutoLock lock(lock_);
  key_frame_request_ = key_frame_request;
}

void WebrtcDummyVideoEncoder::SetTargetBitrateCallback(
    const TargetBitrateCallback& target_bitrate_cb) {
  base::AutoLock lock(lock_);
  target_bitrate_cb_ = target_bitrate_cb;
}

WebrtcDummyVideoEncoderFactory::WebrtcDummyVideoEncoderFactory() {
  // TODO(isheriff): These do not really affect anything internally
  // in webrtc.
  codecs_.push_back(cricket::WebRtcVideoEncoderFactory::VideoCodec(
      webrtc::kVideoCodecVP8, "VP8", 1280, 720, 30));
}

WebrtcDummyVideoEncoderFactory::~WebrtcDummyVideoEncoderFactory() {
  DCHECK(encoders_.empty());
}

webrtc::VideoEncoder* WebrtcDummyVideoEncoderFactory::CreateVideoEncoder(
    webrtc::VideoCodecType type) {
  VLOG(2) << "WebrtcDummyVideoEncoderFactory::CreateVideoEncoder " << type;
  DCHECK(type == webrtc::kVideoCodecVP8);
  WebrtcDummyVideoEncoder* encoder = new WebrtcDummyVideoEncoder(type);
  base::AutoLock lock(lock_);
  encoder->SetKeyFrameRequestCallback(key_frame_request_);
  encoder->SetTargetBitrateCallback(target_bitrate_cb_);
  VLOG(1) << "Created " << encoder;
  encoders_.push_back(base::WrapUnique(encoder));
  return encoder;
}

const std::vector<cricket::WebRtcVideoEncoderFactory::VideoCodec>&
WebrtcDummyVideoEncoderFactory::codecs() const {
  VLOG(2) << "WebrtcDummyVideoEncoderFactory::codecs";
  return codecs_;
}

bool WebrtcDummyVideoEncoderFactory::EncoderTypeHasInternalSource(
    webrtc::VideoCodecType type) const {
  VLOG(2) << "WebrtcDummyVideoEncoderFactory::EncoderTypeHasInternalSource";
  return true;
}

void WebrtcDummyVideoEncoderFactory::DestroyVideoEncoder(
    webrtc::VideoEncoder* encoder) {
  VLOG(2) << "WebrtcDummyVideoEncoderFactory::DestroyVideoEncoder";
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

webrtc::EncodedImageCallback::Result
WebrtcDummyVideoEncoderFactory::SendEncodedFrame(
    const WebrtcVideoEncoder::EncodedFrame& frame,
    base::TimeTicks capture_time) {
  if (encoders_.size() != 1) {
    LOG(ERROR) << "Unexpected number of encoders " << encoders_.size();
    return webrtc::EncodedImageCallback::Result(
        webrtc::EncodedImageCallback::Result::ERROR_SEND_FAILED);
  }
  return encoders_.front()->SendEncodedFrame(frame, capture_time);
}

void WebrtcDummyVideoEncoderFactory::SetKeyFrameRequestCallback(
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

void WebrtcDummyVideoEncoderFactory::SetTargetBitrateCallback(
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

}  // namespace protocol
}  // namespace remoting
