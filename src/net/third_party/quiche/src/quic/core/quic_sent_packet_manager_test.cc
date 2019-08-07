// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/core/quic_sent_packet_manager.h"

#include <memory>

#include "net/third_party/quiche/src/quic/core/quic_pending_retransmission.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_test.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_config_peer.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_sent_packet_manager_peer.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_test_utils.h"

using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::IsEmpty;
using testing::Not;
using testing::Pointwise;
using testing::Return;
using testing::StrictMock;
using testing::WithArgs;

namespace quic {
namespace test {
namespace {
// Default packet length.
const uint32_t kDefaultLength = 1000;

// Stream ID for data sent in CreatePacket().
const QuicStreamId kStreamId = 7;

// Matcher to check that the packet number matches the second argument.
MATCHER(PacketNumberEq, "") {
  return ::testing::get<0>(arg).packet_number ==
         QuicPacketNumber(::testing::get<1>(arg));
}

class MockDebugDelegate : public QuicSentPacketManager::DebugDelegate {
 public:
  MOCK_METHOD2(OnSpuriousPacketRetransmission,
               void(TransmissionType transmission_type,
                    QuicByteCount byte_size));
  MOCK_METHOD3(OnPacketLoss,
               void(QuicPacketNumber lost_packet_number,
                    TransmissionType transmission_type,
                    QuicTime detection_time));
};

class QuicSentPacketManagerTest : public QuicTestWithParam<bool> {
 public:
  void RetransmitCryptoPacket(uint64_t packet_number) {
    EXPECT_CALL(
        *send_algorithm_,
        OnPacketSent(_, BytesInFlight(), QuicPacketNumber(packet_number),
                     kDefaultLength, HAS_RETRANSMITTABLE_DATA));
    SerializedPacket packet(CreatePacket(packet_number, false));
    packet.retransmittable_frames.push_back(
        QuicFrame(QuicStreamFrame(1, false, 0, QuicStringPiece())));
    packet.has_crypto_handshake = IS_HANDSHAKE;
    manager_.OnPacketSent(&packet, QuicPacketNumber(), clock_.Now(),
                          HANDSHAKE_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  }

  void RetransmitDataPacket(uint64_t packet_number, TransmissionType type) {
    EXPECT_CALL(
        *send_algorithm_,
        OnPacketSent(_, BytesInFlight(), QuicPacketNumber(packet_number),
                     kDefaultLength, HAS_RETRANSMITTABLE_DATA));
    SerializedPacket packet(CreatePacket(packet_number, true));
    manager_.OnPacketSent(&packet, QuicPacketNumber(), clock_.Now(), type,
                          HAS_RETRANSMITTABLE_DATA);
  }

 protected:
  QuicSentPacketManagerTest()
      : manager_(Perspective::IS_SERVER,
                 &clock_,
                 QuicRandom::GetInstance(),
                 &stats_,
                 kCubicBytes,
                 kNack),
        send_algorithm_(new StrictMock<MockSendAlgorithm>),
        network_change_visitor_(new StrictMock<MockNetworkChangeVisitor>) {
    QuicSentPacketManagerPeer::SetSendAlgorithm(&manager_, send_algorithm_);
    // Disable tail loss probes for most tests.
    QuicSentPacketManagerPeer::SetMaxTailLossProbes(&manager_, 0);
    // Advance the time 1s so the send times are never QuicTime::Zero.
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1000));
    manager_.SetNetworkChangeVisitor(network_change_visitor_.get());
    manager_.SetSessionNotifier(&notifier_);
    manager_.SetSessionDecideWhatToWrite(GetParam());

    EXPECT_CALL(*send_algorithm_, HasReliableBandwidthEstimate())
        .Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, BandwidthEstimate())
        .Times(AnyNumber())
        .WillRepeatedly(Return(QuicBandwidth::Zero()));
    EXPECT_CALL(*send_algorithm_, InSlowStart()).Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, InRecovery()).Times(AnyNumber());
    EXPECT_CALL(*network_change_visitor_, OnPathMtuIncreased(1000))
        .Times(AnyNumber());
    EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(notifier_, HasUnackedCryptoData())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(notifier_, OnStreamFrameRetransmitted(_)).Times(AnyNumber());
    EXPECT_CALL(notifier_, OnFrameAcked(_, _)).WillRepeatedly(Return(true));
  }

  ~QuicSentPacketManagerTest() override {}

  QuicByteCount BytesInFlight() { return manager_.GetBytesInFlight(); }
  void VerifyUnackedPackets(uint64_t* packets, size_t num_packets) {
    if (num_packets == 0) {
      EXPECT_TRUE(manager_.unacked_packets().empty());
      EXPECT_EQ(0u, QuicSentPacketManagerPeer::GetNumRetransmittablePackets(
                        &manager_));
      return;
    }

    EXPECT_FALSE(manager_.unacked_packets().empty());
    EXPECT_EQ(QuicPacketNumber(packets[0]), manager_.GetLeastUnacked());
    for (size_t i = 0; i < num_packets; ++i) {
      EXPECT_TRUE(
          manager_.unacked_packets().IsUnacked(QuicPacketNumber(packets[i])))
          << packets[i];
    }
  }

  void VerifyRetransmittablePackets(uint64_t* packets, size_t num_packets) {
    EXPECT_EQ(
        num_packets,
        QuicSentPacketManagerPeer::GetNumRetransmittablePackets(&manager_));
    for (size_t i = 0; i < num_packets; ++i) {
      EXPECT_TRUE(QuicSentPacketManagerPeer::HasRetransmittableFrames(
          &manager_, packets[i]))
          << " packets[" << i << "]:" << packets[i];
    }
  }

  void ExpectAck(uint64_t largest_observed) {
    EXPECT_CALL(
        *send_algorithm_,
        // Ensure the AckedPacketVector argument contains largest_observed.
        OnCongestionEvent(true, _, _,
                          Pointwise(PacketNumberEq(), {largest_observed}),
                          IsEmpty()));
    EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  }

  void ExpectUpdatedRtt(uint64_t largest_observed) {
    EXPECT_CALL(*send_algorithm_,
                OnCongestionEvent(true, _, _, IsEmpty(), IsEmpty()));
    EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  }

  void ExpectAckAndLoss(bool rtt_updated,
                        uint64_t largest_observed,
                        uint64_t lost_packet) {
    EXPECT_CALL(
        *send_algorithm_,
        OnCongestionEvent(rtt_updated, _, _,
                          Pointwise(PacketNumberEq(), {largest_observed}),
                          Pointwise(PacketNumberEq(), {lost_packet})));
    EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  }

  // |packets_acked| and |packets_lost| should be in packet number order.
  void ExpectAcksAndLosses(bool rtt_updated,
                           uint64_t* packets_acked,
                           size_t num_packets_acked,
                           uint64_t* packets_lost,
                           size_t num_packets_lost) {
    std::vector<QuicPacketNumber> ack_vector;
    for (size_t i = 0; i < num_packets_acked; ++i) {
      ack_vector.push_back(QuicPacketNumber(packets_acked[i]));
    }
    std::vector<QuicPacketNumber> lost_vector;
    for (size_t i = 0; i < num_packets_lost; ++i) {
      lost_vector.push_back(QuicPacketNumber(packets_lost[i]));
    }
    EXPECT_CALL(*send_algorithm_,
                OnCongestionEvent(rtt_updated, _, _,
                                  Pointwise(PacketNumberEq(), ack_vector),
                                  Pointwise(PacketNumberEq(), lost_vector)));
    EXPECT_CALL(*network_change_visitor_, OnCongestionChange())
        .Times(AnyNumber());
  }

  void RetransmitAndSendPacket(uint64_t old_packet_number,
                               uint64_t new_packet_number) {
    RetransmitAndSendPacket(old_packet_number, new_packet_number,
                            TLP_RETRANSMISSION);
  }

  void RetransmitAndSendPacket(uint64_t old_packet_number,
                               uint64_t new_packet_number,
                               TransmissionType transmission_type) {
    bool is_lost = false;
    if (manager_.session_decides_what_to_write()) {
      if (transmission_type == HANDSHAKE_RETRANSMISSION ||
          transmission_type == TLP_RETRANSMISSION ||
          transmission_type == RTO_RETRANSMISSION ||
          transmission_type == PROBING_RETRANSMISSION) {
        EXPECT_CALL(notifier_, RetransmitFrames(_, _))
            .WillOnce(WithArgs<1>(
                Invoke([this, new_packet_number](TransmissionType type) {
                  RetransmitDataPacket(new_packet_number, type);
                })));
      } else {
        EXPECT_CALL(notifier_, OnFrameLost(_)).Times(1);
        is_lost = true;
      }
    }
    QuicSentPacketManagerPeer::MarkForRetransmission(
        &manager_, old_packet_number, transmission_type);
    if (manager_.session_decides_what_to_write()) {
      if (!is_lost) {
        return;
      }
      EXPECT_CALL(
          *send_algorithm_,
          OnPacketSent(_, BytesInFlight(), QuicPacketNumber(new_packet_number),
                       kDefaultLength, HAS_RETRANSMITTABLE_DATA));
      SerializedPacket packet(CreatePacket(new_packet_number, true));
      manager_.OnPacketSent(&packet, QuicPacketNumber(), clock_.Now(),
                            transmission_type, HAS_RETRANSMITTABLE_DATA);
      return;
    }
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
    QuicPendingRetransmission next_retransmission =
        manager_.NextPendingRetransmission();
    EXPECT_EQ(QuicPacketNumber(old_packet_number),
              next_retransmission.packet_number);
    EXPECT_EQ(transmission_type, next_retransmission.transmission_type);

    EXPECT_CALL(
        *send_algorithm_,
        OnPacketSent(_, BytesInFlight(), QuicPacketNumber(new_packet_number),
                     kDefaultLength, HAS_RETRANSMITTABLE_DATA));
    SerializedPacket packet(CreatePacket(new_packet_number, false));
    manager_.OnPacketSent(&packet, QuicPacketNumber(old_packet_number),
                          clock_.Now(), transmission_type,
                          HAS_RETRANSMITTABLE_DATA);
    EXPECT_TRUE(QuicSentPacketManagerPeer::IsRetransmission(&manager_,
                                                            new_packet_number));
  }

  SerializedPacket CreateDataPacket(uint64_t packet_number) {
    return CreatePacket(packet_number, true);
  }

  SerializedPacket CreatePacket(uint64_t packet_number, bool retransmittable) {
    SerializedPacket packet(QuicPacketNumber(packet_number),
                            PACKET_4BYTE_PACKET_NUMBER, nullptr, kDefaultLength,
                            false, false);
    if (retransmittable) {
      packet.retransmittable_frames.push_back(
          QuicFrame(QuicStreamFrame(kStreamId, false, 0, QuicStringPiece())));
    }
    return packet;
  }

  void SendDataPacket(uint64_t packet_number) {
    SendDataPacket(packet_number, ENCRYPTION_INITIAL);
  }

