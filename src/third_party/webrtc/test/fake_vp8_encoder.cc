/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/fake_vp8_encoder.h"

#include "common_types.h"  // NOLINT(build/include)
#include "modules/video_coding/codecs/vp8/temporal_layers.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/utility/simulcast_utility.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/random.h"
#include "rtc_base/timeutils.h"

namespace webrtc {

namespace test {

FakeVP8Encoder::FakeVP8Encoder(Clock* clock)
    : FakeEncoder(clock), callback_(nullptr) {
  FakeEncoder::RegisterEncodeCompleteCallback(this);
  sequence_checker_.Detach();
}

int32_t FakeVP8Encoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  callback_ = callback;
  return 0;
}

int32_t FakeVP8Encoder::InitEncode(const VideoCodec* config,
                                   int32_t number_of_cores,
                                   size_t max_payload_size) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  auto result =
      FakeEncoder::InitEncode(config, number_of_cores, max_payload_size);
  if (result != WEBRTC_VIDEO_CODEC_OK) {
    return result;
  }

  int number_of_streams = SimulcastUtility::NumberOfSimulcastStreams(*config);
  bool doing_simulcast = number_of_streams > 1;

  int num_temporal_layers =
      doing_simulcast ? config->simulcastStream[0].numberOfTemporalLayers
                      : config->VP8().numberOfTemporalLayers;
  RTC_DCHECK_GT(num_temporal_layers, 0);

  SetupTemporalLayers(number_of_streams, num_temporal_layers, *config);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeVP8Encoder::Release() {
  auto result = FakeEncoder::Release();
  sequence_checker_.Detach();
  return result;
}

void FakeVP8Encoder::SetupTemporalLayers(int num_streams,
                                         int num_temporal_layers,
                                         const VideoCodec& codec) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  temporal_layers_.clear();
  for (int i = 0; i < num_streams; ++i) {
    temporal_layers_.emplace_back(
        TemporalLayers::CreateTemporalLayers(codec, i));
  }
}

void FakeVP8Encoder::PopulateCodecSpecific(
    CodecSpecificInfo* codec_specific,
    const TemporalLayers::FrameConfig& tl_config,
    FrameType frame_type,
    int stream_idx,
    uint32_t timestamp) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  codec_specific->codecType = kVideoCodecVP8;
  codec_specific->codec_name = ImplementationName();
  CodecSpecificInfoVP8* vp8Info = &(codec_specific->codecSpecific.VP8);
  vp8Info->simulcastIdx = stream_idx;
  vp8Info->keyIdx = kNoKeyIdx;
  vp8Info->nonReference = false;
  temporal_layers_[stream_idx]->PopulateCodecSpecific(
      frame_type == kVideoFrameKey, tl_config, vp8Info, timestamp);
}

EncodedImageCallback::Result FakeVP8Encoder::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific_info,
    const RTPFragmentationHeader* fragments) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  uint8_t stream_idx = codec_specific_info->codecSpecific.generic.simulcast_idx;
  CodecSpecificInfo overrided_specific_info;
  TemporalLayers::FrameConfig tl_config =
      temporal_layers_[stream_idx]->UpdateLayerConfig(encoded_image._timeStamp);
  PopulateCodecSpecific(&overrided_specific_info, tl_config,
                        encoded_image._frameType, stream_idx,
                        encoded_image._timeStamp);
  temporal_layers_[stream_idx]->FrameEncoded(encoded_image._timeStamp,
                                             encoded_image._length, -1);

  return callback_->OnEncodedImage(encoded_image, &overrided_specific_info,
                                   fragments);
}

}  // namespace test
}  // namespace webrtc
