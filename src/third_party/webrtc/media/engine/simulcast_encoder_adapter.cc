/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/simulcast_encoder_adapter.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_codec_constants.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/utility/simulcast_rate_allocator.h"
#include "rtc_base/atomic_ops.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/rate_control_settings.h"
#include "system_wrappers/include/field_trial.h"
#include "third_party/libyuv/include/libyuv/scale.h"

namespace {

const unsigned int kDefaultMinQp = 2;
const unsigned int kDefaultMaxQp = 56;
// Max qp for lowest spatial resolution when doing simulcast.
const unsigned int kLowestResMaxQp = 45;

absl::optional<unsigned int> GetScreenshareBoostedQpValue() {
  std::string experiment_group =
      webrtc::field_trial::FindFullName("WebRTC-BoostedScreenshareQp");
  unsigned int qp;
  if (sscanf(experiment_group.c_str(), "%u", &qp) != 1)
    return absl::nullopt;
  qp = std::min(qp, 63u);
  qp = std::max(qp, 1u);
  return qp;
}

uint32_t SumStreamMaxBitrate(int streams, const webrtc::VideoCodec& codec) {
  uint32_t bitrate_sum = 0;
  for (int i = 0; i < streams; ++i) {
    bitrate_sum += codec.simulcastStream[i].maxBitrate;
  }
  return bitrate_sum;
}

int NumberOfStreams(const webrtc::VideoCodec& codec) {
  int streams =
      codec.numberOfSimulcastStreams < 1 ? 1 : codec.numberOfSimulcastStreams;
  uint32_t simulcast_max_bitrate = SumStreamMaxBitrate(streams, codec);
  if (simulcast_max_bitrate == 0) {
    streams = 1;
  }
  return streams;
}

int VerifyCodec(const webrtc::VideoCodec* inst) {
  if (inst == nullptr) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->maxFramerate < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  // allow zero to represent an unspecified maxBitRate
  if (inst->maxBitrate > 0 && inst->startBitrate > inst->maxBitrate) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->width <= 1 || inst->height <= 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->codecType == webrtc::kVideoCodecVP8 &&
      inst->VP8().automaticResizeOn && inst->numberOfSimulcastStreams > 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

bool StreamResolutionCompare(const webrtc::SimulcastStream& a,
                             const webrtc::SimulcastStream& b) {
  return std::tie(a.height, a.width, a.maxBitrate, a.maxFramerate) <
         std::tie(b.height, b.width, b.maxBitrate, b.maxFramerate);
}

// An EncodedImageCallback implementation that forwards on calls to a
// SimulcastEncoderAdapter, but with the stream index it's registered with as
// the first parameter to Encoded.
class AdapterEncodedImageCallback : public webrtc::EncodedImageCallback {
 public:
  AdapterEncodedImageCallback(webrtc::SimulcastEncoderAdapter* adapter,
                              size_t stream_idx)
      : adapter_(adapter), stream_idx_(stream_idx) {}

  EncodedImageCallback::Result OnEncodedImage(
      const webrtc::EncodedImage& encoded_image,
      const webrtc::CodecSpecificInfo* codec_specific_info,
      const webrtc::RTPFragmentationHeader* fragmentation) override {
    return adapter_->OnEncodedImage(stream_idx_, encoded_image,
                                    codec_specific_info, fragmentation);
  }

 private:
  webrtc::SimulcastEncoderAdapter* const adapter_;
  const size_t stream_idx_;
};
}  // namespace

namespace webrtc {

SimulcastEncoderAdapter::SimulcastEncoderAdapter(VideoEncoderFactory* factory,
                                                 const SdpVideoFormat& format)
    : inited_(0),
      factory_(factory),
      video_format_(format),
      encoded_complete_callback_(nullptr),
      experimental_boosted_screenshare_qp_(GetScreenshareBoostedQpValue()),
      boost_base_layer_quality_(RateControlSettings::ParseFromFieldTrials()
                                    .Vp8BoostBaseLayerQuality()) {
  RTC_DCHECK(factory_);
  encoder_info_.implementation_name = "SimulcastEncoderAdapter";

  // The adapter is typically created on the worker thread, but operated on
  // the encoder task queue.
  encoder_queue_.Detach();

  memset(&codec_, 0, sizeof(webrtc::VideoCodec));
}

SimulcastEncoderAdapter::~SimulcastEncoderAdapter() {
  RTC_DCHECK(!Initialized());
  DestroyStoredEncoders();
}

int SimulcastEncoderAdapter::Release() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&encoder_queue_);