  void SendDataPacket(uint64_t packet_number,
                      EncryptionLevel encryption_level) {
    EXPECT_CALL(*send_algorithm_,
                OnPacketSent(_, BytesInFlight(),
                             QuicPacketNumber(packet_number), _, _));
    SerializedPacket packet(CreateDataPacket(packet_number));
    packet.encryption_level = encryption_level;
    manager_.OnPacketSent(&packet, QuicPacketNumber(), clock_.Now(),
                          NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
  }

  void SendCryptoPacket(uint64_t packet_number) {
    EXPECT_CALL(
        *send_algorithm_,
        OnPacketSent(_, BytesInFlight(), QuicPacketNumber(packet_number),
                     kDefaultLength, HAS_RETRANSMITTABLE_DATA));
    SerializedPacket packet(CreatePacket(packet_number, false));
    packet.retransmittable_frames.push_back(
        QuicFrame(QuicStreamFrame(1, false, 0, QuicStringPiece())));
    packet.has_crypto_handshake = IS_HANDSHAKE;
    manager_.OnPacketSent(&packet, QuicPacketNumber(), clock_.Now(),
                          NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
    if (manager_.session_decides_what_to_write()) {
      EXPECT_CALL(notifier_, HasUnackedCryptoData())
          .WillRepeatedly(Return(true));
    }
  }

  void SendAckPacket(uint64_t packet_number, uint64_t largest_acked) {
    EXPECT_CALL(
        *send_algorithm_,
        OnPacketSent(_, BytesInFlight(), QuicPacketNumber(packet_number),
                     kDefaultLength, NO_RETRANSMITTABLE_DATA));
    SerializedPacket packet(CreatePacket(packet_number, false));
    packet.largest_acked = QuicPacketNumber(largest_acked);
    manager_.OnPacketSent(&packet, QuicPacketNumber(), clock_.Now(),
                          NOT_RETRANSMISSION, NO_RETRANSMITTABLE_DATA);
  }

  // Based on QuicConnection's WritePendingRetransmissions.
  void RetransmitNextPacket(uint64_t retransmission_packet_number) {
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
    EXPECT_CALL(
        *send_algorithm_,
        OnPacketSent(_, _, QuicPacketNumber(retransmission_packet_number),
                     kDefaultLength, HAS_RETRANSMITTABLE_DATA));
    const QuicPendingRetransmission pending =
        manager_.NextPendingRetransmission();
    SerializedPacket packet(CreatePacket(retransmission_packet_number, false));
    manager_.OnPacketSent(&packet, pending.packet_number, clock_.Now(),
                          pending.transmission_type, HAS_RETRANSMITTABLE_DATA);
  }

  QuicSentPacketManager manager_;
  MockClock clock_;
  QuicConnectionStats stats_;
  MockSendAlgorithm* send_algorithm_;
  std::unique_ptr<MockNetworkChangeVisitor> network_change_visitor_;
  StrictMock<MockSessionNotifier> notifier_;
};

INSTANTIATE_TEST_SUITE_P(Tests, QuicSentPacketManagerTest, testing::Bool());

TEST_P(QuicSentPacketManagerTest, IsUnacked) {
  VerifyUnackedPackets(nullptr, 0);
  SendDataPacket(1);

  uint64_t unacked[] = {1};
  VerifyUnackedPackets(unacked, QUIC_ARRAYSIZE(unacked));
  uint64_t retransmittable[] = {1};
  VerifyRetransmittablePackets(retransmittable,
                               QUIC_ARRAYSIZE(retransmittable));
}

TEST_P(QuicSentPacketManagerTest, IsUnAckedRetransmit) {
  SendDataPacket(1);
  RetransmitAndSendPacket(1, 2);

  EXPECT_TRUE(QuicSentPacketManagerPeer::IsRetransmission(&manager_, 2));
  uint64_t unacked[] = {1, 2};
  VerifyUnackedPackets(unacked, QUIC_ARRAYSIZE(unacked));
  std::vector<uint64_t> retransmittable;
  if (manager_.session_decides_what_to_write()) {
    retransmittable = {1, 2};
  } else {
    retransmittable = {2};
  }
  VerifyRetransmittablePackets(&retransmittable[0], retransmittable.size());
}

TEST_P(QuicSentPacketManagerTest, RetransmitThenAck) {
  SendDataPacket(1);
  RetransmitAndSendPacket(1, 2);

  // Ack 2 but not 1.
  ExpectAck(2);
  manager_.OnAckFrameStart(QuicPacketNumber(2), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(3));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
  }
  // Packet 1 is unacked, pending, but not retransmittable.
  uint64_t unacked[] = {1};
  VerifyUnackedPackets(unacked, QUIC_ARRAYSIZE(unacked));
  EXPECT_TRUE(manager_.HasInFlightPackets());
  VerifyRetransmittablePackets(nullptr, 0);
}

TEST_P(QuicSentPacketManagerTest, RetransmitThenAckBeforeSend) {
  SendDataPacket(1);
  if (manager_.session_decides_what_to_write()) {
    if (manager_.session_decides_what_to_write()) {
      EXPECT_CALL(notifier_, RetransmitFrames(_, _))
          .WillOnce(WithArgs<1>(Invoke([this](TransmissionType type) {
            RetransmitDataPacket(2, type);
          })));
    }
  }
  QuicSentPacketManagerPeer::MarkForRetransmission(&manager_, 1,
                                                   TLP_RETRANSMISSION);
  if (!manager_.session_decides_what_to_write()) {
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
  }
  // Ack 1.
  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));

  // There should no longer be a pending retransmission.
  EXPECT_FALSE(manager_.HasPendingRetransmissions());

  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
    uint64_t unacked[] = {2};
    VerifyUnackedPackets(unacked, QUIC_ARRAYSIZE(unacked));
    // We do not know packet 2 is a spurious retransmission until it gets acked.
  } else {
    // No unacked packets remain.
    VerifyUnackedPackets(nullptr, 0);
  }
  VerifyRetransmittablePackets(nullptr, 0);
  EXPECT_EQ(0u, stats_.packets_spuriously_retransmitted);
}

TEST_P(QuicSentPacketManagerTest, RetransmitThenStopRetransmittingBeforeSend) {
  SendDataPacket(1);
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _));
  }
  QuicSentPacketManagerPeer::MarkForRetransmission(&manager_, 1,
                                                   TLP_RETRANSMISSION);
  if (!manager_.session_decides_what_to_write()) {
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
  }

  manager_.CancelRetransmissionsForStream(kStreamId);
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
  }

  // There should no longer be a pending retransmission.
  EXPECT_FALSE(manager_.HasPendingRetransmissions());

  uint64_t unacked[] = {1};
  VerifyUnackedPackets(unacked, QUIC_ARRAYSIZE(unacked));
  VerifyRetransmittablePackets(nullptr, 0);
  EXPECT_EQ(0u, stats_.packets_spuriously_retransmitted);
}

TEST_P(QuicSentPacketManagerTest, RetransmitThenAckPrevious) {
  SendDataPacket(1);
  RetransmitAndSendPacket(1, 2);
  QuicTime::Delta rtt = QuicTime::Delta::FromMilliseconds(15);
  clock_.AdvanceTime(rtt);

  // Ack 1 but not 2.
  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
  }
  // 2 remains unacked, but no packets have retransmittable data.
  uint64_t unacked[] = {2};
  VerifyUnackedPackets(unacked, QUIC_ARRAYSIZE(unacked));
  EXPECT_TRUE(manager_.HasInFlightPackets());
  VerifyRetransmittablePackets(nullptr, 0);
  if (manager_.session_decides_what_to_write()) {
    // Ack 2 causes 2 be considered as spurious retransmission.
    EXPECT_CALL(notifier_, OnFrameAcked(_, _)).WillOnce(Return(false));
    ExpectAck(2);
    manager_.OnAckFrameStart(QuicPacketNumber(2), QuicTime::Delta::Infinite(),
                             clock_.Now());
    manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(3));
    EXPECT_EQ(PACKETS_NEWLY_ACKED,
              manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
  }

  EXPECT_EQ(1u, stats_.packets_spuriously_retransmitted);
}

TEST_P(QuicSentPacketManagerTest, RetransmitThenAckPreviousThenNackRetransmit) {
  SendDataPacket(1);
  RetransmitAndSendPacket(1, 2);
  QuicTime::Delta rtt = QuicTime::Delta::FromMilliseconds(15);
  clock_.AdvanceTime(rtt);

  // First, ACK packet 1 which makes packet 2 non-retransmittable.
  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));

  SendDataPacket(3);
  SendDataPacket(4);
  SendDataPacket(5);
  clock_.AdvanceTime(rtt);

  // Next, NACK packet 2 three times.
  ExpectAck(3);
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(4));
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));

  ExpectAck(4);
  manager_.OnAckFrameStart(QuicPacketNumber(4), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(5));
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));

  ExpectAckAndLoss(true, 5, 2);
  if (manager_.session_decides_what_to_write()) {
    // Frames in all packets are acked.
    EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
    // Notify session that stream frame in packet 2 gets lost although it is
    // not outstanding.
    EXPECT_CALL(notifier_, OnFrameLost(_)).Times(1);
  }
  manager_.OnAckFrameStart(QuicPacketNumber(5), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(6));
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));

  if (manager_.session_decides_what_to_write()) {
    uint64_t unacked[] = {2};
    VerifyUnackedPackets(unacked, QUIC_ARRAYSIZE(unacked));
  } else {
    // No packets remain unacked.
    VerifyUnackedPackets(nullptr, 0);
  }
  EXPECT_FALSE(manager_.HasInFlightPackets());
  VerifyRetransmittablePackets(nullptr, 0);

  // Verify that the retransmission alarm would not fire,
  // since there is no retransmittable data outstanding.
  EXPECT_EQ(QuicTime::Zero(), manager_.GetRetransmissionTime());
}

TEST_P(QuicSentPacketManagerTest,
       DISABLED_RetransmitTwiceThenAckPreviousBeforeSend) {
  SendDataPacket(1);
  RetransmitAndSendPacket(1, 2);

  // Fire the RTO, which will mark 2 for retransmission (but will not send it).
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.OnRetransmissionTimeout();
  EXPECT_TRUE(manager_.HasPendingRetransmissions());

  // Ack 1 but not 2, before 2 is able to be sent.
  // Since 1 has been retransmitted, it has already been lost, and so the
  // send algorithm is not informed that it has been ACK'd.
  ExpectUpdatedRtt(1);
  EXPECT_CALL(*send_algorithm_, RevertRetransmissionTimeout());
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));

  // Since 2 was marked for retransmit, when 1 is acked, 2 is kept for RTT.
  uint64_t unacked[] = {2};
  VerifyUnackedPackets(unacked, QUIC_ARRAYSIZE(unacked));
  EXPECT_FALSE(manager_.HasInFlightPackets());
  VerifyRetransmittablePackets(nullptr, 0);

  // Verify that the retransmission alarm would not fire,
  // since there is no retransmittable data outstanding.
  EXPECT_EQ(QuicTime::Zero(), manager_.GetRetransmissionTime());
}

TEST_P(QuicSentPacketManagerTest, RetransmitTwiceThenAckFirst) {
  StrictMock<MockDebugDelegate> debug_delegate;
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(debug_delegate, OnSpuriousPacketRetransmission(
                                    TLP_RETRANSMISSION, kDefaultLength))
        .Times(1);
  } else {
    EXPECT_CALL(debug_delegate, OnSpuriousPacketRetransmission(
                                    TLP_RETRANSMISSION, kDefaultLength))
        .Times(2);
  }
  manager_.SetDebugDelegate(&debug_delegate);

  SendDataPacket(1);
  RetransmitAndSendPacket(1, 2);
  RetransmitAndSendPacket(2, 3);
  QuicTime::Delta rtt = QuicTime::Delta::FromMilliseconds(15);
  clock_.AdvanceTime(rtt);

  // Ack 1 but not 2 or 3.
  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
  if (manager_.session_decides_what_to_write()) {
    // Frames in packets 2 and 3 are acked.
    EXPECT_CALL(notifier_, IsFrameOutstanding(_))
        .Times(2)
        .WillRepeatedly(Return(false));
  }

  // 2 and 3 remain unacked, but no packets have retransmittable data.
  uint64_t unacked[] = {2, 3};
  VerifyUnackedPackets(unacked, QUIC_ARRAYSIZE(unacked));
  EXPECT_TRUE(manager_.HasInFlightPackets());
  VerifyRetransmittablePackets(nullptr, 0);

  // Ensure packet 2 is lost when 4 is sent and 3 and 4 are acked.
  SendDataPacket(4);
  if (manager_.session_decides_what_to_write()) {
    // No new data gets acked in packet 3.
    EXPECT_CALL(notifier_, OnFrameAcked(_, _))
        .WillOnce(Return(false))
        .WillRepeatedly(Return(true));
  }
  uint64_t acked[] = {3, 4};
  ExpectAcksAndLosses(true, acked, QUIC_ARRAYSIZE(acked), nullptr, 0);
  manager_.OnAckFrameStart(QuicPacketNumber(4), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(5));
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));

  uint64_t unacked2[] = {2};
  VerifyUnackedPackets(unacked2, QUIC_ARRAYSIZE(unacked2));
  EXPECT_TRUE(manager_.HasInFlightPackets());

  SendDataPacket(5);
  ExpectAckAndLoss(true, 5, 2);
  EXPECT_CALL(debug_delegate,
              OnPacketLoss(QuicPacketNumber(2), LOSS_RETRANSMISSION, _));
  if (manager_.session_decides_what_to_write()) {
    // Frames in all packets are acked.
    EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
    // Notify session that stream frame in packet 2 gets lost although it is
    // not outstanding.
    EXPECT_CALL(notifier_, OnFrameLost(_)).Times(1);
  }
  manager_.OnAckFrameStart(QuicPacketNumber(5), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(6));
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));

  if (manager_.session_decides_what_to_write()) {
    uint64_t unacked[] = {2};
    VerifyUnackedPackets(unacked, QUIC_ARRAYSIZE(unacked));
  } else {
    VerifyUnackedPackets(nullptr, 0);
  }
  EXPECT_FALSE(manager_.HasInFlightPackets());
  if (manager_.session_decides_what_to_write()) {
    // Spurious retransmission is detected when packet 3 gets acked. We cannot
    // know packet 2 is a spurious until it gets acked.
    EXPECT_EQ(1u, stats_.packets_spuriously_retransmitted);
  } else {
    EXPECT_EQ(2u, stats_.packets_spuriously_retransmitted);
  }
}

