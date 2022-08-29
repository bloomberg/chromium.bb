/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_receive_stream2.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <ostream>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "api/metronome/test/fake_metronome.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/test/mock_video_decoder.h"
#include "api/test/mock_video_decoder_factory.h"
#include "api/test/time_controller.h"
#include "api/test/video/function_video_decoder_factory.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/video/encoded_image.h"
#include "api/video/recordable_encoded_frame.h"
#include "api/video/test/video_frame_matchers.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder.h"
#include "call/rtp_stream_receiver_controller.h"
#include "call/video_receive_stream.h"
#include "common_video/test/utilities.h"
#include "media/base/fake_video_renderer.h"
#include "media/engine/fake_webrtc_call.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/video_coding/encoded_frame.h"
#include "rtc_base/event.h"
#include "rtc_base/strings/string_builder.h"
#include "system_wrappers/include/clock.h"
#include "test/fake_decoder.h"
#include "test/fake_encoded_frame.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "test/run_loop.h"
#include "test/scoped_key_value_config.h"
#include "test/time_controller/simulated_time_controller.h"
#include "test/video_decoder_proxy_factory.h"
#include "video/call_stats2.h"

namespace webrtc {

// Printing SdpVideoFormat for gmock argument matchers.
void PrintTo(const SdpVideoFormat& value, std::ostream* os) {
  *os << value.ToString();
}

void PrintTo(const RecordableEncodedFrame::EncodedResolution& value,
             std::ostream* os) {
  *os << value.width << "x" << value.height;
}

void PrintTo(const RecordableEncodedFrame& value, std::ostream* os) {
  *os << "RecordableEncodedFrame(render_time=" << value.render_time()
      << " resolution=" << ::testing::PrintToString(value.resolution()) << ")";
}

}  // namespace webrtc

namespace webrtc {

namespace {

using test::video_frame_matchers::NtpTimestamp;
using test::video_frame_matchers::PacketInfos;
using test::video_frame_matchers::Rotation;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::WithoutArgs;

auto RenderedFrameWith(::testing::Matcher<VideoFrame> m) {
  return Optional(m);
}
auto RenderedFrame() {
  return RenderedFrameWith(_);
}
testing::Matcher<absl::optional<VideoFrame>> DidNotReceiveFrame() {
  return Eq(absl::nullopt);
}

constexpr TimeDelta kDefaultTimeOut = TimeDelta::Millis(50);
constexpr int kDefaultNumCpuCores = 2;

constexpr Timestamp kStartTime = Timestamp::Millis(1'337'000);
constexpr Frequency k30Fps = Frequency::Hertz(30);
constexpr TimeDelta k30FpsDelay = 1 / k30Fps;
constexpr Frequency kRtpTimestampHz = Frequency::KiloHertz(90);
constexpr uint32_t k30FpsRtpTimestampDelta = kRtpTimestampHz / k30Fps;
constexpr uint32_t kFirstRtpTimestamp = 90000;

class FakeVideoRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  explicit FakeVideoRenderer(TimeController* time_controller,
                             test::RunLoop* loop)
      : time_controller_(time_controller), loop_(loop) {}
  ~FakeVideoRenderer() override = default;

  void OnFrame(const webrtc::VideoFrame& frame) override {
    last_frame_ = frame;
  }

  absl::optional<webrtc::VideoFrame> WaitForFrame(TimeDelta timeout) {
    if (!last_frame_) {
      loop_->Flush();
      time_controller_->AdvanceTime(TimeDelta::Zero());
      time_controller_->Wait(
          [this] {
            loop_->Flush();
            return last_frame_.has_value();
          },
          timeout);
    }
    absl::optional<webrtc::VideoFrame> ret = std::move(last_frame_);
    last_frame_.reset();
    return ret;
  }

 private:
  absl::optional<webrtc::VideoFrame> last_frame_;
  TimeController* const time_controller_;
  test::RunLoop* const loop_;
};

MATCHER_P2(Resolution, w, h, "") {
  return arg.resolution().width == w && arg.resolution().height == h;
}

MATCHER_P(Id, id, "") {
  return arg.id() == id;
}

// Rtp timestamp for in order frame at 30fps.
uint32_t RtpTimestampForFrame(int id) {
  return kFirstRtpTimestamp + id * k30FpsRtpTimestampDelta;
}

// Receive time for in order frame at 30fps.
Timestamp ReceiveTimeForFrame(int id) {
  return kStartTime + id * k30FpsDelay;
}

}  // namespace

class VideoReceiveStream2Test : public ::testing::TestWithParam<bool> {
 public:
  auto DefaultDecodeAction() {
    return testing::Invoke(&fake_decoder_, &test::FakeDecoder::Decode);
  }

  bool UseMetronome() const { return GetParam(); }

