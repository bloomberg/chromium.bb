// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/receiver.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <vector>

#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/api/logging.h"
#include "platform/api/time.h"
#include "platform/api/udp_packet.h"
#include "platform/api/udp_socket.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "streaming/cast/compound_rtcp_parser.h"
#include "streaming/cast/constants.h"
#include "streaming/cast/encoded_frame.h"
#include "streaming/cast/frame_crypto.h"
#include "streaming/cast/receiver_packet_router.h"
#include "streaming/cast/rtcp_common.h"
#include "streaming/cast/rtcp_session.h"
#include "streaming/cast/rtp_defines.h"
#include "streaming/cast/rtp_packetizer.h"
#include "streaming/cast/rtp_time.h"
#include "streaming/cast/sender_report_builder.h"
#include "streaming/cast/ssrc.h"

using openscreen::platform::Clock;
using openscreen::platform::FakeClock;
using openscreen::platform::FakeTaskRunner;
using openscreen::platform::TaskRunner;
using openscreen::platform::UdpPacket;
using openscreen::platform::UdpSocket;

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::seconds;

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::SaveArg;

namespace openscreen {
namespace cast_streaming {
namespace {

// Receiver configuration.
constexpr Ssrc kSenderSsrc = 1;
constexpr Ssrc kReceiverSsrc = 2;
constexpr int kRtpTimebase = 48000;
constexpr milliseconds kTargetPlayoutDelay{100};
constexpr auto kAesKey =
    std::array<uint8_t, 16>{{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                             0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f}};
constexpr auto kCastIvMask =
    std::array<uint8_t, 16>{{0xf0, 0xe0, 0xd0, 0xc0, 0xb0, 0xa0, 0x90, 0x80,
                             0x70, 0x60, 0x50, 0x40, 0x30, 0x20, 0x10, 0x00}};

// Additional configuration for the Sender.
constexpr RtpPayloadType kRtpPayloadType = RtpPayloadType::kVideoVp8;
constexpr int kMaxRtpPacketSize = 64;

// A simulated one-way network delay.
constexpr auto kOneWayNetworkDelay = milliseconds(23);

// An EncodedFrame for unit testing, one of a sequence of simulated frames, each
// of 10 ms duration. The first frame will be a key frame; and any later frames
// will be non-key, dependent on the prior frame. Frame 5 (the 6th frame in the
// zero-based sequence) will include a target playout delay change, an increase
// to 800 ms. Frames with different IDs will contain vary in their payload data
// size, but are always 2 or more packets' worth of data.
struct SimulatedFrame : public EncodedFrame {
  static constexpr milliseconds kFrameDuration = milliseconds(10);
  static constexpr milliseconds kTargetPlayoutDelayChange = milliseconds(800);

  SimulatedFrame(Clock::time_point first_frame_reference_time, int which) {
    frame_id = FrameId::first() + which;
    if (which == 0) {
      dependency = EncodedFrame::KEY_FRAME;
      referenced_frame_id = frame_id;
    } else {
      dependency = EncodedFrame::DEPENDS_ON_ANOTHER;
      referenced_frame_id = frame_id - 1;
    }
    rtp_timestamp =
        GetRtpStartTime() +
        RtpTimeDelta::FromDuration(kFrameDuration * which, kRtpTimebase);
    reference_time = first_frame_reference_time + kFrameDuration * which;
    if (which == 5) {
      new_playout_delay = kTargetPlayoutDelayChange;
    }
    constexpr int kAdditionalBytesEachSuccessiveFrame = 3;
    buffer_.resize(2 * kMaxRtpPacketSize +
                   which * kAdditionalBytesEachSuccessiveFrame);
    for (size_t i = 0; i < buffer_.size(); ++i) {
      buffer_[i] = static_cast<uint8_t>(which + static_cast<int>(i));
    }
    data = absl::Span<uint8_t>(buffer_);
  }

  static RtpTimeTicks GetRtpStartTime() {
    return RtpTimeTicks::FromTimeSinceOrigin(seconds(0), kRtpTimebase);
  }