TEST_P(QuicSentPacketManagerTest, AckOriginalTransmission) {
  auto loss_algorithm = QuicMakeUnique<MockLossAlgorithm>();
  QuicSentPacketManagerPeer::SetLossAlgorithm(&manager_, loss_algorithm.get());

  SendDataPacket(1);
  RetransmitAndSendPacket(1, 2);

  // Ack original transmission, but that wasn't lost via fast retransmit,
  // so no call on OnSpuriousRetransmission is expected.
  {
    ExpectAck(1);
    EXPECT_CALL(*loss_algorithm, DetectLosses(_, _, _, _, _, _));
    manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                             clock_.Now());
    manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
    EXPECT_EQ(PACKETS_NEWLY_ACKED,
              manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
  }

  SendDataPacket(3);
  SendDataPacket(4);
  // Ack 4, which causes 3 to be retransmitted.
  {
    ExpectAck(4);
    EXPECT_CALL(*loss_algorithm, DetectLosses(_, _, _, _, _, _));
    manager_.OnAckFrameStart(QuicPacketNumber(4), QuicTime::Delta::Infinite(),
                             clock_.Now());
    manager_.OnAckRange(QuicPacketNumber(4), QuicPacketNumber(5));
    manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
    EXPECT_EQ(PACKETS_NEWLY_ACKED,
              manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
    RetransmitAndSendPacket(3, 5, LOSS_RETRANSMISSION);
  }

  // Ack 3, which causes SpuriousRetransmitDetected to be called.
  {
    uint64_t acked[] = {3};
    ExpectAcksAndLosses(false, acked, QUIC_ARRAYSIZE(acked), nullptr, 0);
    EXPECT_CALL(*loss_algorithm, DetectLosses(_, _, _, _, _, _));
    EXPECT_CALL(*loss_algorithm,
                SpuriousRetransmitDetected(_, _, _, QuicPacketNumber(5)));
    manager_.OnAckFrameStart(QuicPacketNumber(4), QuicTime::Delta::Infinite(),
                             clock_.Now());
    manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(5));
    manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
    EXPECT_EQ(PACKETS_NEWLY_ACKED,
              manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
    if (manager_.session_decides_what_to_write()) {
      // Ack 3 will not cause 5 be considered as a spurious retransmission. Ack
      // 5 will cause 5 be considered as a spurious retransmission as no new
      // data gets acked.
      ExpectAck(5);
      EXPECT_CALL(*loss_algorithm, DetectLosses(_, _, _, _, _, _));
      EXPECT_CALL(notifier_, OnFrameAcked(_, _)).WillOnce(Return(false));
      manager_.OnAckFrameStart(QuicPacketNumber(5), QuicTime::Delta::Infinite(),
                               clock_.Now());
      manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(6));
      manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
      EXPECT_EQ(PACKETS_NEWLY_ACKED,
                manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
    }
  }
}

TEST_P(QuicSentPacketManagerTest, GetLeastUnacked) {
  EXPECT_EQ(QuicPacketNumber(1u), manager_.GetLeastUnacked());
}

TEST_P(QuicSentPacketManagerTest, GetLeastUnackedUnacked) {
  SendDataPacket(1);
  EXPECT_EQ(QuicPacketNumber(1u), manager_.GetLeastUnacked());
}

TEST_P(QuicSentPacketManagerTest, AckAckAndUpdateRtt) {
  EXPECT_FALSE(manager_.largest_packet_peer_knows_is_acked().IsInitialized());
  SendDataPacket(1);
  SendAckPacket(2, 1);

  // Now ack the ack and expect an RTT update.
  uint64_t acked[] = {1, 2};
  ExpectAcksAndLosses(true, acked, QUIC_ARRAYSIZE(acked), nullptr, 0);
  manager_.OnAckFrameStart(QuicPacketNumber(2),
                           QuicTime::Delta::FromMilliseconds(5), clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(3));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
  EXPECT_EQ(QuicPacketNumber(1), manager_.largest_packet_peer_knows_is_acked());

  SendAckPacket(3, 3);

  // Now ack the ack and expect only an RTT update.
  uint64_t acked2[] = {3};
  ExpectAcksAndLosses(true, acked2, QUIC_ARRAYSIZE(acked2), nullptr, 0);
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(4));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
  EXPECT_EQ(QuicPacketNumber(3u),
            manager_.largest_packet_peer_knows_is_acked());
}

TEST_P(QuicSentPacketManagerTest, Rtt) {
  QuicTime::Delta expected_rtt = QuicTime::Delta::FromMilliseconds(20);
  SendDataPacket(1);
  clock_.AdvanceTime(expected_rtt);

  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
  EXPECT_EQ(expected_rtt, manager_.GetRttStats()->latest_rtt());
}

TEST_P(QuicSentPacketManagerTest, RttWithInvalidDelta) {
  // Expect that the RTT is equal to the local time elapsed, since the
  // ack_delay_time is larger than the local time elapsed
  // and is hence invalid.
  QuicTime::Delta expected_rtt = QuicTime::Delta::FromMilliseconds(10);
  SendDataPacket(1);
  clock_.AdvanceTime(expected_rtt);

  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1),
                           QuicTime::Delta::FromMilliseconds(11), clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
  EXPECT_EQ(expected_rtt, manager_.GetRttStats()->latest_rtt());
}

TEST_P(QuicSentPacketManagerTest, RttWithInfiniteDelta) {
  // Expect that the RTT is equal to the local time elapsed, since the
  // ack_delay_time is infinite, and is hence invalid.
  QuicTime::Delta expected_rtt = QuicTime::Delta::FromMilliseconds(10);
  SendDataPacket(1);
  clock_.AdvanceTime(expected_rtt);

  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
  EXPECT_EQ(expected_rtt, manager_.GetRttStats()->latest_rtt());
}

TEST_P(QuicSentPacketManagerTest, RttZeroDelta) {
  // Expect that the RTT is the time between send and receive since the
  // ack_delay_time is zero.
  QuicTime::Delta expected_rtt = QuicTime::Delta::FromMilliseconds(10);
  SendDataPacket(1);
  clock_.AdvanceTime(expected_rtt);

  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Zero(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
  EXPECT_EQ(expected_rtt, manager_.GetRttStats()->latest_rtt());
}

TEST_P(QuicSentPacketManagerTest, TailLossProbeTimeout) {
  QuicSentPacketManagerPeer::SetMaxTailLossProbes(&manager_, 2);

  // Send 1 packet.
  SendDataPacket(1);

  // The first tail loss probe retransmits 1 packet.
  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(QuicTime::Delta::Zero(), manager_.TimeUntilSend(clock_.Now()));
  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .WillOnce(WithArgs<1>(Invoke(
            [this](TransmissionType type) { RetransmitDataPacket(2, type); })));
  }
  manager_.MaybeRetransmitTailLossProbe();
  if (!manager_.session_decides_what_to_write()) {
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
    RetransmitNextPacket(2);
    EXPECT_FALSE(manager_.HasPendingRetransmissions());
  }

  // The second tail loss probe retransmits 1 packet.
  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(QuicTime::Delta::Zero(), manager_.TimeUntilSend(clock_.Now()));
  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .WillOnce(WithArgs<1>(Invoke(
            [this](TransmissionType type) { RetransmitDataPacket(3, type); })));
  }
  manager_.MaybeRetransmitTailLossProbe();
  if (!manager_.session_decides_what_to_write()) {
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
    RetransmitNextPacket(3);
  }
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillOnce(Return(false));
  EXPECT_EQ(QuicTime::Delta::Infinite(), manager_.TimeUntilSend(clock_.Now()));
  EXPECT_FALSE(manager_.HasPendingRetransmissions());

  // Ack the third and ensure the first two are still pending.
  ExpectAck(3);

  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(4));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));

  EXPECT_TRUE(manager_.HasInFlightPackets());

  // Acking two more packets will lose both of them due to nacks.
  SendDataPacket(4);
  SendDataPacket(5);
  uint64_t acked[] = {4, 5};
  uint64_t lost[] = {1, 2};
  ExpectAcksAndLosses(true, acked, QUIC_ARRAYSIZE(acked), lost,
                      QUIC_ARRAYSIZE(lost));
  if (manager_.session_decides_what_to_write()) {
    // Frames in all packets are acked.
    EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
    // Notify session that stream frame in packets 1 and 2 get lost although
    // they are not outstanding.
    EXPECT_CALL(notifier_, OnFrameLost(_)).Times(2);
  }
  manager_.OnAckFrameStart(QuicPacketNumber(5), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(6));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));

  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  EXPECT_FALSE(manager_.HasInFlightPackets());
  EXPECT_EQ(2u, stats_.tlp_count);
  EXPECT_EQ(0u, stats_.rto_count);
}

TEST_P(QuicSentPacketManagerTest, TailLossProbeThenRTO) {
  QuicSentPacketManagerPeer::SetMaxTailLossProbes(&manager_, 2);

  // Send 100 packets.
  const size_t kNumSentPackets = 100;
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
  }
  QuicTime rto_packet_time = clock_.Now();
  // Advance the time.
  clock_.AdvanceTime(manager_.GetRetransmissionTime() - clock_.Now());

  // The first tail loss probe retransmits 1 packet.
  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(QuicTime::Delta::Zero(), manager_.TimeUntilSend(clock_.Now()));
  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .WillOnce(WithArgs<1>(Invoke([this](TransmissionType type) {
          RetransmitDataPacket(101, type);
        })));
  }
  manager_.MaybeRetransmitTailLossProbe();
  if (!manager_.session_decides_what_to_write()) {
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
    RetransmitNextPacket(101);
  }
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillOnce(Return(false));
  EXPECT_EQ(QuicTime::Delta::Infinite(), manager_.TimeUntilSend(clock_.Now()));
  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  clock_.AdvanceTime(manager_.GetRetransmissionTime() - clock_.Now());

  // The second tail loss probe retransmits 1 packet.
  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(QuicTime::Delta::Zero(), manager_.TimeUntilSend(clock_.Now()));
  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .WillOnce(WithArgs<1>(Invoke([this](TransmissionType type) {
          RetransmitDataPacket(102, type);
        })));
  }
  EXPECT_TRUE(manager_.MaybeRetransmitTailLossProbe());
  if (!manager_.session_decides_what_to_write()) {
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
    RetransmitNextPacket(102);
  }
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillOnce(Return(false));
  EXPECT_EQ(QuicTime::Delta::Infinite(), manager_.TimeUntilSend(clock_.Now()));

  // Ensure the RTO is set based on the correct packet.
  rto_packet_time = clock_.Now();
  EXPECT_EQ(rto_packet_time + QuicTime::Delta::FromMilliseconds(500),
            manager_.GetRetransmissionTime());

  // Advance the time enough to ensure all packets are RTO'd.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1000));

  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .Times(2)
        .WillOnce(WithArgs<1>(Invoke([this](TransmissionType type) {
          RetransmitDataPacket(103, type);
        })))
        .WillOnce(WithArgs<1>(Invoke([this](TransmissionType type) {
          RetransmitDataPacket(104, type);
        })));
  }
  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(2u, stats_.tlp_count);
  EXPECT_EQ(1u, stats_.rto_count);
  if (manager_.session_decides_what_to_write()) {
    // There are 2 RTO retransmissions.
    EXPECT_EQ(104 * kDefaultLength, manager_.GetBytesInFlight());
  }
  if (!manager_.session_decides_what_to_write()) {
    // Send and Ack the RTO and ensure OnRetransmissionTimeout is called.
    EXPECT_EQ(102 * kDefaultLength, manager_.GetBytesInFlight());
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
    RetransmitNextPacket(103);
  }
  QuicPacketNumber largest_acked = QuicPacketNumber(103);
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  EXPECT_CALL(*send_algorithm_,
              OnCongestionEvent(
                  true, _, _, Pointwise(PacketNumberEq(), {largest_acked}), _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  if (manager_.session_decides_what_to_write()) {
    // Although frames in packet 3 gets acked, it would be kept for another
    // RTT.
    EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(true));
    // Packets [1, 102] are lost, although stream frame in packet 3 is not
    // outstanding.
    EXPECT_CALL(notifier_, OnFrameLost(_)).Times(102);
  }
  manager_.OnAckFrameStart(QuicPacketNumber(103), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(103), QuicPacketNumber(104));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
  // All packets before 103 should be lost.
  if (manager_.session_decides_what_to_write()) {
    // Packet 104 is still in flight.
    EXPECT_EQ(1000u, manager_.GetBytesInFlight());
  } else {
    EXPECT_EQ(0u, manager_.GetBytesInFlight());
  }
}