  VideoReceiveStream2Test()
      : time_controller_(kStartTime),
        clock_(time_controller_.GetClock()),
        config_(&mock_transport_, &mock_h264_decoder_factory_),
        call_stats_(clock_, loop_.task_queue()),
        fake_renderer_(&time_controller_, &loop_),
        fake_metronome_(time_controller_.GetTaskQueueFactory(),
                        TimeDelta::Millis(16)),
        decode_sync_(clock_, &fake_metronome_, loop_.task_queue()),
        h264_decoder_factory_(&mock_decoder_) {
    if (UseMetronome()) {
      fake_call_.SetFieldTrial("WebRTC-FrameBuffer3/arm:SyncDecoding/");
    } else {
      fake_call_.SetFieldTrial("WebRTC-FrameBuffer3/arm:FrameBuffer3/");
    }

    // By default, mock decoder factory is backed by VideoDecoderProxyFactory.
    ON_CALL(mock_h264_decoder_factory_, CreateVideoDecoder)
        .WillByDefault(testing::Invoke(
            &h264_decoder_factory_,
            &test::VideoDecoderProxyFactory::CreateVideoDecoder));

    // By default, mock decode will wrap the fake decoder.
    ON_CALL(mock_decoder_, Configure)
        .WillByDefault(
            testing::Invoke(&fake_decoder_, &test::FakeDecoder::Configure));
    ON_CALL(mock_decoder_, Decode).WillByDefault(DefaultDecodeAction());
    ON_CALL(mock_decoder_, RegisterDecodeCompleteCallback)
        .WillByDefault(testing::Invoke(
            &fake_decoder_,
            &test::FakeDecoder::RegisterDecodeCompleteCallback));
    ON_CALL(mock_decoder_, Release)
        .WillByDefault(
            testing::Invoke(&fake_decoder_, &test::FakeDecoder::Release));
  }
  ~VideoReceiveStream2Test() override {
    if (video_receive_stream_) {
      video_receive_stream_->Stop();
      video_receive_stream_->UnregisterFromTransport();
    }
    fake_metronome_.Stop();
    time_controller_.AdvanceTime(TimeDelta::Zero());
  }

  void SetUp() override {
    config_.rtp.remote_ssrc = 1111;
    config_.rtp.local_ssrc = 2222;
    config_.renderer = &fake_renderer_;
    VideoReceiveStreamInterface::Decoder h264_decoder;
    h264_decoder.payload_type = 99;
    h264_decoder.video_format = SdpVideoFormat("H264");
    h264_decoder.video_format.parameters.insert(
        {"sprop-parameter-sets", "Z0IACpZTBYmI,aMljiA=="});
    VideoReceiveStreamInterface::Decoder h265_decoder;
    h265_decoder.payload_type = 100;
    h265_decoder.video_format = SdpVideoFormat("H265");

    config_.decoders = {h265_decoder, h264_decoder};

    RecreateReceiveStream();
  }

  void RecreateReceiveStream(
      absl::optional<VideoReceiveStreamInterface::RecordingState> state =
          absl::nullopt) {
    if (video_receive_stream_) {
      video_receive_stream_->UnregisterFromTransport();
      video_receive_stream_ = nullptr;
    }
    timing_ = new VCMTiming(clock_, fake_call_.trials());
    video_receive_stream_ =
        std::make_unique<webrtc::internal::VideoReceiveStream2>(
            time_controller_.GetTaskQueueFactory(), &fake_call_,
            kDefaultNumCpuCores, &packet_router_, config_.Copy(), &call_stats_,
            clock_, absl::WrapUnique(timing_), &nack_periodic_processor_,
            GetParam() ? &decode_sync_ : nullptr);
    video_receive_stream_->RegisterWithTransport(
        &rtp_stream_receiver_controller_);
    if (state)
      video_receive_stream_->SetAndGetRecordingState(std::move(*state), false);
  }

 protected:
  GlobalSimulatedTimeController time_controller_;
  Clock* const clock_;
  // Tasks on the main thread can be controlled with the run loop.
  test::RunLoop loop_;
  NackPeriodicProcessor nack_periodic_processor_;
  testing::NiceMock<MockVideoDecoderFactory> mock_h264_decoder_factory_;
  VideoReceiveStreamInterface::Config config_;
  internal::CallStats call_stats_;
  testing::NiceMock<MockVideoDecoder> mock_decoder_;
  FakeVideoRenderer fake_renderer_;
  cricket::FakeCall fake_call_;
  MockTransport mock_transport_;
  PacketRouter packet_router_;
  RtpStreamReceiverController rtp_stream_receiver_controller_;
  std::unique_ptr<webrtc::internal::VideoReceiveStream2> video_receive_stream_;
  VCMTiming* timing_;
  test::FakeMetronome fake_metronome_;
  DecodeSynchronizer decode_sync_;

 private:
  test::VideoDecoderProxyFactory h264_decoder_factory_;
  test::FakeDecoder fake_decoder_;
};

TEST_P(VideoReceiveStream2Test, CreateFrameFromH264FmtpSpropAndIdr) {
  constexpr uint8_t idr_nalu[] = {0x05, 0xFF, 0xFF, 0xFF};
  RtpPacketToSend rtppacket(nullptr);
  uint8_t* payload = rtppacket.AllocatePayload(sizeof(idr_nalu));
  memcpy(payload, idr_nalu, sizeof(idr_nalu));
  rtppacket.SetMarker(true);
  rtppacket.SetSsrc(1111);
  rtppacket.SetPayloadType(99);
  rtppacket.SetSequenceNumber(1);
  rtppacket.SetTimestamp(0);
  EXPECT_CALL(mock_decoder_, RegisterDecodeCompleteCallback(_));
  video_receive_stream_->Start();
  EXPECT_CALL(mock_decoder_, Decode(_, false, _));
  RtpPacketReceived parsed_packet;
  ASSERT_TRUE(parsed_packet.Parse(rtppacket.data(), rtppacket.size()));
  rtp_stream_receiver_controller_.OnRtpPacket(parsed_packet);
  EXPECT_CALL(mock_decoder_, Release());

  time_controller_.AdvanceTime(TimeDelta::Zero());
}