  while (!streaminfos_.empty()) {
    std::unique_ptr<VideoEncoder> encoder =
        std::move(streaminfos_.back().encoder);
    // Even though it seems very unlikely, there are no guarantees that the
    // encoder will not call back after being Release()'d. Therefore, we first
    // disable the callbacks here.
    encoder->RegisterEncodeCompleteCallback(nullptr);
    encoder->Release();
    streaminfos_.pop_back();  // Deletes callback adapter.
    stored_encoders_.push(std::move(encoder));
  }

  // It's legal to move the encoder to another queue now.
  encoder_queue_.Detach();

  rtc::AtomicOps::ReleaseStore(&inited_, 0);

  return WEBRTC_VIDEO_CODEC_OK;
}

int SimulcastEncoderAdapter::InitEncode(const VideoCodec* inst,
                                        int number_of_cores,
                                        size_t max_payload_size) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&encoder_queue_);

  if (number_of_cores < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  int ret = VerifyCodec(inst);
  if (ret < 0) {
    return ret;
  }

  ret = Release();
  if (ret < 0) {
    return ret;
  }

  int number_of_streams = NumberOfStreams(*inst);
  RTC_DCHECK_LE(number_of_streams, kMaxSimulcastStreams);
  const bool doing_simulcast = (number_of_streams > 1);

  codec_ = *inst;
  SimulcastRateAllocator rate_allocator(codec_);
  VideoBitrateAllocation allocation = rate_allocator.GetAllocation(
      codec_.startBitrate * 1000, codec_.maxFramerate);
  std::vector<uint32_t> start_bitrates;
  for (int i = 0; i < kMaxSimulcastStreams; ++i) {
    uint32_t stream_bitrate = allocation.GetSpatialLayerSum(i) / 1000;
    start_bitrates.push_back(stream_bitrate);
  }

  encoder_info_.supports_native_handle = true;
  encoder_info_.scaling_settings.thresholds = absl::nullopt;
  // Create |number_of_streams| of encoder instances and init them.

  const auto minmax = std::minmax_element(
      std::begin(codec_.simulcastStream),
      std::begin(codec_.simulcastStream) + number_of_streams,
      StreamResolutionCompare);
  const auto lowest_resolution_stream_index =
      std::distance(std::begin(codec_.simulcastStream), minmax.first);
  const auto highest_resolution_stream_index =
      std::distance(std::begin(codec_.simulcastStream), minmax.second);

  RTC_DCHECK_LT(lowest_resolution_stream_index, number_of_streams);
  RTC_DCHECK_LT(highest_resolution_stream_index, number_of_streams);

  for (int i = 0; i < number_of_streams; ++i) {
    VideoCodec stream_codec;
    uint32_t start_bitrate_kbps = start_bitrates[i];
    const bool send_stream = start_bitrate_kbps > 0;
    if (!doing_simulcast) {
      stream_codec = codec_;
      stream_codec.numberOfSimulcastStreams = 1;

    } else {
      // Cap start bitrate to the min bitrate in order to avoid strange codec
      // behavior. Since sending will be false, this should not matter.
      StreamResolution stream_resolution =
          i == highest_resolution_stream_index
              ? StreamResolution::HIGHEST
              : i == lowest_resolution_stream_index ? StreamResolution::LOWEST
                                                    : StreamResolution::OTHER;

      start_bitrate_kbps =
          std::max(codec_.simulcastStream[i].minBitrate, start_bitrate_kbps);
      PopulateStreamCodec(codec_, i, start_bitrate_kbps, stream_resolution,
                          &stream_codec);
    }

    // TODO(ronghuawu): Remove once this is handled in LibvpxVp8Encoder.
    if (stream_codec.qpMax < kDefaultMinQp) {
      stream_codec.qpMax = kDefaultMaxQp;
    }

    // If an existing encoder instance exists, reuse it.
    // TODO(brandtr): Set initial RTP state (e.g., picture_id/tl0_pic_idx) here,
    // when we start storing that state outside the encoder wrappers.
    std::unique_ptr<VideoEncoder> encoder;
    if (!stored_encoders_.empty()) {
      encoder = std::move(stored_encoders_.top());
      stored_encoders_.pop();
    } else {
      encoder = factory_->CreateVideoEncoder(SdpVideoFormat(
          codec_.codecType == webrtc::kVideoCodecVP8 ? "VP8" : "H264"));
    }

    ret = encoder->InitEncode(&stream_codec, number_of_cores, max_payload_size);
    if (ret < 0) {
      // Explicitly destroy the current encoder; because we haven't registered a
      // StreamInfo for it yet, Release won't do anything about it.
      encoder.reset();
      Release();
      return ret;
    }

    std::unique_ptr<EncodedImageCallback> callback(
        new AdapterEncodedImageCallback(this, i));
    encoder->RegisterEncodeCompleteCallback(callback.get());
    streaminfos_.emplace_back(std::move(encoder), std::move(callback),
                              stream_codec.width, stream_codec.height,
                              send_stream);

    if (!doing_simulcast) {
      // Without simulcast, just pass through the encoder info from the one
      // active encoder.
      encoder_info_ = streaminfos_[0].encoder->GetEncoderInfo();
    } else {
      const EncoderInfo encoder_impl_info =
          streaminfos_[i].encoder->GetEncoderInfo();

      if (i == 0) {
        // Quality scaling not enabled for simulcast.
        encoder_info_.scaling_settings = VideoEncoder::ScalingSettings::kOff;

        // Encoder name indicates names of all sub-encoders.
        encoder_info_.implementation_name = "SimulcastEncoderAdapter (";
        encoder_info_.implementation_name +=
            encoder_impl_info.implementation_name;

        encoder_info_.supports_native_handle =
            encoder_impl_info.supports_native_handle;
        encoder_info_.has_trusted_rate_controller =
            encoder_impl_info.has_trusted_rate_controller;
        encoder_info_.is_hardware_accelerated =
            encoder_impl_info.is_hardware_accelerated;
        encoder_info_.has_internal_source =
            encoder_impl_info.has_internal_source;
      } else {
        encoder_info_.implementation_name += ", ";
        encoder_info_.implementation_name +=
            encoder_impl_info.implementation_name;

        // Native handle supported only if all encoders supports it.
        encoder_info_.supports_native_handle &=
            encoder_impl_info.supports_native_handle;

        // Trusted rate controller only if all encoders have it.
        encoder_info_.has_trusted_rate_controller &=
            encoder_impl_info.has_trusted_rate_controller;

        // Uses hardware support if any of the encoders uses it.
        // For example, if we are having issues with down-scaling due to
        // pipelining delay in HW encoders we need higher encoder usage
        // thresholds in CPU adaptation.
        encoder_info_.is_hardware_accelerated |=
            encoder_impl_info.is_hardware_accelerated;

        // Has internal source only if all encoders have it.
        encoder_info_.has_internal_source &=
            encoder_impl_info.has_internal_source;
      }
      encoder_info_.fps_allocation[i] = encoder_impl_info.fps_allocation[0];
    }
  }

  if (doing_simulcast) {
    encoder_info_.implementation_name += ")";
  }

  // To save memory, don't store encoders that we don't use.
  DestroyStoredEncoders();

  rtc::AtomicOps::ReleaseStore(&inited_, 1);

  return WEBRTC_VIDEO_CODEC_OK;
}