TEST_P(QuicSentPacketManagerTest, CryptoHandshakeTimeout) {
  // Send 2 crypto packets and 3 data packets.
  const size_t kNumSentCryptoPackets = 2;
  for (size_t i = 1; i <= kNumSentCryptoPackets; ++i) {
    SendCryptoPacket(i);
  }
  const size_t kNumSentDataPackets = 3;
  for (size_t i = 1; i <= kNumSentDataPackets; ++i) {
    SendDataPacket(kNumSentCryptoPackets + i);
  }
  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());
  EXPECT_EQ(5 * kDefaultLength, manager_.GetBytesInFlight());

  // The first retransmits 2 packets.
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .Times(2)
        .WillOnce(InvokeWithoutArgs([this]() { RetransmitCryptoPacket(6); }))
        .WillOnce(InvokeWithoutArgs([this]() { RetransmitCryptoPacket(7); }));
  }
  manager_.OnRetransmissionTimeout();
  if (!manager_.session_decides_what_to_write()) {
    EXPECT_EQ(QuicTime::Delta::Zero(), manager_.TimeUntilSend(clock_.Now()));
    RetransmitNextPacket(6);
    RetransmitNextPacket(7);
    EXPECT_FALSE(manager_.HasPendingRetransmissions());
  }
  // Expect all 4 handshake packets to be in flight and 3 data packets.
  if (GetQuicReloadableFlag(quic_loss_removes_from_inflight)) {
    EXPECT_EQ(7 * kDefaultLength, manager_.GetBytesInFlight());
  }
  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());

  // The second retransmits 2 packets.
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .Times(2)
        .WillOnce(InvokeWithoutArgs([this]() { RetransmitCryptoPacket(8); }))
        .WillOnce(InvokeWithoutArgs([this]() { RetransmitCryptoPacket(9); }));
  }
  manager_.OnRetransmissionTimeout();
  if (!manager_.session_decides_what_to_write()) {
    EXPECT_EQ(QuicTime::Delta::Zero(), manager_.TimeUntilSend(clock_.Now()));
    RetransmitNextPacket(8);
    RetransmitNextPacket(9);
    EXPECT_FALSE(manager_.HasPendingRetransmissions());
  }
  if (GetQuicReloadableFlag(quic_loss_removes_from_inflight)) {
    EXPECT_EQ(9 * kDefaultLength, manager_.GetBytesInFlight());
  }
  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());

  // Now ack the two crypto packets and the speculatively encrypted request,
  // and ensure the first four crypto packets get abandoned, but not lost.
  if (GetQuicReloadableFlag(quic_loss_removes_from_inflight)) {
    // Crypto packets remain in flight, so any that aren't acked will be lost.
    uint64_t acked[] = {3, 4, 5, 8, 9};
    uint64_t lost[] = {1, 2, 6};
    ExpectAcksAndLosses(true, acked, QUIC_ARRAYSIZE(acked), lost,
                        QUIC_ARRAYSIZE(lost));
    if (manager_.session_decides_what_to_write()) {
      EXPECT_CALL(notifier_, OnFrameLost(_)).Times(3);
    }
  } else {
    uint64_t acked[] = {3, 4, 5, 8, 9};
    ExpectAcksAndLosses(true, acked, QUIC_ARRAYSIZE(acked), nullptr, 0);
  }
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, HasUnackedCryptoData())
        .WillRepeatedly(Return(false));
  }
  manager_.OnAckFrameStart(QuicPacketNumber(9), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(8), QuicPacketNumber(10));
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(6));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));

  EXPECT_FALSE(manager_.HasUnackedCryptoPackets());
}

TEST_P(QuicSentPacketManagerTest, CryptoHandshakeTimeoutVersionNegotiation) {
  // Send 2 crypto packets and 3 data packets.
  const size_t kNumSentCryptoPackets = 2;
  for (size_t i = 1; i <= kNumSentCryptoPackets; ++i) {
    SendCryptoPacket(i);
  }
  const size_t kNumSentDataPackets = 3;
  for (size_t i = 1; i <= kNumSentDataPackets; ++i) {
    SendDataPacket(kNumSentCryptoPackets + i);
  }
  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());

  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .Times(2)
        .WillOnce(InvokeWithoutArgs([this]() { RetransmitCryptoPacket(6); }))
        .WillOnce(InvokeWithoutArgs([this]() { RetransmitCryptoPacket(7); }));
  }
  manager_.OnRetransmissionTimeout();
  if (!manager_.session_decides_what_to_write()) {
    RetransmitNextPacket(6);
    RetransmitNextPacket(7);
    EXPECT_FALSE(manager_.HasPendingRetransmissions());
  }
  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());

  // Now act like a version negotiation packet arrived, which would cause all
  // unacked packets to be retransmitted.
  if (manager_.session_decides_what_to_write()) {
    // Mark packets [1, 7] lost. And the frames in 6 and 7 are same as packets 1
    // and 2, respectively.
    EXPECT_CALL(notifier_, OnFrameLost(_)).Times(7);
  }
  manager_.RetransmitUnackedPackets(ALL_UNACKED_RETRANSMISSION);

  // Ensure the first two pending packets are the crypto retransmits.
  if (manager_.session_decides_what_to_write()) {
    RetransmitCryptoPacket(8);
    RetransmitCryptoPacket(9);
    RetransmitDataPacket(10, ALL_UNACKED_RETRANSMISSION);
    RetransmitDataPacket(11, ALL_UNACKED_RETRANSMISSION);
    RetransmitDataPacket(12, ALL_UNACKED_RETRANSMISSION);
  } else {
    ASSERT_TRUE(manager_.HasPendingRetransmissions());
    EXPECT_EQ(QuicPacketNumber(6u),
              manager_.NextPendingRetransmission().packet_number);
    RetransmitNextPacket(8);
    EXPECT_EQ(QuicPacketNumber(7u),
              manager_.NextPendingRetransmission().packet_number);
    RetransmitNextPacket(9);
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
    // Send 3 more data packets and ensure the least unacked is raised.
    RetransmitNextPacket(10);
    RetransmitNextPacket(11);
    RetransmitNextPacket(12);
    EXPECT_FALSE(manager_.HasPendingRetransmissions());
  }

  EXPECT_EQ(QuicPacketNumber(1u), manager_.GetLeastUnacked());
  // Least unacked isn't raised until an ack is received, so ack the
  // crypto packets.
  uint64_t acked[] = {8, 9};
  ExpectAcksAndLosses(true, acked, QUIC_ARRAYSIZE(acked), nullptr, 0);
  manager_.OnAckFrameStart(QuicPacketNumber(9), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(8), QuicPacketNumber(10));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, HasUnackedCryptoData())
        .WillRepeatedly(Return(false));
  }
  EXPECT_EQ(QuicPacketNumber(10u), manager_.GetLeastUnacked());
}

TEST_P(QuicSentPacketManagerTest, CryptoHandshakeSpuriousRetransmission) {
  // Send 1 crypto packet.
  SendCryptoPacket(1);
  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());

  // Retransmit the crypto packet as 2.
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .WillOnce(InvokeWithoutArgs([this]() { RetransmitCryptoPacket(2); }));
  }
  manager_.OnRetransmissionTimeout();
  if (!manager_.session_decides_what_to_write()) {
    RetransmitNextPacket(2);
  }

  // Retransmit the crypto packet as 3.
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .WillOnce(InvokeWithoutArgs([this]() { RetransmitCryptoPacket(3); }));
  }
  manager_.OnRetransmissionTimeout();
  if (!manager_.session_decides_what_to_write()) {
    RetransmitNextPacket(3);
  }

  // Now ack the second crypto packet, and ensure the first gets removed, but
  // the third does not.
  uint64_t acked[] = {2};
  ExpectAcksAndLosses(true, acked, QUIC_ARRAYSIZE(acked), nullptr, 0);
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, HasUnackedCryptoData())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
  }
  manager_.OnAckFrameStart(QuicPacketNumber(2), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(3));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));

  EXPECT_FALSE(manager_.HasUnackedCryptoPackets());
  if (GetQuicReloadableFlag(quic_loss_removes_from_inflight)) {
    uint64_t unacked[] = {1, 3};
    VerifyUnackedPackets(unacked, QUIC_ARRAYSIZE(unacked));
  } else {
    uint64_t unacked[] = {3};
    VerifyUnackedPackets(unacked, QUIC_ARRAYSIZE(unacked));
  }
}

TEST_P(QuicSentPacketManagerTest, CryptoHandshakeTimeoutUnsentDataPacket) {
  // Send 2 crypto packets and 1 data packet.
  const size_t kNumSentCryptoPackets = 2;
  for (size_t i = 1; i <= kNumSentCryptoPackets; ++i) {
    SendCryptoPacket(i);
  }
  SendDataPacket(3);
  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());

  // Retransmit 2 crypto packets, but not the serialized packet.
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .Times(2)
        .WillOnce(InvokeWithoutArgs([this]() { RetransmitCryptoPacket(4); }))
        .WillOnce(InvokeWithoutArgs([this]() { RetransmitCryptoPacket(5); }));
  }
  manager_.OnRetransmissionTimeout();
  if (!manager_.session_decides_what_to_write()) {
    RetransmitNextPacket(4);
    RetransmitNextPacket(5);
    EXPECT_FALSE(manager_.HasPendingRetransmissions());
  }
  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());
}

TEST_P(QuicSentPacketManagerTest,
       CryptoHandshakeRetransmissionThenRetransmitAll) {
  // Send 1 crypto packet.
  SendCryptoPacket(1);

  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());

  // Retransmit the crypto packet as 2.
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .WillOnce(InvokeWithoutArgs([this]() { RetransmitCryptoPacket(2); }));
  }
  manager_.OnRetransmissionTimeout();
  if (!manager_.session_decides_what_to_write()) {
    RetransmitNextPacket(2);
  }
  // Now retransmit all the unacked packets, which occurs when there is a
  // version negotiation.
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, OnFrameLost(_)).Times(2);
  }
  manager_.RetransmitUnackedPackets(ALL_UNACKED_RETRANSMISSION);
  if (manager_.session_decides_what_to_write()) {
    // Both packets 1 and 2 are unackable.
    EXPECT_FALSE(manager_.unacked_packets().IsUnacked(QuicPacketNumber(1)));
    EXPECT_FALSE(manager_.unacked_packets().IsUnacked(QuicPacketNumber(2)));
  } else {
    // Packet 2 is useful because it does not get retransmitted and still has
    // retransmittable frames.
    uint64_t unacked[] = {1, 2};
    VerifyUnackedPackets(unacked, QUIC_ARRAYSIZE(unacked));
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
  }
  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());
  EXPECT_FALSE(manager_.HasInFlightPackets());
}

TEST_P(QuicSentPacketManagerTest,
       CryptoHandshakeRetransmissionThenNeuterAndAck) {
  // Send 1 crypto packet.
  SendCryptoPacket(1);

  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());

  // Retransmit the crypto packet as 2.
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .WillOnce(InvokeWithoutArgs([this]() { RetransmitCryptoPacket(2); }));
  }
  manager_.OnRetransmissionTimeout();
  if (!manager_.session_decides_what_to_write()) {
    RetransmitNextPacket(2);
  }
  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());

  // Retransmit the crypto packet as 3.
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .WillOnce(InvokeWithoutArgs([this]() { RetransmitCryptoPacket(3); }));
  }
  manager_.OnRetransmissionTimeout();
  if (!manager_.session_decides_what_to_write()) {
    RetransmitNextPacket(3);
  }
  EXPECT_TRUE(manager_.HasUnackedCryptoPackets());

  // Now neuter all unacked unencrypted packets, which occurs when the
  // connection goes forward secure.
  manager_.NeuterUnencryptedPackets();
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, HasUnackedCryptoData())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(false));
  }
  EXPECT_FALSE(manager_.HasUnackedCryptoPackets());
  uint64_t unacked[] = {1, 2, 3};
  VerifyUnackedPackets(unacked, QUIC_ARRAYSIZE(unacked));
  VerifyRetransmittablePackets(nullptr, 0);
  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  EXPECT_FALSE(manager_.HasUnackedCryptoPackets());
  EXPECT_FALSE(manager_.HasInFlightPackets());

  // Ensure both packets get discarded when packet 2 is acked.
  uint64_t acked[] = {3};
  ExpectAcksAndLosses(true, acked, QUIC_ARRAYSIZE(acked), nullptr, 0);
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(4));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
  VerifyUnackedPackets(nullptr, 0);
  VerifyRetransmittablePackets(nullptr, 0);
}

TEST_P(QuicSentPacketManagerTest, RetransmissionTimeout) {
  StrictMock<MockDebugDelegate> debug_delegate;
  manager_.SetDebugDelegate(&debug_delegate);

  // Send 100 packets.
  const size_t kNumSentPackets = 100;
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
  }

  EXPECT_FALSE(manager_.MaybeRetransmitTailLossProbe());
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .Times(2)
        .WillOnce(WithArgs<1>(Invoke([this](TransmissionType type) {
          RetransmitDataPacket(101, type);
        })))
        .WillOnce(WithArgs<1>(Invoke([this](TransmissionType type) {
          RetransmitDataPacket(102, type);
        })));
  }
  manager_.OnRetransmissionTimeout();
  if (manager_.session_decides_what_to_write()) {
    EXPECT_EQ(102 * kDefaultLength, manager_.GetBytesInFlight());
  } else {
    ASSERT_TRUE(manager_.HasPendingRetransmissions());
    EXPECT_EQ(100 * kDefaultLength, manager_.GetBytesInFlight());
    RetransmitNextPacket(101);
    ASSERT_TRUE(manager_.HasPendingRetransmissions());
    RetransmitNextPacket(102);
    EXPECT_FALSE(manager_.HasPendingRetransmissions());
  }

  // Ack a retransmission.
  // Ensure no packets are lost.
  QuicPacketNumber largest_acked = QuicPacketNumber(102);
  EXPECT_CALL(*send_algorithm_,
              OnCongestionEvent(true, _, _,
                                Pointwise(PacketNumberEq(), {largest_acked}),
                                /*lost_packets=*/IsEmpty()));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  // RTO's use loss detection instead of immediately declaring retransmitted
  // packets lost.
  for (int i = 1; i <= 99; ++i) {
    EXPECT_CALL(debug_delegate,
                OnPacketLoss(QuicPacketNumber(i), LOSS_RETRANSMISSION, _));
  }
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(true));
    // Packets [1, 99] are considered as lost, although stream frame in packet
    // 2 is not outstanding.
    EXPECT_CALL(notifier_, OnFrameLost(_)).Times(99);
  }
  manager_.OnAckFrameStart(QuicPacketNumber(102), QuicTime::Delta::Zero(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(102), QuicPacketNumber(103));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
}