TEST_P(VideoReceiveStream2Test, PlayoutDelay) {
  const VideoPlayoutDelay kPlayoutDelayMs = {123, 321};
  std::unique_ptr<test::FakeEncodedFrame> test_frame =
      test::FakeFrameBuilder().Id(0).AsLast().Build();
  test_frame->SetPlayoutDelay(kPlayoutDelayMs);

  video_receive_stream_->OnCompleteFrame(std::move(test_frame));
  auto timings = timing_->GetTimings();
  EXPECT_EQ(kPlayoutDelayMs.min_ms, timings.min_playout_delay.ms());
  EXPECT_EQ(kPlayoutDelayMs.max_ms, timings.max_playout_delay.ms());

  // Check that the biggest minimum delay is chosen.
  video_receive_stream_->SetMinimumPlayoutDelay(400);
  timings = timing_->GetTimings();
  EXPECT_EQ(400, timings.min_playout_delay.ms());

  // Check base minimum delay validation.
  EXPECT_FALSE(video_receive_stream_->SetBaseMinimumPlayoutDelayMs(12345));
  EXPECT_FALSE(video_receive_stream_->SetBaseMinimumPlayoutDelayMs(-1));
  EXPECT_TRUE(video_receive_stream_->SetBaseMinimumPlayoutDelayMs(500));
  timings = timing_->GetTimings();
  EXPECT_EQ(500, timings.min_playout_delay.ms());

  // Check that intermidiate values are remembered and the biggest remembered
  // is chosen.
  video_receive_stream_->SetBaseMinimumPlayoutDelayMs(0);
  timings = timing_->GetTimings();
  EXPECT_EQ(400, timings.min_playout_delay.ms());

  video_receive_stream_->SetMinimumPlayoutDelay(0);
  timings = timing_->GetTimings();
  EXPECT_EQ(123, timings.min_playout_delay.ms());
}

TEST_P(VideoReceiveStream2Test, PlayoutDelayPreservesDefaultMaxValue) {
  const TimeDelta default_max_playout_latency =
      timing_->GetTimings().max_playout_delay;
  const VideoPlayoutDelay kPlayoutDelayMs = {123, -1};

  std::unique_ptr<test::FakeEncodedFrame> test_frame =
      test::FakeFrameBuilder().Id(0).AsLast().Build();
  test_frame->SetPlayoutDelay(kPlayoutDelayMs);

  video_receive_stream_->OnCompleteFrame(std::move(test_frame));

  // Ensure that -1 preserves default maximum value from `timing_`.
  auto timings = timing_->GetTimings();
  EXPECT_EQ(kPlayoutDelayMs.min_ms, timings.min_playout_delay.ms());
  EXPECT_NE(kPlayoutDelayMs.max_ms, timings.max_playout_delay.ms());
  EXPECT_EQ(default_max_playout_latency, timings.max_playout_delay);
}

TEST_P(VideoReceiveStream2Test, PlayoutDelayPreservesDefaultMinValue) {
  const TimeDelta default_min_playout_latency =
      timing_->GetTimings().min_playout_delay;
  const VideoPlayoutDelay kPlayoutDelayMs = {-1, 321};

  std::unique_ptr<test::FakeEncodedFrame> test_frame =
      test::FakeFrameBuilder().Id(0).AsLast().Build();
  test_frame->SetPlayoutDelay(kPlayoutDelayMs);

  video_receive_stream_->OnCompleteFrame(std::move(test_frame));

  // Ensure that -1 preserves default minimum value from `timing_`.
  auto timings = timing_->GetTimings();
  EXPECT_NE(kPlayoutDelayMs.min_ms, timings.min_playout_delay.ms());
  EXPECT_EQ(kPlayoutDelayMs.max_ms, timings.max_playout_delay.ms());
  EXPECT_EQ(default_min_playout_latency, timings.min_playout_delay);
}

TEST_P(VideoReceiveStream2Test, MaxCompositionDelayNotSetByDefault) {
  // Default with no playout delay set.
  std::unique_ptr<test::FakeEncodedFrame> test_frame0 =
      test::FakeFrameBuilder().Id(0).AsLast().Build();
  video_receive_stream_->OnCompleteFrame(std::move(test_frame0));
  EXPECT_FALSE(timing_->MaxCompositionDelayInFrames());

  // Max composition delay not set for playout delay 0,0.
  std::unique_ptr<test::FakeEncodedFrame> test_frame1 =
      test::FakeFrameBuilder().Id(1).AsLast().Build();
  test_frame1->SetPlayoutDelay({0, 0});
  video_receive_stream_->OnCompleteFrame(std::move(test_frame1));
  EXPECT_FALSE(timing_->MaxCompositionDelayInFrames());

  // Max composition delay not set for playout delay X,Y, where X,Y>0.
  std::unique_ptr<test::FakeEncodedFrame> test_frame2 =
      test::FakeFrameBuilder().Id(2).AsLast().Build();
  test_frame2->SetPlayoutDelay({10, 30});
  video_receive_stream_->OnCompleteFrame(std::move(test_frame2));
  EXPECT_FALSE(timing_->MaxCompositionDelayInFrames());
}

TEST_P(VideoReceiveStream2Test, MaxCompositionDelaySetFromMaxPlayoutDelay) {
  // Max composition delay set if playout delay X,Y, where X=0,Y>0.
  const VideoPlayoutDelay kPlayoutDelayMs = {0, 50};
  const int kExpectedMaxCompositionDelayInFrames = 3;  // ~50 ms at 60 fps.
  std::unique_ptr<test::FakeEncodedFrame> test_frame =
      test::FakeFrameBuilder().Id(0).AsLast().Build();
  test_frame->SetPlayoutDelay(kPlayoutDelayMs);
  video_receive_stream_->OnCompleteFrame(std::move(test_frame));
  EXPECT_EQ(kExpectedMaxCompositionDelayInFrames,
            timing_->MaxCompositionDelayInFrames());
}