 private:
  std::vector<uint8_t> buffer_;
};

// static
constexpr milliseconds SimulatedFrame::kFrameDuration;
constexpr milliseconds SimulatedFrame::kTargetPlayoutDelayChange;

// Processes packets from the Receiver under test, as a real Sender might, and
// allows the unit tests to set expectations on events of interest to confirm
// proper behavior of the Receiver.
class MockSender : public CompoundRtcpParser::Client {
 public:
  MockSender(TaskRunner* task_runner, UdpSocket::Client* receiver)
      : task_runner_(task_runner),
        receiver_(receiver),
        sender_endpoint_{
            // Use a random IPv6 address in the range reserved for
            // "documentation purposes." Thus, the following is a fake address
            // that should be blocked by the OS (and all network packet
            // routers). But, these tests don't use real sockets, so...
            IPAddress::Parse("2001:db8:0d93:69c2:fd1a:49a6:a7c0:e8a6").value(),
            2344},
        rtcp_session_(kSenderSsrc, kReceiverSsrc, FakeClock::now()),
        sender_report_builder_(&rtcp_session_),
        rtcp_parser_(&rtcp_session_, this),
        crypto_(kAesKey, kCastIvMask),
        rtp_packetizer_(kRtpPayloadType, kSenderSsrc, kMaxRtpPacketSize) {}

  ~MockSender() override = default;

  void set_max_feedback_frame_id(FrameId f) { max_feedback_frame_id_ = f; }

  // Called by the test procedures to generate a Sender Report containing the
  // given lip-sync timestamps, and send it to the Receiver. The caller must
  // spin the TaskRunner for the RTCP packet to be delivered to the Receiver.
  StatusReportId SendSenderReport(Clock::time_point reference_time,
                                  RtpTimeTicks rtp_timestamp) {
    // Generate the Sender Report RTCP packet.
    uint8_t buffer[kMaxRtpPacketSizeForIpv4UdpOnEthernet];
    RtcpSenderReport sender_report;
    sender_report.reference_time = reference_time;
    sender_report.rtp_timestamp = rtp_timestamp;
    const auto packet_and_report_id =
        sender_report_builder_.BuildPacket(sender_report, buffer);

    // Send the RTCP packet as a UdpPacket directly to the Receiver instance.
    UdpPacket packet_to_send;
    packet_to_send.set_source(sender_endpoint_);
    packet_to_send.assign(packet_and_report_id.first.begin(),
                          packet_and_report_id.first.end());
    task_runner_->PostTask(
        [receiver = receiver_, packet = std::move(packet_to_send)]() mutable {
          receiver->OnRead(nullptr, ErrorOr<UdpPacket>(std::move(packet)));
        });

    return packet_and_report_id.second;
  }

  // Sets which frame is currently being sent by this MockSender. Test code must
  // call SendRtpPackets() to send the packets.
  void SetFrameBeingSent(const EncodedFrame& frame) {
    frame_being_sent_ = crypto_.Encrypt(frame);
  }

  // Returns a vector containing each packet ID once (of the current frame being
  // sent). |permutation| controls the sort order of the vector: zero will
  // provide all the packet IDs in order, and greater values will provide them
  // in a different, predictable order.
  std::vector<FramePacketId> GetAllPacketIds(int permutation) {
    const int num_packets =
        rtp_packetizer_.ComputeNumberOfPackets(frame_being_sent_);
    OSP_CHECK_GT(num_packets, 0);
    std::vector<FramePacketId> ids;
    ids.reserve(num_packets);
    const FramePacketId last_packet_id =
        static_cast<FramePacketId>(num_packets - 1);
    for (FramePacketId packet_id = 0; packet_id <= last_packet_id;
         ++packet_id) {
      ids.push_back(packet_id);
    }
    for (int i = 0; i < permutation; ++i) {
      std::next_permutation(ids.begin(), ids.end());
    }
    return ids;
  }

  // Send the specified packets of the current frame being sent.
  void SendRtpPackets(const std::vector<FramePacketId>& packets_to_send) {
    uint8_t buffer[kMaxRtpPacketSize];
    for (FramePacketId packet_id : packets_to_send) {
      const auto span =
          rtp_packetizer_.GeneratePacket(frame_being_sent_, packet_id, buffer);
      UdpPacket packet_to_send;
      packet_to_send.set_source(sender_endpoint_);
      packet_to_send.assign(span.begin(), span.end());
      task_runner_->PostTask(
          [receiver = receiver_, packet = std::move(packet_to_send)]() mutable {
            receiver->OnRead(nullptr, ErrorOr<UdpPacket>(std::move(packet)));
          });
    }
  }