TEST_P(QuicSentPacketManagerTest, RetransmissionTimeoutOnePacket) {
  // Set the 1RTO connection option.
  QuicConfig client_config;
  QuicTagVector options;
  options.push_back(k1RTO);
  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);
  client_config.SetConnectionOptionsToSend(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, PacingRate(_))
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(10 * kDefaultTCPMSS));
  manager_.SetFromConfig(client_config);
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));

  StrictMock<MockDebugDelegate> debug_delegate;
  manager_.SetDebugDelegate(&debug_delegate);

  // Send 100 packets.
  const size_t kNumSentPackets = 100;
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
  }

  EXPECT_FALSE(manager_.MaybeRetransmitTailLossProbe());
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .Times(1)
        .WillOnce(WithArgs<1>(Invoke([this](TransmissionType type) {
          RetransmitDataPacket(101, type);
        })));
  }
  manager_.OnRetransmissionTimeout();
  if (manager_.session_decides_what_to_write()) {
    EXPECT_EQ(101 * kDefaultLength, manager_.GetBytesInFlight());
  } else {
    ASSERT_TRUE(manager_.HasPendingRetransmissions());
    EXPECT_EQ(100 * kDefaultLength, manager_.GetBytesInFlight());
    RetransmitNextPacket(101);
    EXPECT_FALSE(manager_.HasPendingRetransmissions());
  }
}

TEST_P(QuicSentPacketManagerTest, NewRetransmissionTimeout) {
  QuicConfig client_config;
  QuicTagVector options;
  options.push_back(kNRTO);
  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);
  client_config.SetConnectionOptionsToSend(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, PacingRate(_))
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(10 * kDefaultTCPMSS));
  manager_.SetFromConfig(client_config);
  EXPECT_TRUE(QuicSentPacketManagerPeer::GetUseNewRto(&manager_));
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillRepeatedly(Return(true));

  // Send 100 packets.
  const size_t kNumSentPackets = 100;
  for (size_t i = 1; i <= kNumSentPackets; ++i) {
    SendDataPacket(i);
  }

  EXPECT_FALSE(manager_.MaybeRetransmitTailLossProbe());
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .Times(2)
        .WillOnce(WithArgs<1>(Invoke([this](TransmissionType type) {
          RetransmitDataPacket(101, type);
        })))
        .WillOnce(WithArgs<1>(Invoke([this](TransmissionType type) {
          RetransmitDataPacket(102, type);
        })));
  }
  manager_.OnRetransmissionTimeout();
  if (manager_.session_decides_what_to_write()) {
    EXPECT_EQ(102 * kDefaultLength, manager_.GetBytesInFlight());
  } else {
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
    EXPECT_EQ(100 * kDefaultLength, manager_.GetBytesInFlight());
    RetransmitNextPacket(101);
    RetransmitNextPacket(102);
    EXPECT_FALSE(manager_.HasPendingRetransmissions());
  }

  // Ack a retransmission and expect no call to OnRetransmissionTimeout.
  // This will include packets in the lost packet map.
  QuicPacketNumber largest_acked = QuicPacketNumber(102);
  EXPECT_CALL(*send_algorithm_,
              OnCongestionEvent(true, _, _,
                                Pointwise(PacketNumberEq(), {largest_acked}),
                                /*lost_packets=*/Not(IsEmpty())));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, IsFrameOutstanding(_)).WillRepeatedly(Return(true));
    // Packets [1, 99] are considered as lost, although stream frame in packet
    // 2 is not outstanding.
    EXPECT_CALL(notifier_, OnFrameLost(_)).Times(99);
  }
  manager_.OnAckFrameStart(QuicPacketNumber(102), QuicTime::Delta::Zero(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(102), QuicPacketNumber(103));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
}

TEST_P(QuicSentPacketManagerTest, TwoRetransmissionTimeoutsAckSecond) {
  // Send 1 packet.
  SendDataPacket(1);

  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .WillOnce(WithArgs<1>(Invoke(
            [this](TransmissionType type) { RetransmitDataPacket(2, type); })));
  }
  manager_.OnRetransmissionTimeout();
  if (manager_.session_decides_what_to_write()) {
    EXPECT_EQ(2 * kDefaultLength, manager_.GetBytesInFlight());
  } else {
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
    EXPECT_EQ(kDefaultLength, manager_.GetBytesInFlight());
    RetransmitNextPacket(2);
    EXPECT_FALSE(manager_.HasPendingRetransmissions());
  }

  // Rto a second time.
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .WillOnce(WithArgs<1>(Invoke(
            [this](TransmissionType type) { RetransmitDataPacket(3, type); })));
  }
  manager_.OnRetransmissionTimeout();
  if (manager_.session_decides_what_to_write()) {
    EXPECT_EQ(3 * kDefaultLength, manager_.GetBytesInFlight());
  } else {
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
    EXPECT_EQ(2 * kDefaultLength, manager_.GetBytesInFlight());
    RetransmitNextPacket(3);
    EXPECT_FALSE(manager_.HasPendingRetransmissions());
  }

  // Ack a retransmission and ensure OnRetransmissionTimeout is called.
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  ExpectAck(2);
  manager_.OnAckFrameStart(QuicPacketNumber(2), QuicTime::Delta::Zero(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(3));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));

  // The original packet and newest should be outstanding.
  EXPECT_EQ(2 * kDefaultLength, manager_.GetBytesInFlight());
}

TEST_P(QuicSentPacketManagerTest, TwoRetransmissionTimeoutsAckFirst) {
  // Send 1 packet.
  SendDataPacket(1);

  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .WillOnce(WithArgs<1>(Invoke(
            [this](TransmissionType type) { RetransmitDataPacket(2, type); })));
  }
  manager_.OnRetransmissionTimeout();
  if (manager_.session_decides_what_to_write()) {
    EXPECT_EQ(2 * kDefaultLength, manager_.GetBytesInFlight());
  } else {
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
    EXPECT_EQ(kDefaultLength, manager_.GetBytesInFlight());
    RetransmitNextPacket(2);
    EXPECT_FALSE(manager_.HasPendingRetransmissions());
  }

  // Rto a second time.
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .WillOnce(WithArgs<1>(Invoke(
            [this](TransmissionType type) { RetransmitDataPacket(3, type); })));
  }
  manager_.OnRetransmissionTimeout();
  if (manager_.session_decides_what_to_write()) {
    EXPECT_EQ(3 * kDefaultLength, manager_.GetBytesInFlight());
  } else {
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
    EXPECT_EQ(2 * kDefaultLength, manager_.GetBytesInFlight());
    RetransmitNextPacket(3);
    EXPECT_FALSE(manager_.HasPendingRetransmissions());
  }

  // Ack a retransmission and ensure OnRetransmissionTimeout is called.
  EXPECT_CALL(*send_algorithm_, OnRetransmissionTimeout(true));
  ExpectAck(3);
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Zero(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(3), QuicPacketNumber(4));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));

  // The first two packets should still be outstanding.
  EXPECT_EQ(2 * kDefaultLength, manager_.GetBytesInFlight());
}

TEST_P(QuicSentPacketManagerTest, GetTransmissionTime) {
  EXPECT_EQ(QuicTime::Zero(), manager_.GetRetransmissionTime());
}

TEST_P(QuicSentPacketManagerTest, GetTransmissionTimeCryptoHandshake) {
  QuicTime crypto_packet_send_time = clock_.Now();
  SendCryptoPacket(1);

  // Check the min.
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->set_initial_rtt(QuicTime::Delta::FromMilliseconds(1));
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromMilliseconds(10),
            manager_.GetRetransmissionTime());

  // Test with a standard smoothed RTT.
  rtt_stats->set_initial_rtt(QuicTime::Delta::FromMilliseconds(100));

  QuicTime::Delta srtt = rtt_stats->initial_rtt();
  QuicTime expected_time = clock_.Now() + 1.5 * srtt;
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());

  // Retransmit the packet by invoking the retransmission timeout.
  clock_.AdvanceTime(1.5 * srtt);
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .WillOnce(InvokeWithoutArgs([this]() { RetransmitCryptoPacket(2); }));
    // When session decides what to write, crypto_packet_send_time gets updated.
    crypto_packet_send_time = clock_.Now();
  }
  manager_.OnRetransmissionTimeout();
  if (!manager_.session_decides_what_to_write()) {
    RetransmitNextPacket(2);
  }

  // The retransmission time should now be twice as far in the future.
  expected_time = crypto_packet_send_time + srtt * 2 * 1.5;
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());

  // Retransmit the packet for the 2nd time.
  clock_.AdvanceTime(2 * 1.5 * srtt);
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .WillOnce(InvokeWithoutArgs([this]() { RetransmitCryptoPacket(3); }));
    // When session decides what to write, crypto_packet_send_time gets updated.
    crypto_packet_send_time = clock_.Now();
  }
  manager_.OnRetransmissionTimeout();
  if (!manager_.session_decides_what_to_write()) {
    RetransmitNextPacket(3);
  }

  // Verify exponential backoff of the retransmission timeout.
  expected_time = crypto_packet_send_time + srtt * 4 * 1.5;
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());
}

TEST_P(QuicSentPacketManagerTest,
       GetConservativeTransmissionTimeCryptoHandshake) {
  QuicConfig config;
  QuicTagVector options;
  options.push_back(kCONH);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  // Calling SetFromConfig requires mocking out some send algorithm methods.
  EXPECT_CALL(*send_algorithm_, PacingRate(_))
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillRepeatedly(Return(10 * kDefaultTCPMSS));

  QuicTime crypto_packet_send_time = clock_.Now();
  SendCryptoPacket(1);

  // Check the min.
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->set_initial_rtt(QuicTime::Delta::FromMilliseconds(1));
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromMilliseconds(25),
            manager_.GetRetransmissionTime());

  // Test with a standard smoothed RTT.
  rtt_stats->set_initial_rtt(QuicTime::Delta::FromMilliseconds(100));

  QuicTime::Delta srtt = rtt_stats->initial_rtt();
  QuicTime expected_time = clock_.Now() + 2 * srtt;
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());

  // Retransmit the packet by invoking the retransmission timeout.
  clock_.AdvanceTime(2 * srtt);
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .WillOnce(InvokeWithoutArgs([this]() { RetransmitCryptoPacket(2); }));
    crypto_packet_send_time = clock_.Now();
  }
  manager_.OnRetransmissionTimeout();
  if (!manager_.session_decides_what_to_write()) {
    RetransmitNextPacket(2);
  }

  // The retransmission time should now be twice as far in the future.
  expected_time = crypto_packet_send_time + srtt * 2 * 2;
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());
}

TEST_P(QuicSentPacketManagerTest, GetTransmissionTimeTailLossProbe) {
  QuicSentPacketManagerPeer::SetMaxTailLossProbes(&manager_, 2);
  SendDataPacket(1);
  SendDataPacket(2);

  // Check the min.
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->set_initial_rtt(QuicTime::Delta::FromMilliseconds(1));
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromMilliseconds(10),
            manager_.GetRetransmissionTime());

  // Test with a standard smoothed RTT.
  rtt_stats->set_initial_rtt(QuicTime::Delta::FromMilliseconds(100));
  QuicTime::Delta srtt = rtt_stats->initial_rtt();
  QuicTime::Delta expected_tlp_delay = 2 * srtt;
  QuicTime expected_time = clock_.Now() + expected_tlp_delay;
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());

  // Retransmit the packet by invoking the retransmission timeout.
  clock_.AdvanceTime(expected_tlp_delay);
  manager_.OnRetransmissionTimeout();
  EXPECT_EQ(QuicTime::Delta::Zero(), manager_.TimeUntilSend(clock_.Now()));
  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .WillOnce(WithArgs<1>(Invoke(
            [this](TransmissionType type) { RetransmitDataPacket(3, type); })));
  }
  EXPECT_TRUE(manager_.MaybeRetransmitTailLossProbe());
  if (!manager_.session_decides_what_to_write()) {
    EXPECT_TRUE(manager_.HasPendingRetransmissions());
    RetransmitNextPacket(3);
  }
  EXPECT_CALL(*send_algorithm_, CanSend(_)).WillOnce(Return(false));
  EXPECT_EQ(QuicTime::Delta::Infinite(), manager_.TimeUntilSend(clock_.Now()));
  EXPECT_FALSE(manager_.HasPendingRetransmissions());

  expected_time = clock_.Now() + expected_tlp_delay;
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());
}