TEST_P(VideoReceiveStream2Test, LazyDecoderCreation) {
  constexpr uint8_t idr_nalu[] = {0x05, 0xFF, 0xFF, 0xFF};
  RtpPacketToSend rtppacket(nullptr);
  uint8_t* payload = rtppacket.AllocatePayload(sizeof(idr_nalu));
  memcpy(payload, idr_nalu, sizeof(idr_nalu));
  rtppacket.SetMarker(true);
  rtppacket.SetSsrc(1111);
  // H265 payload type.
  rtppacket.SetPayloadType(99);
  rtppacket.SetSequenceNumber(1);
  rtppacket.SetTimestamp(0);

  // Only 1 decoder is created by default. It will be H265 since that was the
  // first in the decoder list.
  EXPECT_CALL(mock_h264_decoder_factory_, CreateVideoDecoder(_)).Times(0);
  EXPECT_CALL(mock_h264_decoder_factory_,
              CreateVideoDecoder(
                  testing::Field(&SdpVideoFormat::name, testing::Eq("H265"))))
      .Times(1);
  video_receive_stream_->Start();

  EXPECT_TRUE(
      testing::Mock::VerifyAndClearExpectations(&mock_h264_decoder_factory_));

  EXPECT_CALL(mock_h264_decoder_factory_,
              CreateVideoDecoder(
                  testing::Field(&SdpVideoFormat::name, testing::Eq("H264"))))
      .Times(1);
  rtc::Event init_decode_event;
  EXPECT_CALL(mock_decoder_, Configure).WillOnce(WithoutArgs([&] {
    init_decode_event.Set();
    return true;
  }));
  EXPECT_CALL(mock_decoder_, RegisterDecodeCompleteCallback(_));
  EXPECT_CALL(mock_decoder_, Decode(_, false, _));
  RtpPacketReceived parsed_packet;
  ASSERT_TRUE(parsed_packet.Parse(rtppacket.data(), rtppacket.size()));
  rtp_stream_receiver_controller_.OnRtpPacket(parsed_packet);
  EXPECT_CALL(mock_decoder_, Release());

  // Make sure the decoder thread had a chance to run.
  init_decode_event.Wait(kDefaultTimeOut.ms());
}

TEST_P(VideoReceiveStream2Test, PassesNtpTime) {
  const Timestamp kNtpTimestamp = Timestamp::Millis(12345);
  std::unique_ptr<test::FakeEncodedFrame> test_frame =
      test::FakeFrameBuilder()
          .Id(0)
          .PayloadType(99)
          .NtpTime(kNtpTimestamp)
          .AsLast()
          .Build();

  video_receive_stream_->Start();
  video_receive_stream_->OnCompleteFrame(std::move(test_frame));
  EXPECT_THAT(fake_renderer_.WaitForFrame(kDefaultTimeOut),
              RenderedFrameWith(NtpTimestamp(kNtpTimestamp)));
}

TEST_P(VideoReceiveStream2Test, PassesRotation) {
  const webrtc::VideoRotation kRotation = webrtc::kVideoRotation_180;
  std::unique_ptr<test::FakeEncodedFrame> test_frame = test::FakeFrameBuilder()
                                                           .Id(0)
                                                           .PayloadType(99)
                                                           .Rotation(kRotation)
                                                           .AsLast()
                                                           .Build();

  video_receive_stream_->Start();
  video_receive_stream_->OnCompleteFrame(std::move(test_frame));
  EXPECT_THAT(fake_renderer_.WaitForFrame(kDefaultTimeOut),
              RenderedFrameWith(Rotation(kRotation)));
}

TEST_P(VideoReceiveStream2Test, PassesPacketInfos) {
  RtpPacketInfos packet_infos = CreatePacketInfos(3);
  auto test_frame = test::FakeFrameBuilder()
                        .Id(0)
                        .PayloadType(99)
                        .PacketInfos(packet_infos)
                        .AsLast()
                        .Build();

  video_receive_stream_->Start();
  video_receive_stream_->OnCompleteFrame(std::move(test_frame));
  EXPECT_THAT(fake_renderer_.WaitForFrame(kDefaultTimeOut),
              RenderedFrameWith(PacketInfos(ElementsAreArray(packet_infos))));
}

