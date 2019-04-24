/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/video_coding_impl.h"

#include <algorithm>
#include <memory>

#include "api/video/encoded_image.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/timing.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/thread_checker.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace vcm {

int64_t VCMProcessTimer::Period() const {
  return _periodMs;
}

int64_t VCMProcessTimer::TimeUntilProcess() const {
  const int64_t time_since_process = _clock->TimeInMilliseconds() - _latestMs;
  const int64_t time_until_process = _periodMs - time_since_process;
  return std::max<int64_t>(time_until_process, 0);
}

void VCMProcessTimer::Processed() {
  _latestMs = _clock->TimeInMilliseconds();
}
}  // namespace vcm

namespace {

class VideoCodingModuleImpl : public VideoCodingModule {
 public:
  VideoCodingModuleImpl(Clock* clock,
                        NackSender* nack_sender,
                        KeyFrameRequestSender* keyframe_request_sender)
      : VideoCodingModule(),
        timing_(new VCMTiming(clock)),
        receiver_(clock, timing_.get(), nack_sender, keyframe_request_sender) {}

  ~VideoCodingModuleImpl() override {}

  int64_t TimeUntilNextProcess() override {
    int64_t receiver_time = receiver_.TimeUntilNextProcess();
    RTC_DCHECK_GE(receiver_time, 0);
    return receiver_time;
  }

  void Process() override { receiver_.Process(); }

  int32_t RegisterReceiveCodec(const VideoCodec* receiveCodec,
                               int32_t numberOfCores,
                               bool requireKeyFrame) override {
    return receiver_.RegisterReceiveCodec(receiveCodec, numberOfCores,
                                          requireKeyFrame);
  }

  void RegisterExternalDecoder(VideoDecoder* externalDecoder,
                               uint8_t payloadType) override {
    receiver_.RegisterExternalDecoder(externalDecoder, payloadType);
  }

  int32_t RegisterReceiveCallback(
      VCMReceiveCallback* receiveCallback) override {
    RTC_DCHECK(construction_thread_.CalledOnValidThread());
    return receiver_.RegisterReceiveCallback(receiveCallback);
  }

  int32_t RegisterFrameTypeCallback(
      VCMFrameTypeCallback* frameTypeCallback) override {
    return receiver_.RegisterFrameTypeCallback(frameTypeCallback);
  }

  int32_t RegisterPacketRequestCallback(
      VCMPacketRequestCallback* callback) override {
    RTC_DCHECK(construction_thread_.CalledOnValidThread());
    return receiver_.RegisterPacketRequestCallback(callback);
  }

  int32_t Decode(uint16_t maxWaitTimeMs) override {
    return receiver_.Decode(maxWaitTimeMs);
  }

  int32_t IncomingPacket(const uint8_t* incomingPayload,
                         size_t payloadLength,
                         const WebRtcRTPHeader& rtpInfo) override {
    return receiver_.IncomingPacket(incomingPayload, payloadLength, rtpInfo);
  }

  int SetReceiverRobustnessMode(ReceiverRobustness robustnessMode) override {
    return receiver_.SetReceiverRobustnessMode(robustnessMode);
  }

  void SetNackSettings(size_t max_nack_list_size,
                       int max_packet_age_to_nack,
                       int max_incomplete_time_ms) override {
    return receiver_.SetNackSettings(max_nack_list_size, max_packet_age_to_nack,
                                     max_incomplete_time_ms);
  }

 private:
  rtc::ThreadChecker construction_thread_;
  const std::unique_ptr<VCMTiming> timing_;
  vcm::VideoReceiver receiver_;
};
}  // namespace

// DEPRECATED.  Create method for current interface, will be removed when the
// new jitter buffer is in place.
VideoCodingModule* VideoCodingModule::Create(Clock* clock) {
  RTC_DCHECK(clock);
  return new VideoCodingModuleImpl(clock, nullptr, nullptr);
}

}  // namespace webrtc