TEST_P(QuicSentPacketManagerTest, GetTransmissionTimeSpuriousRTO) {
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(100),
                       QuicTime::Delta::Zero(), QuicTime::Zero());

  SendDataPacket(1);
  SendDataPacket(2);
  SendDataPacket(3);
  SendDataPacket(4);

  QuicTime::Delta expected_rto_delay =
      rtt_stats->smoothed_rtt() + 4 * rtt_stats->mean_deviation();
  QuicTime expected_time = clock_.Now() + expected_rto_delay;
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());

  // Retransmit the packet by invoking the retransmission timeout.
  clock_.AdvanceTime(expected_rto_delay);
  if (manager_.session_decides_what_to_write()) {
    EXPECT_CALL(notifier_, RetransmitFrames(_, _))
        .Times(2)
        .WillOnce(WithArgs<1>(Invoke(
            [this](TransmissionType type) { RetransmitDataPacket(5, type); })))
        .WillOnce(WithArgs<1>(Invoke(
            [this](TransmissionType type) { RetransmitDataPacket(6, type); })));
  }
  manager_.OnRetransmissionTimeout();
  if (!manager_.session_decides_what_to_write()) {
    // All packets are still considered inflight.
    EXPECT_EQ(4 * kDefaultLength, manager_.GetBytesInFlight());
    RetransmitNextPacket(5);
    RetransmitNextPacket(6);
  }
  // All previous packets are inflight, plus two rto retransmissions.
  EXPECT_EQ(6 * kDefaultLength, manager_.GetBytesInFlight());
  EXPECT_FALSE(manager_.HasPendingRetransmissions());

  // The delay should double the second time.
  expected_time = clock_.Now() + expected_rto_delay + expected_rto_delay;
  // Once we always base the timer on the right edge, leaving the older packets
  // in flight doesn't change the timeout.
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());

  // Ack a packet before the first RTO and ensure the RTO timeout returns to the
  // original value and OnRetransmissionTimeout is not called or reverted.
  ExpectAck(2);
  manager_.OnAckFrameStart(QuicPacketNumber(2), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(3));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
  EXPECT_FALSE(manager_.HasPendingRetransmissions());
  EXPECT_EQ(5 * kDefaultLength, manager_.GetBytesInFlight());

  // Wait 2RTTs from now for the RTO, since it's the max of the RTO time
  // and the TLP time.  In production, there would always be two TLP's first.
  // Since retransmission was spurious, smoothed_rtt_ is expired, and replaced
  // by the latest RTT sample of 500ms.
  expected_time = clock_.Now() + QuicTime::Delta::FromMilliseconds(1000);
  // Once we always base the timer on the right edge, leaving the older packets
  // in flight doesn't change the timeout.
  EXPECT_EQ(expected_time, manager_.GetRetransmissionTime());
}

TEST_P(QuicSentPacketManagerTest, GetTransmissionDelayMin) {
  SendDataPacket(1);
  // Provide a 1ms RTT sample.
  const_cast<RttStats*>(manager_.GetRttStats())
      ->UpdateRtt(QuicTime::Delta::FromMilliseconds(1), QuicTime::Delta::Zero(),
                  QuicTime::Zero());
  QuicTime::Delta delay = QuicTime::Delta::FromMilliseconds(200);

  // If the delay is smaller than the min, ensure it exponentially backs off
  // from the min.
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(delay,
              QuicSentPacketManagerPeer::GetRetransmissionDelay(&manager_));
    EXPECT_EQ(delay,
              QuicSentPacketManagerPeer::GetRetransmissionDelay(&manager_, i));
    delay = delay + delay;
    if (manager_.session_decides_what_to_write()) {
      EXPECT_CALL(notifier_, RetransmitFrames(_, _))
          .WillOnce(WithArgs<1>(Invoke([this, i](TransmissionType type) {
            RetransmitDataPacket(i + 2, type);
          })));
    }
    manager_.OnRetransmissionTimeout();
    if (!manager_.session_decides_what_to_write()) {
      RetransmitNextPacket(i + 2);
    }
  }
}

TEST_P(QuicSentPacketManagerTest, GetTransmissionDelayMax) {
  SendDataPacket(1);
  // Provide a 60s RTT sample.
  const_cast<RttStats*>(manager_.GetRttStats())
      ->UpdateRtt(QuicTime::Delta::FromSeconds(60), QuicTime::Delta::Zero(),
                  QuicTime::Zero());

  EXPECT_EQ(QuicTime::Delta::FromSeconds(60),
            QuicSentPacketManagerPeer::GetRetransmissionDelay(&manager_));
  EXPECT_EQ(QuicTime::Delta::FromSeconds(60),
            QuicSentPacketManagerPeer::GetRetransmissionDelay(&manager_, 0));
}

TEST_P(QuicSentPacketManagerTest, GetTransmissionDelayExponentialBackoff) {
  SendDataPacket(1);
  QuicTime::Delta delay = QuicTime::Delta::FromMilliseconds(500);

  // Delay should back off exponentially.
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(delay,
              QuicSentPacketManagerPeer::GetRetransmissionDelay(&manager_));
    EXPECT_EQ(delay,
              QuicSentPacketManagerPeer::GetRetransmissionDelay(&manager_, i));
    delay = delay + delay;
    if (manager_.session_decides_what_to_write()) {
      EXPECT_CALL(notifier_, RetransmitFrames(_, _))
          .WillOnce(WithArgs<1>(Invoke([this, i](TransmissionType type) {
            RetransmitDataPacket(i + 2, type);
          })));
    }
    manager_.OnRetransmissionTimeout();
    if (!manager_.session_decides_what_to_write()) {
      RetransmitNextPacket(i + 2);
    }
  }
}

TEST_P(QuicSentPacketManagerTest, RetransmissionDelay) {
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  const int64_t kRttMs = 250;
  const int64_t kDeviationMs = 5;

  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(kRttMs),
                       QuicTime::Delta::Zero(), clock_.Now());

  // Initial value is to set the median deviation to half of the initial rtt,
  // the median in then multiplied by a factor of 4 and finally the smoothed rtt
  // is added which is the initial rtt.
  QuicTime::Delta expected_delay =
      QuicTime::Delta::FromMilliseconds(kRttMs + kRttMs / 2 * 4);
  EXPECT_EQ(expected_delay,
            QuicSentPacketManagerPeer::GetRetransmissionDelay(&manager_));
  EXPECT_EQ(expected_delay,
            QuicSentPacketManagerPeer::GetRetransmissionDelay(&manager_, 0));

  for (int i = 0; i < 100; ++i) {
    // Run to make sure that we converge.
    rtt_stats->UpdateRtt(
        QuicTime::Delta::FromMilliseconds(kRttMs + kDeviationMs),
        QuicTime::Delta::Zero(), clock_.Now());
    rtt_stats->UpdateRtt(
        QuicTime::Delta::FromMilliseconds(kRttMs - kDeviationMs),
        QuicTime::Delta::Zero(), clock_.Now());
  }
  expected_delay = QuicTime::Delta::FromMilliseconds(kRttMs + kDeviationMs * 4);

  EXPECT_NEAR(kRttMs, rtt_stats->smoothed_rtt().ToMilliseconds(), 1);
  EXPECT_NEAR(expected_delay.ToMilliseconds(),
              QuicSentPacketManagerPeer::GetRetransmissionDelay(&manager_)
                  .ToMilliseconds(),
              1);
  EXPECT_EQ(QuicSentPacketManagerPeer::GetRetransmissionDelay(&manager_, 0),
            QuicSentPacketManagerPeer::GetRetransmissionDelay(&manager_));
}

TEST_P(QuicSentPacketManagerTest, GetLossDelay) {
  auto loss_algorithm = QuicMakeUnique<MockLossAlgorithm>();
  QuicSentPacketManagerPeer::SetLossAlgorithm(&manager_, loss_algorithm.get());

  EXPECT_CALL(*loss_algorithm, GetLossTimeout())
      .WillRepeatedly(Return(QuicTime::Zero()));
  SendDataPacket(1);
  SendDataPacket(2);

  // Handle an ack which causes the loss algorithm to be evaluated and
  // set the loss timeout.
  ExpectAck(2);
  EXPECT_CALL(*loss_algorithm, DetectLosses(_, _, _, _, _, _));
  manager_.OnAckFrameStart(QuicPacketNumber(2), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(3));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));

  QuicTime timeout(clock_.Now() + QuicTime::Delta::FromMilliseconds(10));
  EXPECT_CALL(*loss_algorithm, GetLossTimeout())
      .WillRepeatedly(Return(timeout));
  EXPECT_EQ(timeout, manager_.GetRetransmissionTime());

  // Fire the retransmission timeout and ensure the loss detection algorithm
  // is invoked.
  EXPECT_CALL(*loss_algorithm, DetectLosses(_, _, _, _, _, _));
  manager_.OnRetransmissionTimeout();
}

TEST_P(QuicSentPacketManagerTest, NegotiateTimeLossDetectionFromOptions) {
  EXPECT_EQ(kNack, QuicSentPacketManagerPeer::GetLossAlgorithm(&manager_)
                       ->GetLossDetectionType());

  QuicConfig config;
  QuicTagVector options;
  options.push_back(kTIME);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);

  EXPECT_EQ(kTime, QuicSentPacketManagerPeer::GetLossAlgorithm(&manager_)
                       ->GetLossDetectionType());
}

TEST_P(QuicSentPacketManagerTest, NegotiateCongestionControlFromOptions) {
  QuicConfig config;
  QuicTagVector options;

  options.push_back(kRENO);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kRenoBytes, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                            ->GetCongestionControlType());

  options.clear();
  options.push_back(kTBBR);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kBBR, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                      ->GetCongestionControlType());

  options.clear();
  options.push_back(kBYTE);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kCubicBytes, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                             ->GetCongestionControlType());
  options.clear();
  options.push_back(kRENO);
  options.push_back(kBYTE);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kRenoBytes, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                            ->GetCongestionControlType());
}

TEST_P(QuicSentPacketManagerTest, NegotiateClientCongestionControlFromOptions) {
  QuicConfig config;
  QuicTagVector options;

  // No change if the server receives client options.
  const SendAlgorithmInterface* mock_sender =
      QuicSentPacketManagerPeer::GetSendAlgorithm(manager_);
  options.push_back(kRENO);
  config.SetClientConnectionOptions(options);
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(mock_sender, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_));

  // Change the congestion control on the client with client options.
  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kRenoBytes, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                            ->GetCongestionControlType());

  options.clear();
  options.push_back(kTBBR);
  config.SetClientConnectionOptions(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kBBR, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                      ->GetCongestionControlType());

  options.clear();
  options.push_back(kBYTE);
  config.SetClientConnectionOptions(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kCubicBytes, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                             ->GetCongestionControlType());

  options.clear();
  options.push_back(kRENO);
  options.push_back(kBYTE);
  config.SetClientConnectionOptions(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);
  EXPECT_EQ(kRenoBytes, QuicSentPacketManagerPeer::GetSendAlgorithm(manager_)
                            ->GetCongestionControlType());
}

TEST_P(QuicSentPacketManagerTest, NegotiateNumConnectionsFromOptions) {
  QuicConfig config;
  QuicTagVector options;

  options.push_back(k1CON);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(*send_algorithm_, SetNumEmulatedConnections(1));
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  manager_.SetFromConfig(config);

  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);
  QuicConfig client_config;
  client_config.SetConnectionOptionsToSend(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(*send_algorithm_, SetNumEmulatedConnections(1));
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  manager_.SetFromConfig(client_config);
}

TEST_P(QuicSentPacketManagerTest, NegotiateNoMinTLPFromOptionsAtServer) {
  QuicConfig config;
  QuicTagVector options;

  options.push_back(kMAD2);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, PacingRate(_))
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillOnce(Return(10 * kDefaultTCPMSS));
  manager_.SetFromConfig(config);
  // Set the initial RTT to 1us.
  QuicSentPacketManagerPeer::GetRttStats(&manager_)->set_initial_rtt(
      QuicTime::Delta::FromMicroseconds(1));
  // The TLP with fewer than 2 packets outstanding includes 1/2 min RTO(200ms).
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(100002),
            QuicSentPacketManagerPeer::GetTailLossProbeDelay(&manager_));
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(100002),
            QuicSentPacketManagerPeer::GetTailLossProbeDelay(&manager_, 0));

  // Send two packets, and the TLP should be 2 us.
  SendDataPacket(1);
  SendDataPacket(2);
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(2),
            QuicSentPacketManagerPeer::GetTailLossProbeDelay(&manager_));
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(2),
            QuicSentPacketManagerPeer::GetTailLossProbeDelay(&manager_, 0));
}

