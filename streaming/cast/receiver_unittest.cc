// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/receiver.h"

#include <stdint.h>

#include <array>

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
#include "streaming/cast/receiver_packet_router.h"
#include "streaming/cast/rtcp_common.h"
#include "streaming/cast/rtcp_session.h"
#include "streaming/cast/rtp_defines.h"
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
        rtcp_parser_(&rtcp_session_, this) {}

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

  // Called to process a packet from the Receiver.
  void OnPacketFromReceiver(absl::Span<const uint8_t> packet) {
    EXPECT_TRUE(rtcp_parser_.Parse(packet, max_feedback_frame_id_));
    // TODO(miu): This will be expanded to detect/handle RTP packets in future
    // commits.
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

  FrameId max_feedback_frame_id_ = FrameId::first() + kMaxUnackedFrames;
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
  }

  ~ReceiverTest() override = default;

  Receiver* receiver() { return &receiver_; }
  MockSender* sender() { return &sender_; }

  void AdvanceClock(Clock::duration delta) { clock_.Advance(delta); }

 private:
  FakeClock clock_;
  FakeTaskRunner task_runner_;
  testing::NiceMock<MockEnvironment> env_;
  ReceiverPacketRouter packet_router_;
  Receiver receiver_;
  testing::NiceMock<MockSender> sender_;
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

  // Move forward in time by 10 milliseconds. The FakeClock will have the
  // FakeTaskRunner execute all the pending tasks after that.
  constexpr auto kOneWayNetworkDelay = milliseconds(10);
  AdvanceClock(kOneWayNetworkDelay);

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
    AdvanceClock(kRtcpReportInterval);
    testing::Mock::VerifyAndClearExpectations(sender());
    EXPECT_LT(last_receiver_reference_time, receiver_reference_time);
    last_receiver_reference_time = receiver_reference_time;
  }
}

}  // namespace
}  // namespace cast_streaming
}  // namespace openscreen