int SimulcastEncoderAdapter::Encode(
    const VideoFrame& input_image,
    const CodecSpecificInfo* codec_specific_info,
    const std::vector<FrameType>* frame_types) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&encoder_queue_);

  if (!Initialized()) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (encoded_complete_callback_ == nullptr) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  // All active streams should generate a key frame if
  // a key frame is requested by any stream.
  bool send_key_frame = false;
  if (frame_types) {
    for (size_t i = 0; i < frame_types->size(); ++i) {
      if (frame_types->at(i) == kVideoFrameKey) {
        send_key_frame = true;
        break;
      }
    }
  }
  for (size_t stream_idx = 0; stream_idx < streaminfos_.size(); ++stream_idx) {
    if (streaminfos_[stream_idx].key_frame_request &&
        streaminfos_[stream_idx].send_stream) {
      send_key_frame = true;
      break;
    }
  }

  int src_width = input_image.width();
  int src_height = input_image.height();
  for (size_t stream_idx = 0; stream_idx < streaminfos_.size(); ++stream_idx) {
    // Don't encode frames in resolutions that we don't intend to send.
    if (!streaminfos_[stream_idx].send_stream) {
      continue;
    }

    std::vector<FrameType> stream_frame_types;
    if (send_key_frame) {
      stream_frame_types.push_back(kVideoFrameKey);
      streaminfos_[stream_idx].key_frame_request = false;
    } else {
      stream_frame_types.push_back(kVideoFrameDelta);
    }

    int dst_width = streaminfos_[stream_idx].width;
    int dst_height = streaminfos_[stream_idx].height;
    // If scaling isn't required, because the input resolution
    // matches the destination or the input image is empty (e.g.
    // a keyframe request for encoders with internal camera
    // sources) or the source image has a native handle, pass the image on
    // directly. Otherwise, we'll scale it to match what the encoder expects
    // (below).
    // For texture frames, the underlying encoder is expected to be able to
    // correctly sample/scale the source texture.
    // TODO(perkj): ensure that works going forward, and figure out how this
    // affects webrtc:5683.
    if ((dst_width == src_width && dst_height == src_height) ||
        input_image.video_frame_buffer()->type() ==
            VideoFrameBuffer::Type::kNative) {
      int ret = streaminfos_[stream_idx].encoder->Encode(
          input_image, codec_specific_info, &stream_frame_types);
      if (ret != WEBRTC_VIDEO_CODEC_OK) {
        return ret;
      }
    } else {
      rtc::scoped_refptr<I420Buffer> dst_buffer =
          I420Buffer::Create(dst_width, dst_height);
      rtc::scoped_refptr<I420BufferInterface> src_buffer =
          input_image.video_frame_buffer()->ToI420();
      libyuv::I420Scale(src_buffer->DataY(), src_buffer->StrideY(),
                        src_buffer->DataU(), src_buffer->StrideU(),
                        src_buffer->DataV(), src_buffer->StrideV(), src_width,
                        src_height, dst_buffer->MutableDataY(),
                        dst_buffer->StrideY(), dst_buffer->MutableDataU(),
                        dst_buffer->StrideU(), dst_buffer->MutableDataV(),
                        dst_buffer->StrideV(), dst_width, dst_height,
                        libyuv::kFilterBilinear);

      // UpdateRect is not propagated to lower simulcast layers currently.
      // TODO(ilnik): Consider scaling UpdateRect together with the buffer.
      VideoFrame frame = VideoFrame::Builder()
                             .set_video_frame_buffer(dst_buffer)
                             .set_timestamp_rtp(input_image.timestamp())
                             .set_rotation(webrtc::kVideoRotation_0)
                             .set_timestamp_ms(input_image.render_time_ms())
                             .build();
      int ret = streaminfos_[stream_idx].encoder->Encode(
          frame, codec_specific_info, &stream_frame_types);
      if (ret != WEBRTC_VIDEO_CODEC_OK) {
        return ret;
      }
    }
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int SimulcastEncoderAdapter::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&encoder_queue_);
  encoded_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int SimulcastEncoderAdapter::SetRateAllocation(
    const VideoBitrateAllocation& bitrate,
    uint32_t new_framerate) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&encoder_queue_);

  if (!Initialized()) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  if (new_framerate < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  if (codec_.maxBitrate > 0 && bitrate.get_sum_kbps() > codec_.maxBitrate) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  if (bitrate.get_sum_bps() > 0) {
    // Make sure the bitrate fits the configured min bitrates. 0 is a special
    // value that means paused, though, so leave it alone.
    if (bitrate.get_sum_kbps() < codec_.minBitrate) {
      return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }

    if (codec_.numberOfSimulcastStreams > 0 &&
        bitrate.get_sum_kbps() < codec_.simulcastStream[0].minBitrate) {
      return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
  }

  codec_.maxFramerate = new_framerate;

  for (size_t stream_idx = 0; stream_idx < streaminfos_.size(); ++stream_idx) {
    uint32_t stream_bitrate_kbps =
        bitrate.GetSpatialLayerSum(stream_idx) / 1000;

    // Need a key frame if we have not sent this stream before.
    if (stream_bitrate_kbps > 0 && !streaminfos_[stream_idx].send_stream) {
      streaminfos_[stream_idx].key_frame_request = true;
    }
    streaminfos_[stream_idx].send_stream = stream_bitrate_kbps > 0;

    // Slice the temporal layers out of the full allocation and pass it on to
    // the encoder handling the current simulcast stream.
    VideoBitrateAllocation stream_allocation;
    for (int i = 0; i < kMaxTemporalStreams; ++i) {
      if (bitrate.HasBitrate(stream_idx, i)) {
        stream_allocation.SetBitrate(0, i, bitrate.GetBitrate(stream_idx, i));
      }
    }
    streaminfos_[stream_idx].encoder->SetRateAllocation(stream_allocation,
                                                        new_framerate);
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

// TODO(brandtr): Add task checker to this member function, when all encoder
// callbacks are coming in on the encoder queue.
EncodedImageCallback::Result SimulcastEncoderAdapter::OnEncodedImage(
    size_t stream_idx,
    const EncodedImage& encodedImage,
    const CodecSpecificInfo* codecSpecificInfo,
    const RTPFragmentationHeader* fragmentation) {
  EncodedImage stream_image(encodedImage);
  CodecSpecificInfo stream_codec_specific = *codecSpecificInfo;

  stream_image.SetSpatialIndex(stream_idx);

  return encoded_complete_callback_->OnEncodedImage(
      stream_image, &stream_codec_specific, fragmentation);
}

void SimulcastEncoderAdapter::PopulateStreamCodec(
    const webrtc::VideoCodec& inst,
    int stream_index,
    uint32_t start_bitrate_kbps,
    StreamResolution stream_resolution,
    webrtc::VideoCodec* stream_codec) {
  *stream_codec = inst;

  // Stream specific settings.
  stream_codec->numberOfSimulcastStreams = 0;
  stream_codec->width = inst.simulcastStream[stream_index].width;
  stream_codec->height = inst.simulcastStream[stream_index].height;
  stream_codec->maxBitrate = inst.simulcastStream[stream_index].maxBitrate;
  stream_codec->minBitrate = inst.simulcastStream[stream_index].minBitrate;
  stream_codec->qpMax = inst.simulcastStream[stream_index].qpMax;
  // Settings that are based on stream/resolution.
  if (stream_resolution == StreamResolution::LOWEST) {
    // Settings for lowest spatial resolutions.
    if (inst.mode == VideoCodecMode::kScreensharing) {
      if (experimental_boosted_screenshare_qp_) {
        stream_codec->qpMax = *experimental_boosted_screenshare_qp_;
      }
    } else if (boost_base_layer_quality_) {
      stream_codec->qpMax = kLowestResMaxQp;
    }
  }
  if (inst.codecType == webrtc::kVideoCodecVP8) {
    stream_codec->VP8()->numberOfTemporalLayers =
        inst.simulcastStream[stream_index].numberOfTemporalLayers;
    if (stream_resolution != StreamResolution::HIGHEST) {
      // For resolutions below CIF, set the codec |complexity| parameter to
      // kComplexityHigher, which maps to cpu_used = -4.
      int pixels_per_frame = stream_codec->width * stream_codec->height;
      if (pixels_per_frame < 352 * 288) {
        stream_codec->VP8()->complexity =
            webrtc::VideoCodecComplexity::kComplexityHigher;
      }
      // Turn off denoising for all streams but the highest resolution.
      stream_codec->VP8()->denoisingOn = false;
    }
  }
  // TODO(ronghuawu): what to do with targetBitrate.

  stream_codec->startBitrate = start_bitrate_kbps;
}

bool SimulcastEncoderAdapter::Initialized() const {
  return rtc::AtomicOps::AcquireLoad(&inited_) == 1;
}

void SimulcastEncoderAdapter::DestroyStoredEncoders() {
  while (!stored_encoders_.empty()) {
    stored_encoders_.pop();
  }
}

VideoEncoder::EncoderInfo SimulcastEncoderAdapter::GetEncoderInfo() const {
  return encoder_info_;
}

}  // namespace webrtc