TEST_P(QuicSentPacketManagerTest, NegotiateNoMinTLPFromOptionsAtClient) {
  QuicConfig client_config;
  QuicTagVector options;

  options.push_back(kMAD2);
  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);
  client_config.SetConnectionOptionsToSend(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*send_algorithm_, PacingRate(_))
      .WillRepeatedly(Return(QuicBandwidth::Zero()));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .WillOnce(Return(10 * kDefaultTCPMSS));
  manager_.SetFromConfig(client_config);
  // Set the initial RTT to 1us.
  QuicSentPacketManagerPeer::GetRttStats(&manager_)->set_initial_rtt(
      QuicTime::Delta::FromMicroseconds(1));
  // The TLP with fewer than 2 packets outstanding includes 1/2 min RTO(200ms).
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(100002),
            QuicSentPacketManagerPeer::GetTailLossProbeDelay(&manager_));
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(100002),
            QuicSentPacketManagerPeer::GetTailLossProbeDelay(&manager_, 0));
  // Send two packets, and the TLP should be 2 us.
  SendDataPacket(1);
  SendDataPacket(2);
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(2),
            QuicSentPacketManagerPeer::GetTailLossProbeDelay(&manager_));
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(2),
            QuicSentPacketManagerPeer::GetTailLossProbeDelay(&manager_, 0));
}

TEST_P(QuicSentPacketManagerTest, NegotiateIETFTLPFromOptionsAtServer) {
  QuicConfig config;
  QuicTagVector options;

  options.push_back(kMAD4);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  manager_.SetFromConfig(config);
  // Provide an RTT measurement of 100ms.
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(100),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  // Expect 1.5x * SRTT + 0ms MAD
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(150),
            QuicSentPacketManagerPeer::GetTailLossProbeDelay(&manager_));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(150),
            QuicSentPacketManagerPeer::GetTailLossProbeDelay(&manager_, 0));
  // Expect 1.5x * SRTT + 50ms MAD
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(150),
                       QuicTime::Delta::FromMilliseconds(50), QuicTime::Zero());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(100), rtt_stats->smoothed_rtt());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(200),
            QuicSentPacketManagerPeer::GetTailLossProbeDelay(&manager_));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(200),
            QuicSentPacketManagerPeer::GetTailLossProbeDelay(&manager_, 0));
}

TEST_P(QuicSentPacketManagerTest, NegotiateIETFTLPFromOptionsAtClient) {
  QuicConfig client_config;
  QuicTagVector options;

  options.push_back(kMAD4);
  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);
  client_config.SetConnectionOptionsToSend(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  manager_.SetFromConfig(client_config);
  // Provide an RTT measurement of 100ms.
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(100),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  // Expect 1.5x * SRTT + 0ms MAD
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(150),
            QuicSentPacketManagerPeer::GetTailLossProbeDelay(&manager_));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(150),
            QuicSentPacketManagerPeer::GetTailLossProbeDelay(&manager_, 0));
  // Expect 1.5x * SRTT + 50ms MAD
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(150),
                       QuicTime::Delta::FromMilliseconds(50), QuicTime::Zero());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(100), rtt_stats->smoothed_rtt());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(200),
            QuicSentPacketManagerPeer::GetTailLossProbeDelay(&manager_));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(200),
            QuicSentPacketManagerPeer::GetTailLossProbeDelay(&manager_, 0));
}

TEST_P(QuicSentPacketManagerTest, NegotiateNoMinRTOFromOptionsAtServer) {
  QuicConfig config;
  QuicTagVector options;

  options.push_back(kMAD3);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  manager_.SetFromConfig(config);
  // Provide one RTT measurement, because otherwise we use the default of 500ms.
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMicroseconds(1),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(1),
            QuicSentPacketManagerPeer::GetRetransmissionDelay(&manager_));
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(1),
            QuicSentPacketManagerPeer::GetRetransmissionDelay(&manager_, 0));
  // The TLP with fewer than 2 packets outstanding includes 1/2 min RTO(0ms).
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(2),
            QuicSentPacketManagerPeer::GetTailLossProbeDelay(&manager_));
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(2),
            QuicSentPacketManagerPeer::GetTailLossProbeDelay(&manager_, 0));
}

TEST_P(QuicSentPacketManagerTest, NegotiateNoMinRTOFromOptionsAtClient) {
  QuicConfig client_config;
  QuicTagVector options;

  options.push_back(kMAD3);
  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);
  client_config.SetConnectionOptionsToSend(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  manager_.SetFromConfig(client_config);
  // Provide one RTT measurement, because otherwise we use the default of 500ms.
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  rtt_stats->UpdateRtt(QuicTime::Delta::FromMicroseconds(1),
                       QuicTime::Delta::Zero(), QuicTime::Zero());
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(1),
            QuicSentPacketManagerPeer::GetRetransmissionDelay(&manager_));
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(1),
            QuicSentPacketManagerPeer::GetRetransmissionDelay(&manager_, 0));
  // The TLP with fewer than 2 packets outstanding includes 1/2 min RTO(0ms).
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(2),
            QuicSentPacketManagerPeer::GetTailLossProbeDelay(&manager_));
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(2),
            QuicSentPacketManagerPeer::GetTailLossProbeDelay(&manager_, 0));
}

TEST_P(QuicSentPacketManagerTest, NegotiateNoTLPFromOptionsAtServer) {
  QuicConfig config;
  QuicTagVector options;

  options.push_back(kNTLP);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  manager_.SetFromConfig(config);
  EXPECT_EQ(0u, QuicSentPacketManagerPeer::GetMaxTailLossProbes(&manager_));
}

TEST_P(QuicSentPacketManagerTest, NegotiateNoTLPFromOptionsAtClient) {
  QuicConfig client_config;
  QuicTagVector options;

  options.push_back(kNTLP);
  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);
  client_config.SetConnectionOptionsToSend(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  manager_.SetFromConfig(client_config);
  EXPECT_EQ(0u, QuicSentPacketManagerPeer::GetMaxTailLossProbes(&manager_));
}

TEST_P(QuicSentPacketManagerTest, Negotiate1TLPFromOptionsAtServer) {
  QuicConfig config;
  QuicTagVector options;

  options.push_back(k1TLP);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  manager_.SetFromConfig(config);
  EXPECT_EQ(1u, QuicSentPacketManagerPeer::GetMaxTailLossProbes(&manager_));
}

TEST_P(QuicSentPacketManagerTest, Negotiate1TLPFromOptionsAtClient) {
  QuicConfig client_config;
  QuicTagVector options;

  options.push_back(k1TLP);
  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);
  client_config.SetConnectionOptionsToSend(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  manager_.SetFromConfig(client_config);
  EXPECT_EQ(1u, QuicSentPacketManagerPeer::GetMaxTailLossProbes(&manager_));
}

TEST_P(QuicSentPacketManagerTest, NegotiateTLPRttFromOptionsAtServer) {
  QuicConfig config;
  QuicTagVector options;

  options.push_back(kTLPR);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  manager_.SetFromConfig(config);
  EXPECT_TRUE(
      QuicSentPacketManagerPeer::GetEnableHalfRttTailLossProbe(&manager_));
}

TEST_P(QuicSentPacketManagerTest, NegotiateTLPRttFromOptionsAtClient) {
  QuicConfig client_config;
  QuicTagVector options;

  options.push_back(kTLPR);
  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);
  client_config.SetConnectionOptionsToSend(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  manager_.SetFromConfig(client_config);
  EXPECT_TRUE(
      QuicSentPacketManagerPeer::GetEnableHalfRttTailLossProbe(&manager_));
}

TEST_P(QuicSentPacketManagerTest, NegotiateNewRTOFromOptionsAtServer) {
  EXPECT_FALSE(QuicSentPacketManagerPeer::GetUseNewRto(&manager_));
  QuicConfig config;
  QuicTagVector options;

  options.push_back(kNRTO);
  QuicConfigPeer::SetReceivedConnectionOptions(&config, options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  manager_.SetFromConfig(config);
  EXPECT_TRUE(QuicSentPacketManagerPeer::GetUseNewRto(&manager_));
}

TEST_P(QuicSentPacketManagerTest, NegotiateNewRTOFromOptionsAtClient) {
  EXPECT_FALSE(QuicSentPacketManagerPeer::GetUseNewRto(&manager_));
  QuicConfig client_config;
  QuicTagVector options;

  options.push_back(kNRTO);
  QuicSentPacketManagerPeer::SetPerspective(&manager_, Perspective::IS_CLIENT);
  client_config.SetConnectionOptionsToSend(options);
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  manager_.SetFromConfig(client_config);
  EXPECT_TRUE(QuicSentPacketManagerPeer::GetUseNewRto(&manager_));
}

TEST_P(QuicSentPacketManagerTest, UseInitialRoundTripTimeToSend) {
  QuicTime::Delta initial_rtt = QuicTime::Delta::FromMilliseconds(325);
  EXPECT_NE(initial_rtt, manager_.GetRttStats()->smoothed_rtt());

  QuicConfig config;
  config.SetInitialRoundTripTimeUsToSend(initial_rtt.ToMicroseconds());
  EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _));
  EXPECT_CALL(*network_change_visitor_, OnCongestionChange());
  manager_.SetFromConfig(config);

  EXPECT_EQ(QuicTime::Delta::Zero(), manager_.GetRttStats()->smoothed_rtt());
  EXPECT_EQ(initial_rtt, manager_.GetRttStats()->initial_rtt());
}

TEST_P(QuicSentPacketManagerTest, ResumeConnectionState) {
  // The sent packet manager should use the RTT from CachedNetworkParameters if
  // it is provided.
  const QuicTime::Delta kRtt = QuicTime::Delta::FromMilliseconds(1234);
  CachedNetworkParameters cached_network_params;
  cached_network_params.set_min_rtt_ms(kRtt.ToMilliseconds());

  EXPECT_CALL(*send_algorithm_,
              AdjustNetworkParameters(QuicBandwidth::Zero(), kRtt, false));
  EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
      .Times(testing::AnyNumber());
  manager_.ResumeConnectionState(cached_network_params, false);
  EXPECT_EQ(kRtt, manager_.GetRttStats()->initial_rtt());
}

TEST_P(QuicSentPacketManagerTest, ConnectionMigrationUnspecifiedChange) {
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  QuicTime::Delta default_init_rtt = rtt_stats->initial_rtt();
  rtt_stats->set_initial_rtt(default_init_rtt * 2);
  EXPECT_EQ(2 * default_init_rtt, rtt_stats->initial_rtt());

  QuicSentPacketManagerPeer::SetConsecutiveRtoCount(&manager_, 1);
  EXPECT_EQ(1u, manager_.GetConsecutiveRtoCount());
  QuicSentPacketManagerPeer::SetConsecutiveTlpCount(&manager_, 2);
  EXPECT_EQ(2u, manager_.GetConsecutiveTlpCount());

  EXPECT_CALL(*send_algorithm_, OnConnectionMigration());
  manager_.OnConnectionMigration(IPV4_TO_IPV4_CHANGE);

  EXPECT_EQ(default_init_rtt, rtt_stats->initial_rtt());
  EXPECT_EQ(0u, manager_.GetConsecutiveRtoCount());
  EXPECT_EQ(0u, manager_.GetConsecutiveTlpCount());
}

TEST_P(QuicSentPacketManagerTest, ConnectionMigrationIPSubnetChange) {
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  QuicTime::Delta default_init_rtt = rtt_stats->initial_rtt();
  rtt_stats->set_initial_rtt(default_init_rtt * 2);
  EXPECT_EQ(2 * default_init_rtt, rtt_stats->initial_rtt());

  QuicSentPacketManagerPeer::SetConsecutiveRtoCount(&manager_, 1);
  EXPECT_EQ(1u, manager_.GetConsecutiveRtoCount());
  QuicSentPacketManagerPeer::SetConsecutiveTlpCount(&manager_, 2);
  EXPECT_EQ(2u, manager_.GetConsecutiveTlpCount());

  manager_.OnConnectionMigration(IPV4_SUBNET_CHANGE);

  EXPECT_EQ(2 * default_init_rtt, rtt_stats->initial_rtt());
  EXPECT_EQ(1u, manager_.GetConsecutiveRtoCount());
  EXPECT_EQ(2u, manager_.GetConsecutiveTlpCount());
}

TEST_P(QuicSentPacketManagerTest, ConnectionMigrationPortChange) {
  RttStats* rtt_stats = const_cast<RttStats*>(manager_.GetRttStats());
  QuicTime::Delta default_init_rtt = rtt_stats->initial_rtt();
  rtt_stats->set_initial_rtt(default_init_rtt * 2);
  EXPECT_EQ(2 * default_init_rtt, rtt_stats->initial_rtt());

  QuicSentPacketManagerPeer::SetConsecutiveRtoCount(&manager_, 1);
  EXPECT_EQ(1u, manager_.GetConsecutiveRtoCount());
  QuicSentPacketManagerPeer::SetConsecutiveTlpCount(&manager_, 2);
  EXPECT_EQ(2u, manager_.GetConsecutiveTlpCount());

  manager_.OnConnectionMigration(PORT_CHANGE);

  EXPECT_EQ(2 * default_init_rtt, rtt_stats->initial_rtt());
  EXPECT_EQ(1u, manager_.GetConsecutiveRtoCount());
  EXPECT_EQ(2u, manager_.GetConsecutiveTlpCount());
}