TEST_P(VideoReceiveStream2Test, RenderedFrameUpdatesGetSources) {
  constexpr uint32_t kSsrc = 1111;
  constexpr uint32_t kCsrc = 9001;
  constexpr uint32_t kRtpTimestamp = 12345;

  // Prepare one video frame with per-packet information.
  auto test_frame =
      test::FakeFrameBuilder().Id(0).PayloadType(99).AsLast().Build();
  RtpPacketInfos packet_infos;
  {
    RtpPacketInfos::vector_type infos;

    RtpPacketInfo info;
    info.set_ssrc(kSsrc);
    info.set_csrcs({kCsrc});
    info.set_rtp_timestamp(kRtpTimestamp);

    info.set_receive_time(clock_->CurrentTime() - TimeDelta::Millis(5000));
    infos.push_back(info);

    info.set_receive_time(clock_->CurrentTime() - TimeDelta::Millis(3000));
    infos.push_back(info);

    info.set_receive_time(clock_->CurrentTime() - TimeDelta::Millis(2000));
    infos.push_back(info);

    info.set_receive_time(clock_->CurrentTime() - TimeDelta::Millis(1000));
    infos.push_back(info);

    packet_infos = RtpPacketInfos(std::move(infos));
  }
  test_frame->SetPacketInfos(packet_infos);

  // Start receive stream.
  video_receive_stream_->Start();
  EXPECT_THAT(video_receive_stream_->GetSources(), IsEmpty());

  // Render one video frame.
  int64_t timestamp_ms_min = clock_->TimeInMilliseconds();
  video_receive_stream_->OnCompleteFrame(std::move(test_frame));
  // Verify that the per-packet information is passed to the renderer.
  EXPECT_THAT(fake_renderer_.WaitForFrame(kDefaultTimeOut),
              RenderedFrameWith(PacketInfos(ElementsAreArray(packet_infos))));
  int64_t timestamp_ms_max = clock_->TimeInMilliseconds();

  // Verify that the per-packet information also updates `GetSources()`.
  std::vector<RtpSource> sources = video_receive_stream_->GetSources();
  ASSERT_THAT(sources, SizeIs(2));
  {
    auto it = std::find_if(sources.begin(), sources.end(),
                           [](const RtpSource& source) {
                             return source.source_type() == RtpSourceType::SSRC;
                           });
    ASSERT_NE(it, sources.end());

    EXPECT_EQ(it->source_id(), kSsrc);
    EXPECT_EQ(it->source_type(), RtpSourceType::SSRC);
    EXPECT_EQ(it->rtp_timestamp(), kRtpTimestamp);
    EXPECT_GE(it->timestamp_ms(), timestamp_ms_min);
    EXPECT_LE(it->timestamp_ms(), timestamp_ms_max);
  }
  {
    auto it = std::find_if(sources.begin(), sources.end(),
                           [](const RtpSource& source) {
                             return source.source_type() == RtpSourceType::CSRC;
                           });
    ASSERT_NE(it, sources.end());

    EXPECT_EQ(it->source_id(), kCsrc);
    EXPECT_EQ(it->source_type(), RtpSourceType::CSRC);
    EXPECT_EQ(it->rtp_timestamp(), kRtpTimestamp);
    EXPECT_GE(it->timestamp_ms(), timestamp_ms_min);
    EXPECT_LE(it->timestamp_ms(), timestamp_ms_max);
  }
}

std::unique_ptr<test::FakeEncodedFrame> MakeFrameWithResolution(
    VideoFrameType frame_type,
    int picture_id,
    int width,
    int height) {
  auto frame =
      test::FakeFrameBuilder().Id(picture_id).PayloadType(99).AsLast().Build();
  frame->SetFrameType(frame_type);
  frame->_encodedWidth = width;
  frame->_encodedHeight = height;
  return frame;
}

std::unique_ptr<test::FakeEncodedFrame> MakeFrame(VideoFrameType frame_type,
                                                  int picture_id) {
  return MakeFrameWithResolution(frame_type, picture_id, 320, 240);
}

TEST_P(VideoReceiveStream2Test, PassesFrameWhenEncodedFramesCallbackSet) {
  testing::MockFunction<void(const RecordableEncodedFrame&)> callback;
  video_receive_stream_->Start();
  // Expect a keyframe request to be generated
  EXPECT_CALL(mock_transport_, SendRtcp);
  EXPECT_CALL(callback, Call);
  video_receive_stream_->SetAndGetRecordingState(
      VideoReceiveStreamInterface::RecordingState(callback.AsStdFunction()),
      true);
  video_receive_stream_->OnCompleteFrame(
      MakeFrame(VideoFrameType::kVideoFrameKey, 0));
  EXPECT_TRUE(fake_renderer_.WaitForFrame(kDefaultTimeOut));
  video_receive_stream_->Stop();
}

TEST_P(VideoReceiveStream2Test, MovesEncodedFrameDispatchStateWhenReCreating) {
  testing::MockFunction<void(const RecordableEncodedFrame&)> callback;
  video_receive_stream_->Start();
  // Expect a key frame request over RTCP.
  EXPECT_CALL(mock_transport_, SendRtcp).Times(1);
  video_receive_stream_->SetAndGetRecordingState(
      VideoReceiveStreamInterface::RecordingState(callback.AsStdFunction()),
      true);
  video_receive_stream_->Stop();
  VideoReceiveStreamInterface::RecordingState old_state =
      video_receive_stream_->SetAndGetRecordingState(
          VideoReceiveStreamInterface::RecordingState(), false);
  RecreateReceiveStream(std::move(old_state));
  video_receive_stream_->Stop();
}

TEST_P(VideoReceiveStream2Test, RequestsKeyFramesUntilKeyFrameReceived) {
  // Recreate receive stream with shorter delay to test rtx.
  TimeDelta rtx_delay = TimeDelta::Millis(50);
  config_.rtp.nack.rtp_history_ms = rtx_delay.ms();
  auto tick = rtx_delay / 2;
  RecreateReceiveStream();
  video_receive_stream_->Start();

  EXPECT_CALL(mock_transport_, SendRtcp).Times(1).WillOnce(testing::Return(0));
  video_receive_stream_->GenerateKeyFrame();
  video_receive_stream_->OnCompleteFrame(
      MakeFrame(VideoFrameType::kVideoFrameDelta, 0));
  fake_renderer_.WaitForFrame(kDefaultTimeOut);
  time_controller_.AdvanceTime(tick);
  loop_.Flush();
  video_receive_stream_->OnCompleteFrame(
      MakeFrame(VideoFrameType::kVideoFrameDelta, 1));
  fake_renderer_.WaitForFrame(kDefaultTimeOut);
  time_controller_.AdvanceTime(TimeDelta::Zero());
  testing::Mock::VerifyAndClearExpectations(&mock_transport_);

  // T+keyframetimeout: still no key frame received, expect key frame request
  // sent again.
  EXPECT_CALL(mock_transport_, SendRtcp).Times(1).WillOnce(testing::Return(0));
  time_controller_.AdvanceTime(tick);
  video_receive_stream_->OnCompleteFrame(
      MakeFrame(VideoFrameType::kVideoFrameDelta, 2));
  EXPECT_THAT(fake_renderer_.WaitForFrame(kDefaultTimeOut), RenderedFrame());
  loop_.Flush();
  testing::Mock::VerifyAndClearExpectations(&mock_transport_);

  // T+keyframetimeout: now send a key frame - we should not observe new key
  // frame requests after this.
  EXPECT_CALL(mock_transport_, SendRtcp).Times(0);
  video_receive_stream_->OnCompleteFrame(
      MakeFrame(VideoFrameType::kVideoFrameKey, 3));
  EXPECT_THAT(fake_renderer_.WaitForFrame(kDefaultTimeOut), RenderedFrame());
  time_controller_.AdvanceTime(2 * tick);
  video_receive_stream_->OnCompleteFrame(
      MakeFrame(VideoFrameType::kVideoFrameDelta, 4));
  EXPECT_THAT(fake_renderer_.WaitForFrame(kDefaultTimeOut), RenderedFrame());
  loop_.Flush();
}