  // Called to process a packet from the Receiver.
  void OnPacketFromReceiver(absl::Span<const uint8_t> packet) {
    EXPECT_TRUE(rtcp_parser_.Parse(packet, max_feedback_frame_id_));
  }

  // CompoundRtcpParser::Client implementation: Tests set expectations on these
  // mocks to confirm that the receiver is providing the right data to the
  // sender in its RTCP packets.
  MOCK_METHOD1(OnReceiverReferenceTimeAdvanced,
               void(Clock::time_point reference_time));
  MOCK_METHOD1(OnReceiverReport, void(const RtcpReportBlock& receiver_report));
  MOCK_METHOD0(OnReceiverIndicatesPictureLoss, void());
  MOCK_METHOD2(OnReceiverCheckpoint,
               void(FrameId frame_id, milliseconds playout_delay));
  MOCK_METHOD1(OnReceiverHasFrames, void(std::vector<FrameId> acks));
  MOCK_METHOD1(OnReceiverIsMissingPackets, void(std::vector<PacketNack> nacks));

 private:
  TaskRunner* const task_runner_;
  UdpSocket::Client* const receiver_;
  const IPEndpoint sender_endpoint_;
  RtcpSession rtcp_session_;
  SenderReportBuilder sender_report_builder_;
  CompoundRtcpParser rtcp_parser_;
  FrameCrypto crypto_;
  RtpPacketizer rtp_packetizer_;

  FrameId max_feedback_frame_id_ = FrameId::first() + kMaxUnackedFrames;

  EncryptedFrame frame_being_sent_;
};

// An Environment that can intercept all packet sends. ReceiverTest will connect
// the SendPacket() method calls to the MockSender.
class MockEnvironment : public Environment {
 public:
  MockEnvironment(platform::ClockNowFunctionPtr now_function,
                  platform::TaskRunner* task_runner)
      : Environment(now_function, task_runner) {}

  ~MockEnvironment() override = default;

  // Used for intercepting packet sends from the implementation under test.
  MOCK_METHOD1(SendPacket, void(absl::Span<const uint8_t> packet));
};

class MockConsumer : public Receiver::Consumer {
 public:
  MOCK_METHOD0(OnFramesComplete, void());
};

class ReceiverTest : public testing::Test {
 public:
  ReceiverTest()
      : clock_(Clock::now()),
        task_runner_(&clock_),
        env_(&FakeClock::now, &task_runner_),
        packet_router_(&env_),
        receiver_(&env_,
                  &packet_router_,
                  kSenderSsrc,
                  kReceiverSsrc,
                  kRtpTimebase,
                  kTargetPlayoutDelay,
                  kAesKey,
                  kCastIvMask),
        sender_(&task_runner_, &env_) {
    env_.set_socket_error_handler(
        [](Error error) { ASSERT_TRUE(error.ok()) << error; });
    ON_CALL(env_, SendPacket(_))
        .WillByDefault(Invoke(&sender_, &MockSender::OnPacketFromReceiver));
    receiver_.SetConsumer(&consumer_);
  }

  ~ReceiverTest() override = default;

  Receiver* receiver() { return &receiver_; }
  MockSender* sender() { return &sender_; }
  MockConsumer* consumer() { return &consumer_; }

  void AdvanceClockAndRunTasks(Clock::duration delta) { clock_.Advance(delta); }
  void RunTasksUntilIdle() { task_runner_.RunTasksUntilIdle(); }