TEST_P(QuicSentPacketManagerTest, PathMtuIncreased) {
  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, BytesInFlight(), QuicPacketNumber(1), _, _));
  SerializedPacket packet(QuicPacketNumber(1), PACKET_4BYTE_PACKET_NUMBER,
                          nullptr, kDefaultLength + 100, false, false);
  manager_.OnPacketSent(&packet, QuicPacketNumber(), clock_.Now(),
                        NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);

  // Ack the large packet and expect the path MTU to increase.
  ExpectAck(1);
  EXPECT_CALL(*network_change_visitor_,
              OnPathMtuIncreased(kDefaultLength + 100));
  QuicAckFrame ack_frame = InitAckFrame(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
}

TEST_P(QuicSentPacketManagerTest, OnAckRangeSlowPath) {
  // Send packets 1 - 20.
  for (size_t i = 1; i <= 20; ++i) {
    SendDataPacket(i);
  }
  // Ack [5, 7), [10, 12), [15, 17).
  uint64_t acked1[] = {5, 6, 10, 11, 15, 16};
  uint64_t lost1[] = {1, 2, 3, 4, 7, 8, 9, 12, 13};
  ExpectAcksAndLosses(true, acked1, QUIC_ARRAYSIZE(acked1), lost1,
                      QUIC_ARRAYSIZE(lost1));
  EXPECT_CALL(notifier_, OnFrameLost(_)).Times(AnyNumber());
  manager_.OnAckFrameStart(QuicPacketNumber(16), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(15), QuicPacketNumber(17));
  manager_.OnAckRange(QuicPacketNumber(10), QuicPacketNumber(12));
  manager_.OnAckRange(QuicPacketNumber(5), QuicPacketNumber(7));
  // Make sure empty range does not harm.
  manager_.OnAckRange(QuicPacketNumber(4), QuicPacketNumber(4));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));

  // Ack [4, 8), [9, 13), [14, 21).
  uint64_t acked2[] = {4, 7, 9, 12, 14, 17, 18, 19, 20};
  ExpectAcksAndLosses(true, acked2, QUIC_ARRAYSIZE(acked2), nullptr, 0);
  manager_.OnAckFrameStart(QuicPacketNumber(20), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(14), QuicPacketNumber(21));
  manager_.OnAckRange(QuicPacketNumber(9), QuicPacketNumber(13));
  manager_.OnAckRange(QuicPacketNumber(4), QuicPacketNumber(8));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
}

TEST_P(QuicSentPacketManagerTest, TolerateReneging) {
  if (!manager_.tolerate_reneging()) {
    return;
  }
  // Send packets 1 - 20.
  for (size_t i = 1; i <= 20; ++i) {
    SendDataPacket(i);
  }
  // Ack [5, 7), [10, 12), [15, 17).
  uint64_t acked1[] = {5, 6, 10, 11, 15, 16};
  uint64_t lost1[] = {1, 2, 3, 4, 7, 8, 9, 12, 13};
  ExpectAcksAndLosses(true, acked1, QUIC_ARRAYSIZE(acked1), lost1,
                      QUIC_ARRAYSIZE(lost1));
  EXPECT_CALL(notifier_, OnFrameLost(_)).Times(AnyNumber());
  manager_.OnAckFrameStart(QuicPacketNumber(16), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(15), QuicPacketNumber(17));
  manager_.OnAckRange(QuicPacketNumber(10), QuicPacketNumber(12));
  manager_.OnAckRange(QuicPacketNumber(5), QuicPacketNumber(7));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));

  // Making sure reneged ACK does not harm. Ack [4, 8), [9, 13).
  uint64_t acked2[] = {4, 7, 9, 12};
  ExpectAcksAndLosses(true, acked2, QUIC_ARRAYSIZE(acked2), nullptr, 0);
  manager_.OnAckFrameStart(QuicPacketNumber(12), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(9), QuicPacketNumber(13));
  manager_.OnAckRange(QuicPacketNumber(4), QuicPacketNumber(8));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
  EXPECT_EQ(QuicPacketNumber(16), manager_.GetLargestObserved());
}

TEST_P(QuicSentPacketManagerTest, MultiplePacketNumberSpaces) {
  if (!GetQuicReloadableFlag(quic_use_uber_loss_algorithm)) {
    return;
  }
  manager_.EnableMultiplePacketNumberSpacesSupport();
  EXPECT_FALSE(
      manager_.GetLargestSentPacket(ENCRYPTION_INITIAL).IsInitialized());
  EXPECT_FALSE(
      manager_.GetLargestAckedPacket(ENCRYPTION_INITIAL).IsInitialized());
  // Send packet 1.
  SendDataPacket(1, ENCRYPTION_INITIAL);
  EXPECT_EQ(QuicPacketNumber(1),
            manager_.GetLargestSentPacket(ENCRYPTION_INITIAL));
  EXPECT_FALSE(
      manager_.GetLargestSentPacket(ENCRYPTION_HANDSHAKE).IsInitialized());
  // Ack packet 1.
  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
  EXPECT_EQ(QuicPacketNumber(1),
            manager_.GetLargestAckedPacket(ENCRYPTION_INITIAL));
  EXPECT_FALSE(
      manager_.GetLargestAckedPacket(ENCRYPTION_HANDSHAKE).IsInitialized());
  // Send packets 2 and 3.
  SendDataPacket(2, ENCRYPTION_HANDSHAKE);
  SendDataPacket(3, ENCRYPTION_HANDSHAKE);
  EXPECT_EQ(QuicPacketNumber(1),
            manager_.GetLargestSentPacket(ENCRYPTION_INITIAL));
  EXPECT_EQ(QuicPacketNumber(3),
            manager_.GetLargestSentPacket(ENCRYPTION_HANDSHAKE));
  EXPECT_FALSE(
      manager_.GetLargestSentPacket(ENCRYPTION_ZERO_RTT).IsInitialized());
  // Ack packet 2.
  ExpectAck(2);
  manager_.OnAckFrameStart(QuicPacketNumber(2), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(3));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_HANDSHAKE));
  EXPECT_EQ(QuicPacketNumber(2),
            manager_.GetLargestAckedPacket(ENCRYPTION_HANDSHAKE));
  EXPECT_FALSE(
      manager_.GetLargestAckedPacket(ENCRYPTION_ZERO_RTT).IsInitialized());
  // Ack packet 3.
  ExpectAck(3);
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(2), QuicPacketNumber(4));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_HANDSHAKE));
  EXPECT_EQ(QuicPacketNumber(3),
            manager_.GetLargestAckedPacket(ENCRYPTION_HANDSHAKE));
  EXPECT_FALSE(
      manager_.GetLargestAckedPacket(ENCRYPTION_ZERO_RTT).IsInitialized());
  // Send packets 4 and 5.
  SendDataPacket(4, ENCRYPTION_ZERO_RTT);
  SendDataPacket(5, ENCRYPTION_ZERO_RTT);
  EXPECT_EQ(QuicPacketNumber(1),
            manager_.GetLargestSentPacket(ENCRYPTION_INITIAL));
  EXPECT_EQ(QuicPacketNumber(3),
            manager_.GetLargestSentPacket(ENCRYPTION_HANDSHAKE));
  EXPECT_EQ(QuicPacketNumber(5),
            manager_.GetLargestSentPacket(ENCRYPTION_ZERO_RTT));
  EXPECT_EQ(QuicPacketNumber(5),
            manager_.GetLargestSentPacket(ENCRYPTION_FORWARD_SECURE));
  // Ack packet 5.
  ExpectAck(5);
  manager_.OnAckFrameStart(QuicPacketNumber(5), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(5), QuicPacketNumber(6));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_FORWARD_SECURE));
  EXPECT_EQ(QuicPacketNumber(3),
            manager_.GetLargestAckedPacket(ENCRYPTION_HANDSHAKE));
  EXPECT_EQ(QuicPacketNumber(5),
            manager_.GetLargestAckedPacket(ENCRYPTION_ZERO_RTT));
  EXPECT_EQ(QuicPacketNumber(5),
            manager_.GetLargestAckedPacket(ENCRYPTION_FORWARD_SECURE));

  // Send packets 6 - 8.
  SendDataPacket(6, ENCRYPTION_FORWARD_SECURE);
  SendDataPacket(7, ENCRYPTION_FORWARD_SECURE);
  SendDataPacket(8, ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(QuicPacketNumber(1),
            manager_.GetLargestSentPacket(ENCRYPTION_INITIAL));
  EXPECT_EQ(QuicPacketNumber(3),
            manager_.GetLargestSentPacket(ENCRYPTION_HANDSHAKE));
  EXPECT_EQ(QuicPacketNumber(8),
            manager_.GetLargestSentPacket(ENCRYPTION_ZERO_RTT));
  EXPECT_EQ(QuicPacketNumber(8),
            manager_.GetLargestSentPacket(ENCRYPTION_FORWARD_SECURE));
  // Ack all packets.
  uint64_t acked[] = {4, 6, 7, 8};
  ExpectAcksAndLosses(true, acked, QUIC_ARRAYSIZE(acked), nullptr, 0);
  manager_.OnAckFrameStart(QuicPacketNumber(8), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(4), QuicPacketNumber(9));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_FORWARD_SECURE));
  EXPECT_EQ(QuicPacketNumber(3),
            manager_.GetLargestAckedPacket(ENCRYPTION_HANDSHAKE));
  EXPECT_EQ(QuicPacketNumber(8),
            manager_.GetLargestAckedPacket(ENCRYPTION_ZERO_RTT));
  EXPECT_EQ(QuicPacketNumber(8),
            manager_.GetLargestAckedPacket(ENCRYPTION_FORWARD_SECURE));
}

TEST_P(QuicSentPacketManagerTest, PacketsGetAckedInWrongPacketNumberSpace) {
  if (!GetQuicReloadableFlag(quic_use_uber_loss_algorithm)) {
    return;
  }
  manager_.EnableMultiplePacketNumberSpacesSupport();
  // Send packet 1.
  SendDataPacket(1, ENCRYPTION_INITIAL);
  // Send packets 2 and 3.
  SendDataPacket(2, ENCRYPTION_HANDSHAKE);
  SendDataPacket(3, ENCRYPTION_HANDSHAKE);

  // ACK packets 2 and 3 in the wrong packet number space.
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(4));
  EXPECT_EQ(PACKETS_ACKED_IN_WRONG_PACKET_NUMBER_SPACE,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));
}

TEST_P(QuicSentPacketManagerTest, PacketsGetAckedInWrongPacketNumberSpace2) {
  if (!GetQuicReloadableFlag(quic_use_uber_loss_algorithm)) {
    return;
  }
  manager_.EnableMultiplePacketNumberSpacesSupport();
  // Send packet 1.
  SendDataPacket(1, ENCRYPTION_INITIAL);
  // Send packets 2 and 3.
  SendDataPacket(2, ENCRYPTION_HANDSHAKE);
  SendDataPacket(3, ENCRYPTION_HANDSHAKE);

  // ACK packet 1 in the wrong packet number space.
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(4));
  EXPECT_EQ(PACKETS_ACKED_IN_WRONG_PACKET_NUMBER_SPACE,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_HANDSHAKE));
}

TEST_P(QuicSentPacketManagerTest,
       ToleratePacketsGetAckedInWrongPacketNumberSpace) {
  if (!GetQuicReloadableFlag(quic_use_uber_loss_algorithm)) {
    return;
  }
  manager_.EnableMultiplePacketNumberSpacesSupport();
  // Send packet 1.
  SendDataPacket(1, ENCRYPTION_INITIAL);
  // Ack packet 1.
  ExpectAck(1);
  manager_.OnAckFrameStart(QuicPacketNumber(1), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(2));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_INITIAL));

  // Send packets 2 and 3.
  SendDataPacket(2, ENCRYPTION_HANDSHAKE);
  SendDataPacket(3, ENCRYPTION_HANDSHAKE);

  // Packet 1 gets acked in the wrong packet number space. Since packet 1 has
  // been acked in the correct packet number space, tolerate it.
  uint64_t acked[] = {2, 3};
  ExpectAcksAndLosses(true, acked, QUIC_ARRAYSIZE(acked), nullptr, 0);
  manager_.OnAckFrameStart(QuicPacketNumber(3), QuicTime::Delta::Infinite(),
                           clock_.Now());
  manager_.OnAckRange(QuicPacketNumber(1), QuicPacketNumber(4));
  EXPECT_EQ(PACKETS_NEWLY_ACKED,
            manager_.OnAckFrameEnd(clock_.Now(), ENCRYPTION_HANDSHAKE));
}

}  // namespace
}  // namespace test
}  // namespace quic