TEST_P(VideoReceiveStream2Test,
       DispatchesEncodedFrameSequenceStartingWithKeyframeWithoutResolution) {
  video_receive_stream_->Start();
  testing::MockFunction<void(const RecordableEncodedFrame&)> callback;
  video_receive_stream_->SetAndGetRecordingState(
      VideoReceiveStreamInterface::RecordingState(callback.AsStdFunction()),
      /*generate_key_frame=*/false);

  InSequence s;
  EXPECT_CALL(callback, Call(Resolution(test::FakeDecoder::kDefaultWidth,
                                        test::FakeDecoder::kDefaultHeight)));
  EXPECT_CALL(callback, Call);

  video_receive_stream_->OnCompleteFrame(
      MakeFrameWithResolution(VideoFrameType::kVideoFrameKey, 0, 0, 0));
  EXPECT_THAT(fake_renderer_.WaitForFrame(kDefaultTimeOut), RenderedFrame());
  video_receive_stream_->OnCompleteFrame(
      MakeFrameWithResolution(VideoFrameType::kVideoFrameDelta, 1, 0, 0));
  EXPECT_THAT(fake_renderer_.WaitForFrame(kDefaultTimeOut), RenderedFrame());

  video_receive_stream_->Stop();
}

TEST_P(VideoReceiveStream2Test,
       DispatchesEncodedFrameSequenceStartingWithKeyframeWithResolution) {
  video_receive_stream_->Start();
  testing::MockFunction<void(const RecordableEncodedFrame&)> callback;
  video_receive_stream_->SetAndGetRecordingState(
      VideoReceiveStreamInterface::RecordingState(callback.AsStdFunction()),
      /*generate_key_frame=*/false);

  InSequence s;
  EXPECT_CALL(callback, Call(Resolution(1080u, 720u)));
  EXPECT_CALL(callback, Call);

  video_receive_stream_->OnCompleteFrame(
      MakeFrameWithResolution(VideoFrameType::kVideoFrameKey, 0, 1080, 720));
  EXPECT_THAT(fake_renderer_.WaitForFrame(kDefaultTimeOut), RenderedFrame());
  video_receive_stream_->OnCompleteFrame(
      MakeFrameWithResolution(VideoFrameType::kVideoFrameDelta, 1, 0, 0));
  EXPECT_THAT(fake_renderer_.WaitForFrame(kDefaultTimeOut), RenderedFrame());

  video_receive_stream_->Stop();
}

TEST_P(VideoReceiveStream2Test, DependantFramesAreScheduled) {
  video_receive_stream_->Start();

  auto key_frame = test::FakeFrameBuilder()
                       .Id(0)
                       .PayloadType(99)
                       .Time(kFirstRtpTimestamp)
                       .ReceivedTime(kStartTime)
                       .AsLast()
                       .Build();
  auto delta_frame = test::FakeFrameBuilder()
                         .Id(1)
                         .PayloadType(99)
                         .Time(RtpTimestampForFrame(1))
                         .ReceivedTime(ReceiveTimeForFrame(1))
                         .Refs({0})
                         .AsLast()
                         .Build();

  // Expect frames are decoded in order.
  InSequence seq;
  EXPECT_CALL(mock_decoder_,
              Decode(test::RtpTimestamp(kFirstRtpTimestamp), _, _));
  EXPECT_CALL(mock_decoder_, Decode(test::RtpTimestamp(kFirstRtpTimestamp +
                                                       k30FpsRtpTimestampDelta),
                                    _, _))
      .Times(1);
  video_receive_stream_->OnCompleteFrame(std::move(key_frame));
  EXPECT_THAT(fake_renderer_.WaitForFrame(TimeDelta::Zero()), RenderedFrame());

  time_controller_.AdvanceTime(k30FpsDelay);
  video_receive_stream_->OnCompleteFrame(std::move(delta_frame));
  EXPECT_THAT(fake_renderer_.WaitForFrame(k30FpsDelay), RenderedFrame());

  video_receive_stream_->Stop();
}