 private:
  FakeClock clock_;
  FakeTaskRunner task_runner_;
  testing::NiceMock<MockEnvironment> env_;
  ReceiverPacketRouter packet_router_;
  Receiver receiver_;
  testing::NiceMock<MockSender> sender_;
  testing::NiceMock<MockConsumer> consumer_;
};

// Tests that the Receiver processes RTCP packets correctly and sends RTCP
// reports at regular intervals.
TEST_F(ReceiverTest, ReceivesAndSendsRtcpPackets) {
  // Sender-side expectations, after the Receiver has processed the first Sender
  // Report.
  Clock::time_point receiver_reference_time{};
  EXPECT_CALL(*sender(), OnReceiverReferenceTimeAdvanced(_))
      .WillOnce(SaveArg<0>(&receiver_reference_time));
  RtcpReportBlock receiver_report;
  EXPECT_CALL(*sender(), OnReceiverReport(_))
      .WillOnce(SaveArg<0>(&receiver_report));
  EXPECT_CALL(*sender(),
              OnReceiverCheckpoint(FrameId::first() - 1, kTargetPlayoutDelay))
      .Times(1);

  // Have the MockSender send a Sender Report with lip-sync timing information.
  const Clock::time_point sender_reference_time = FakeClock::now();
  const RtpTimeTicks sender_rtp_timestamp =
      RtpTimeTicks::FromTimeSinceOrigin(seconds(1), kRtpTimebase);
  const StatusReportId sender_report_id =
      sender()->SendSenderReport(sender_reference_time, sender_rtp_timestamp);
  AdvanceClockAndRunTasks(kOneWayNetworkDelay);

  // Expect the MockSender got back a Receiver Report that includes its SSRC and
  // the last Sender Report ID.
  testing::Mock::VerifyAndClearExpectations(sender());
  EXPECT_EQ(kSenderSsrc, receiver_report.ssrc);
  EXPECT_EQ(sender_report_id, receiver_report.last_status_report_id);

  // Confirm the clock offset math: Since the Receiver and MockSender share the
  // same underlying FakeClock, the Receiver should be 10ms ahead of the Sender,
  // which reflects the simulated one-way network packet travel time (of the
  // Sender Report).
  //
  // Note: The offset can be affected by the lossy conversion when going to and
  // from the wire-format NtpTimestamps. See the unit tests in
  // ntp_time_unittest.cc for further discussion.
  constexpr auto kAllowedNtpRoundingError = microseconds(2);
  EXPECT_NEAR(duration_cast<microseconds>(kOneWayNetworkDelay).count(),
              duration_cast<microseconds>(receiver_reference_time -
                                          sender_reference_time)
                  .count(),
              kAllowedNtpRoundingError.count());

  // Without the Sender doing anything, the Receiver should continue providing
  // RTCP reports at regular intervals. Simulate three intervals of time,
  // verifying that the Receiver did send reports.
  Clock::time_point last_receiver_reference_time = receiver_reference_time;
  for (int i = 0; i < 3; ++i) {
    receiver_reference_time = Clock::time_point();
    EXPECT_CALL(*sender(), OnReceiverReferenceTimeAdvanced(_))
        .WillRepeatedly(SaveArg<0>(&receiver_reference_time));
    AdvanceClockAndRunTasks(kRtcpReportInterval);
    testing::Mock::VerifyAndClearExpectations(sender());
    EXPECT_LT(last_receiver_reference_time, receiver_reference_time);
    last_receiver_reference_time = receiver_reference_time;
  }
}

// Tests that the Receiver processes RTP packets, which might arrive in-order or
// out of order, but such that each frame is completely received in-order. Also,
// confirms that target playout delay changes are processed/applied correctly.
TEST_F(ReceiverTest, ReceivesFramesInOrder) {
  // Send the initial Sender Report with lip-sync timing information to
  // "unblock" the Receiver.
  const Clock::time_point start_time = FakeClock::now();
  sender()->SendSenderReport(start_time, SimulatedFrame::GetRtpStartTime());
  AdvanceClockAndRunTasks(kOneWayNetworkDelay);

  EXPECT_CALL(*consumer(), OnFramesComplete()).Times(10);
  for (int i = 0; i <= 9; ++i) {
    EXPECT_CALL(*sender(), OnReceiverCheckpoint(FrameId::first() + i, _))
        .Times(1);
    EXPECT_CALL(*sender(), OnReceiverIsMissingPackets(_)).Times(0);

    sender()->SetFrameBeingSent(SimulatedFrame(start_time, i));
    // Send the frame's packets in-order half the time, out-of-order the other
    // half.
    const int permutation = (i % 2) ? i : 0;
    sender()->SendRtpPackets(sender()->GetAllPacketIds(permutation));

    // The Receiver should immediately ACK once it has received all the RTP
    // packets to complete the frame.
    RunTasksUntilIdle();
    testing::Mock::VerifyAndClearExpectations(sender());
  }

  // TODO(miu): In a soon-upcoming commit: Use AdvanceToNextFrame() and
  // ConsumeNextFrame() to extract the 10 frames out of the Receiver, and ensure
  // their data and metadata match expectations.

  // When the Receiver has all of the frames and they are complete, it should
  // send out a low-frequency periodic RTCP "ping." Verify that there is one and
  // only one "ping" sent when the clock moves forward by one default report
  // interval during a period of inactivity.
  EXPECT_CALL(*sender(), OnReceiverCheckpoint(FrameId::first() + 9, _))
      .Times(1);
  AdvanceClockAndRunTasks(kRtcpReportInterval);
  testing::Mock::VerifyAndClearExpectations(sender());
}

// Tests that the Receiver processes RTP packets, can receive frames out of
// order, and issues the appropriate ACK/NACK feedback to the Sender as it
// realizes what it has and what it's missing.
TEST_F(ReceiverTest, ReceivesFramesOutOfOrder) {
  // Send the initial Sender Report with lip-sync timing information to
  // "unblock" the Receiver.
  const Clock::time_point start_time = FakeClock::now();
  sender()->SendSenderReport(start_time, SimulatedFrame::GetRtpStartTime());
  AdvanceClockAndRunTasks(kOneWayNetworkDelay);

  constexpr static int kOutOfOrderFrames[] = {3, 4, 2, 0, 1};
  for (int i : kOutOfOrderFrames) {
    // Expectations are different as each frame is sent and received.
    switch (i) {
      case 3: {
        // Note that frame 4 will not yet be known to the Receiver, and so it
        // should not be mentioned in any of the feedback for this case.
        EXPECT_CALL(*sender(), OnReceiverCheckpoint(FrameId::first() - 1, _))
            .Times(AtLeast(1));
        EXPECT_CALL(
            *sender(),
            OnReceiverHasFrames(std::vector<FrameId>({FrameId::first() + 3})))
            .Times(AtLeast(1));
        EXPECT_CALL(*sender(),
                    OnReceiverIsMissingPackets(std::vector<PacketNack>({
                        PacketNack{FrameId::first(), kAllPacketsLost},
                        PacketNack{FrameId::first() + 1, kAllPacketsLost},
                        PacketNack{FrameId::first() + 2, kAllPacketsLost},
                    })))
            .Times(AtLeast(1));
        // The consumer should never be notified, since frame 0 (the first
        // frame) must be completed before consumption of any frames can happen.
        EXPECT_CALL(*consumer(), OnFramesComplete()).Times(0);
        break;
      }

      case 4: {
        EXPECT_CALL(*sender(), OnReceiverCheckpoint(FrameId::first() - 1, _))
            .Times(AtLeast(1));
        EXPECT_CALL(*sender(),
                    OnReceiverHasFrames(std::vector<FrameId>(
                        {FrameId::first() + 3, FrameId::first() + 4})))
            .Times(AtLeast(1));
        EXPECT_CALL(*sender(),
                    OnReceiverIsMissingPackets(std::vector<PacketNack>({
                        PacketNack{FrameId::first(), kAllPacketsLost},
                        PacketNack{FrameId::first() + 1, kAllPacketsLost},
                        PacketNack{FrameId::first() + 2, kAllPacketsLost},
                    })))
            .Times(AtLeast(1));
        EXPECT_CALL(*consumer(), OnFramesComplete()).Times(0);
        break;
      }

      case 2: {
        EXPECT_CALL(*sender(), OnReceiverCheckpoint(FrameId::first() - 1, _))
            .Times(AtLeast(1));
        EXPECT_CALL(*sender(), OnReceiverHasFrames(std::vector<FrameId>(
                                   {FrameId::first() + 2, FrameId::first() + 3,
                                    FrameId::first() + 4})))
            .Times(AtLeast(1));
        EXPECT_CALL(*sender(),
                    OnReceiverIsMissingPackets(std::vector<PacketNack>({
                        PacketNack{FrameId::first(), kAllPacketsLost},
                        PacketNack{FrameId::first() + 1, kAllPacketsLost},
                    })))
            .Times(AtLeast(1));
        EXPECT_CALL(*consumer(), OnFramesComplete()).Times(0);
        break;
      }

      case 0: {
        EXPECT_CALL(*sender(), OnReceiverCheckpoint(FrameId::first(), _))
            .Times(AtLeast(1));
        EXPECT_CALL(*sender(), OnReceiverHasFrames(std::vector<FrameId>(
                                   {FrameId::first() + 2, FrameId::first() + 3,
                                    FrameId::first() + 4})))
            .Times(AtLeast(1));
        EXPECT_CALL(*sender(),
                    OnReceiverIsMissingPackets(std::vector<PacketNack>(
                        {PacketNack{FrameId::first() + 1, kAllPacketsLost}})))
            .Times(AtLeast(1));
        EXPECT_CALL(*consumer(), OnFramesComplete()).Times(1);
        break;
      }

      case 1: {
        EXPECT_CALL(*sender(), OnReceiverCheckpoint(FrameId::first() + 4, _))
            .Times(AtLeast(1));
        EXPECT_CALL(*sender(), OnReceiverHasFrames(_)).Times(0);
        EXPECT_CALL(*sender(), OnReceiverIsMissingPackets(_)).Times(0);
        EXPECT_CALL(*consumer(), OnFramesComplete()).Times(1);
        break;
      }

      default:
        OSP_NOTREACHED();
    }

    sender()->SetFrameBeingSent(SimulatedFrame(start_time, i));
    sender()->SendRtpPackets(sender()->GetAllPacketIds(i));

    // While there are known incomplete frames, the Receiver should send RTCP
    // packets more frequently than the default "ping" interval. Thus, advancing
    // the clock by this much should result in several feedback reports
    // transmitted to the Sender.
    AdvanceClockAndRunTasks(kRtcpReportInterval);

    testing::Mock::VerifyAndClearExpectations(sender());
    testing::Mock::VerifyAndClearExpectations(consumer());
  }

  // TODO(miu): In a soon-upcoming commit: Use AdvanceToNextFrame() and
  // ConsumeNextFrame() to extract the 10 frames out of the Receiver, and ensure
  // their data and metadata match expectations.
}

// Tests that the Receiver will respond to a key frame request from its client
// by sending a Picture Loss Indicator (PLI) to the Sender, and then will
// automatically stop sending the PLI once a key frame has been received.
TEST_F(ReceiverTest, RequestsKeyFrameToRectifyPictureLoss) {
  // Send the initial Sender Report with lip-sync timing information to
  // "unblock" the Receiver.
  const Clock::time_point start_time = FakeClock::now();
  sender()->SendSenderReport(start_time, SimulatedFrame::GetRtpStartTime());
  AdvanceClockAndRunTasks(kOneWayNetworkDelay);

  // Send and Receive three frames in-order, normally.
  EXPECT_CALL(*consumer(), OnFramesComplete()).Times(3);
  for (int i = 0; i <= 2; ++i) {
    EXPECT_CALL(*sender(), OnReceiverCheckpoint(FrameId::first() + i, _))
        .Times(1);
    sender()->SetFrameBeingSent(SimulatedFrame(start_time, i));
    sender()->SendRtpPackets(sender()->GetAllPacketIds(0));
    RunTasksUntilIdle();
    testing::Mock::VerifyAndClearExpectations(sender());
  }
  testing::Mock::VerifyAndClearExpectations(consumer());

  // Simulate the Consumer requesting a key frame after picture loss (e.g., a
  // decoder failure). Ensure the Sender is immediately notified.
  EXPECT_CALL(*sender(), OnReceiverIndicatesPictureLoss()).Times(1);
  receiver()->RequestKeyFrame();
  RunTasksUntilIdle();
  testing::Mock::VerifyAndClearExpectations(sender());

  // The Sender sends another frame that is not a key frame and, upon receipt,
  // the Receiver should repeat its "cry" for a key frame.
  EXPECT_CALL(*consumer(), OnFramesComplete()).Times(1);
  EXPECT_CALL(*sender(), OnReceiverCheckpoint(FrameId::first() + 3, _))
      .Times(1);
  EXPECT_CALL(*sender(), OnReceiverIndicatesPictureLoss()).Times(1);
  sender()->SetFrameBeingSent(SimulatedFrame(start_time, 3));
  sender()->SendRtpPackets(sender()->GetAllPacketIds(0));
  RunTasksUntilIdle();
  testing::Mock::VerifyAndClearExpectations(sender());
  testing::Mock::VerifyAndClearExpectations(consumer());

  // Finally, the Sender responds to the PLI condition by sending a key frame.
  // Confirm the Receiver has stopped indicating picture loss after having
  // received the key frame.
  EXPECT_CALL(*consumer(), OnFramesComplete()).Times(1);
  EXPECT_CALL(*sender(), OnReceiverCheckpoint(FrameId::first() + 4, _))
      .Times(1);
  EXPECT_CALL(*sender(), OnReceiverIndicatesPictureLoss()).Times(0);
  SimulatedFrame frame(start_time, 4);
  frame.dependency = EncodedFrame::KEY_FRAME;
  frame.referenced_frame_id = frame.frame_id;
  sender()->SetFrameBeingSent(frame);
  sender()->SendRtpPackets(sender()->GetAllPacketIds(0));
  RunTasksUntilIdle();
  testing::Mock::VerifyAndClearExpectations(sender());
  testing::Mock::VerifyAndClearExpectations(consumer());
}

// Tests that the Receiver will start dropping packets once its frame queue is
// full (i.e., when the consumer is not pulling them out of the queue). Since
// the Receiver will stop ACK'ing frames, the Sender will become stalled.
TEST_F(ReceiverTest, EatsItsFill) {
  // Send the initial Sender Report with lip-sync timing information to
  // "unblock" the Receiver.
  const Clock::time_point start_time = FakeClock::now();
  sender()->SendSenderReport(start_time, SimulatedFrame::GetRtpStartTime());
  AdvanceClockAndRunTasks(kOneWayNetworkDelay);

  // Send and Receive the maximum possible number of frames in-order, normally.
  EXPECT_CALL(*consumer(), OnFramesComplete()).Times(kMaxUnackedFrames);
  for (int i = 0; i < kMaxUnackedFrames; ++i) {
    EXPECT_CALL(*sender(), OnReceiverCheckpoint(FrameId::first() + i, _))
        .Times(1);
    sender()->SetFrameBeingSent(SimulatedFrame(start_time, i));
    sender()->SendRtpPackets(sender()->GetAllPacketIds(0));
    RunTasksUntilIdle();
    testing::Mock::VerifyAndClearExpectations(sender());
  }
  testing::Mock::VerifyAndClearExpectations(consumer());

  // Sending one more frame should be ignored. Over and over. None of the
  // feedback reports from the Receiver should indicate it is collecting packets
  // for future frames.
  constexpr int kIgnoredFrame = kMaxUnackedFrames;
  for (int i = 0; i < 5; ++i) {
    EXPECT_CALL(*consumer(), OnFramesComplete()).Times(0);
    EXPECT_CALL(*sender(),
                OnReceiverCheckpoint(FrameId::first() + (kIgnoredFrame - 1), _))
        .Times(AtLeast(1));
    EXPECT_CALL(*sender(), OnReceiverIsMissingPackets(_)).Times(0);
    sender()->SetFrameBeingSent(SimulatedFrame(start_time, kIgnoredFrame));
    sender()->SendRtpPackets(sender()->GetAllPacketIds(0));
    AdvanceClockAndRunTasks(kRtcpReportInterval);
    testing::Mock::VerifyAndClearExpectations(sender());
    testing::Mock::VerifyAndClearExpectations(consumer());
  }

  // TODO(miu): In a soon-upcoming commit: Use AdvanceToNextFrame() and
  // ConsumeNextFrame() to start extracting frames out of the Receiver, and
  // ensure the queue drains and the Receiver resumes collecting packets for
  // later frames.
}

}  // namespace
}  // namespace cast_streaming
}  // namespace openscreen