TEST_P(VideoReceiveStream2Test, FramesScheduledInOrder) {
  video_receive_stream_->Start();

  auto key_frame = test::FakeFrameBuilder()
                       .Id(0)
                       .PayloadType(99)
                       .Time(kFirstRtpTimestamp)
                       .AsLast()
                       .Build();
  auto delta_frame1 = test::FakeFrameBuilder()
                          .Id(1)
                          .PayloadType(99)
                          .Time(RtpTimestampForFrame(1))
                          .Refs({0})
                          .AsLast()
                          .Build();
  auto delta_frame2 = test::FakeFrameBuilder()
                          .Id(2)
                          .PayloadType(99)
                          .Time(RtpTimestampForFrame(2))
                          .Refs({1})
                          .AsLast()
                          .Build();

  // Expect frames are decoded in order despite delta_frame1 arriving first.
  InSequence seq;
  EXPECT_CALL(mock_decoder_,
              Decode(test::RtpTimestamp(kFirstRtpTimestamp), _, _))
      .Times(1);
  EXPECT_CALL(mock_decoder_,
              Decode(test::RtpTimestamp(RtpTimestampForFrame(1)), _, _))
      .Times(1);
  EXPECT_CALL(mock_decoder_,
              Decode(test::RtpTimestamp(RtpTimestampForFrame(2)), _, _))
      .Times(1);
  key_frame->SetReceivedTime(clock_->CurrentTime().ms());
  video_receive_stream_->OnCompleteFrame(std::move(key_frame));
  EXPECT_THAT(fake_renderer_.WaitForFrame(TimeDelta::Zero()), RenderedFrame());

  delta_frame2->SetReceivedTime(clock_->CurrentTime().ms());
  video_receive_stream_->OnCompleteFrame(std::move(delta_frame2));
  EXPECT_THAT(fake_renderer_.WaitForFrame(k30FpsDelay), DidNotReceiveFrame());
  // `delta_frame1` arrives late.
  delta_frame1->SetReceivedTime(clock_->CurrentTime().ms());
  video_receive_stream_->OnCompleteFrame(std::move(delta_frame1));
  EXPECT_THAT(fake_renderer_.WaitForFrame(k30FpsDelay), RenderedFrame());
  EXPECT_THAT(fake_renderer_.WaitForFrame(k30FpsDelay * 2), RenderedFrame());
  video_receive_stream_->Stop();
}

TEST_P(VideoReceiveStream2Test, WaitsforAllSpatialLayers) {
  video_receive_stream_->Start();
  auto sl0 = test::FakeFrameBuilder()
                 .Id(0)
                 .PayloadType(99)
                 .Time(kFirstRtpTimestamp)
                 .ReceivedTime(kStartTime)
                 .Build();
  auto sl1 = test::FakeFrameBuilder()
                 .Id(1)
                 .PayloadType(99)
                 .ReceivedTime(kStartTime)
                 .Time(kFirstRtpTimestamp)
                 .Refs({0})
                 .Build();
  auto sl2 = test::FakeFrameBuilder()
                 .Id(2)
                 .PayloadType(99)
                 .ReceivedTime(kStartTime)
                 .Time(kFirstRtpTimestamp)
                 .Refs({0, 1})
                 .AsLast()
                 .Build();

  // No decodes should be called until `sl2` is received.
  EXPECT_CALL(mock_decoder_, Decode).Times(0);
  sl0->SetReceivedTime(clock_->CurrentTime().ms());
  video_receive_stream_->OnCompleteFrame(std::move(sl0));
  EXPECT_THAT(fake_renderer_.WaitForFrame(TimeDelta::Zero()),
              DidNotReceiveFrame());
  video_receive_stream_->OnCompleteFrame(std::move(sl1));
  EXPECT_THAT(fake_renderer_.WaitForFrame(TimeDelta::Zero()),
              DidNotReceiveFrame());
  // When `sl2` arrives decode should happen.
  EXPECT_CALL(mock_decoder_,
              Decode(test::RtpTimestamp(kFirstRtpTimestamp), _, _))
      .Times(1);
  video_receive_stream_->OnCompleteFrame(std::move(sl2));
  EXPECT_THAT(fake_renderer_.WaitForFrame(TimeDelta::Zero()), RenderedFrame());
  video_receive_stream_->Stop();
}

TEST_P(VideoReceiveStream2Test, FramesFastForwardOnSystemHalt) {
  video_receive_stream_->Start();

  // The frame structure looks like this,
  //   F1
  //   /
  // F0 --> F2
  //
  // In this case we will have a system halt simulated. By the time the system
  // resumes, F1 will be old and so F2 should be decoded.
  auto key_frame = test::FakeFrameBuilder()
                       .Id(0)
                       .PayloadType(99)
                       .Time(kFirstRtpTimestamp)
                       .AsLast()
                       .Build();
  auto ffwd_frame = test::FakeFrameBuilder()
                        .Id(1)
                        .PayloadType(99)
                        .Time(RtpTimestampForFrame(1))
                        .Refs({0})
                        .AsLast()
                        .Build();
  auto rendered_frame = test::FakeFrameBuilder()
                            .Id(2)
                            .PayloadType(99)
                            .Time(RtpTimestampForFrame(2))
                            .Refs({0})
                            .AsLast()
                            .Build();
  InSequence seq;
  EXPECT_CALL(mock_decoder_,
              Decode(test::RtpTimestamp(kFirstRtpTimestamp), _, _))
      .WillOnce(testing::DoAll(testing::Invoke([&] {
                                 // System halt will be simulated in the decode.
                                 time_controller_.AdvanceTime(k30FpsDelay * 2);
                               }),
                               DefaultDecodeAction()));
  EXPECT_CALL(mock_decoder_,
              Decode(test::RtpTimestamp(RtpTimestampForFrame(2)), _, _));
  video_receive_stream_->OnCompleteFrame(std::move(key_frame));
  video_receive_stream_->OnCompleteFrame(std::move(ffwd_frame));
  video_receive_stream_->OnCompleteFrame(std::move(rendered_frame));
  EXPECT_THAT(fake_renderer_.WaitForFrame(TimeDelta::Zero()), RenderedFrame());
  EXPECT_THAT(fake_renderer_.WaitForFrame(TimeDelta::Zero()), RenderedFrame());

  // Check stats show correct dropped frames.
  auto stats = video_receive_stream_->GetStats();
  EXPECT_EQ(stats.frames_dropped, 1u);

  video_receive_stream_->Stop();
}

TEST_P(VideoReceiveStream2Test, BetterFrameInsertedWhileWaitingToDecodeFrame) {
  video_receive_stream_->Start();

  auto key_frame = test::FakeFrameBuilder()
                       .Id(0)
                       .PayloadType(99)
                       .Time(kFirstRtpTimestamp)
                       .ReceivedTime(ReceiveTimeForFrame(0))
                       .AsLast()
                       .Build();
  auto f1 = test::FakeFrameBuilder()
                .Id(1)
                .PayloadType(99)
                .Time(RtpTimestampForFrame(1))
                .ReceivedTime(ReceiveTimeForFrame(1))
                .Refs({0})
                .AsLast()
                .Build();
  auto f2 = test::FakeFrameBuilder()
                .Id(2)
                .PayloadType(99)
                .Time(RtpTimestampForFrame(2))
                .ReceivedTime(ReceiveTimeForFrame(2))
                .Refs({0})
                .AsLast()
                .Build();

  video_receive_stream_->OnCompleteFrame(std::move(key_frame));
  EXPECT_THAT(fake_renderer_.WaitForFrame(TimeDelta::Zero()), RenderedFrame());

  InSequence seq;
  EXPECT_CALL(mock_decoder_,
              Decode(test::RtpTimestamp(RtpTimestampForFrame(1)), _, _))
      .Times(1);
  EXPECT_CALL(mock_decoder_,
              Decode(test::RtpTimestamp(RtpTimestampForFrame(2)), _, _))
      .Times(1);
  // Simulate f1 arriving after f2 but before f2 is decoded.
  video_receive_stream_->OnCompleteFrame(std::move(f2));
  EXPECT_THAT(fake_renderer_.WaitForFrame(k30FpsDelay), DidNotReceiveFrame());
  video_receive_stream_->OnCompleteFrame(std::move(f1));
  EXPECT_THAT(fake_renderer_.WaitForFrame(k30FpsDelay), RenderedFrame());
  EXPECT_THAT(fake_renderer_.WaitForFrame(k30FpsDelay), RenderedFrame());

  video_receive_stream_->Stop();
}

// Note: This test takes a long time (~10s) to run if the fake metronome is
// active. Since the test needs to wait for the timestamp to rollover, it has a
// fake delay of around 6.5 hours. Even though time is simulated, this will be
// around 1,500,000 metronome tick invocations.
TEST_P(VideoReceiveStream2Test, RtpTimestampWrapAround) {
  video_receive_stream_->Start();

  constexpr uint32_t kBaseRtp = std::numeric_limits<uint32_t>::max() / 2;
  video_receive_stream_->OnCompleteFrame(
      test::FakeFrameBuilder()
          .Id(0)
          .PayloadType(99)
          .Time(kBaseRtp)
          .ReceivedTime(clock_->CurrentTime())
          .AsLast()
          .Build());
  EXPECT_THAT(fake_renderer_.WaitForFrame(TimeDelta::Zero()), RenderedFrame());
  time_controller_.AdvanceTime(k30FpsDelay);
  video_receive_stream_->OnCompleteFrame(
      test::FakeFrameBuilder()
          .Id(1)
          .PayloadType(99)
          .Time(kBaseRtp + k30FpsRtpTimestampDelta)
          .ReceivedTime(clock_->CurrentTime())
          .AsLast()
          .Build());
  EXPECT_THAT(fake_renderer_.WaitForFrame(k30FpsDelay), RenderedFrame());

  // Pause stream so that RTP timestamp wraps around.
  constexpr uint32_t kLastRtp = kBaseRtp + k30FpsRtpTimestampDelta;
  constexpr uint32_t kWrapAroundRtp =
      kLastRtp + std::numeric_limits<uint32_t>::max() / 2 + 1;
  // Pause for corresponding delay such that RTP timestamp would increase this
  // much at 30fps.
  constexpr TimeDelta kWrapAroundDelay =
      (std::numeric_limits<uint32_t>::max() / 2 + 1) / kRtpTimestampHz;

  time_controller_.AdvanceTime(kWrapAroundDelay);
  video_receive_stream_->OnCompleteFrame(
      test::FakeFrameBuilder()
          .Id(2)
          .PayloadType(99)
          .Time(kWrapAroundRtp)
          .ReceivedTime(clock_->CurrentTime())
          .AsLast()
          .Build());
  EXPECT_CALL(mock_decoder_, Decode(test::RtpTimestamp(kWrapAroundRtp), _, _))
      .Times(1);
  EXPECT_THAT(fake_renderer_.WaitForFrame(TimeDelta::Zero()), RenderedFrame());

  video_receive_stream_->Stop();
}

INSTANTIATE_TEST_SUITE_P(VideoReceiveStream2Test,
                         VideoReceiveStream2Test,
                         testing::Bool(),
                         [](const auto& test_param_info) {
                           rtc::StringBuilder ss;
                           ss << (test_param_info.param
                                      ? "ScheduleDecodesWithMetronome"
                                      : "ScheduleDecodesWithPostTask");
                           return ss.Release();
                         });

}  // namespace webrtc
