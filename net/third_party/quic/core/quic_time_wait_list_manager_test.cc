// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_time_wait_list_manager.h"

#include <errno.h>
#include <memory>
#include <ostream>

#include "net/third_party/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quic/core/crypto/null_encrypter.h"
#include "net/third_party/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quic/core/quic_data_reader.h"
#include "net/third_party/quic/core/quic_epoll_alarm_factory.h"
#include "net/third_party/quic/core/quic_epoll_connection_helper.h"
#include "net/third_party/quic/core/quic_framer.h"
#include "net/third_party/quic/core/quic_packet_writer.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/mock_epoll_server.h"
#include "net/third_party/quic/test_tools/mock_quic_session_visitor.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quic/test_tools/quic_time_wait_list_manager_peer.h"

using testing::_;
using testing::Args;
using testing::Assign;
using testing::DoAll;
using testing::Matcher;
using testing::NiceMock;
using testing::Return;
using testing::ReturnPointee;
using testing::StrictMock;
using testing::Truly;

namespace net {
namespace test {
namespace {

const QuicUint128 kTestStatelessResetToken = 1010101;

class FramerVisitorCapturingPublicReset : public NoOpFramerVisitor {
 public:
  FramerVisitorCapturingPublicReset() = default;
  ~FramerVisitorCapturingPublicReset() override = default;

  void OnPublicResetPacket(const QuicPublicResetPacket& public_reset) override {
    public_reset_packet_ = public_reset;
  }

  const QuicPublicResetPacket public_reset_packet() {
    return public_reset_packet_;
  }

  bool IsValidStatelessResetToken(QuicUint128 token) const override {
    return token == kTestStatelessResetToken;
  }

  void OnAuthenticatedIetfStatelessResetPacket(
      const QuicIetfStatelessResetPacket& packet) override {
    stateless_reset_packet_ = packet;
  }

  const QuicIetfStatelessResetPacket stateless_reset_packet() {
    return stateless_reset_packet_;
  }

 private:
  QuicPublicResetPacket public_reset_packet_;
  QuicIetfStatelessResetPacket stateless_reset_packet_;
};

class MockFakeTimeEpollServer : public FakeTimeEpollServer {
 public:
  MOCK_METHOD2(RegisterAlarm,
               void(int64_t timeout_in_us, EpollAlarmCallbackInterface* alarm));
};

class QuicTimeWaitListManagerTest : public QuicTest {
 protected:
  QuicTimeWaitListManagerTest()
      : helper_(&epoll_server_, QuicAllocator::BUFFER_POOL),
        alarm_factory_(&epoll_server_),
        time_wait_list_manager_(&writer_, &visitor_, &helper_, &alarm_factory_),
        connection_id_(45),
        client_address_(TestPeerIPAddress(), kTestPort),
        writer_is_blocked_(false) {}

  ~QuicTimeWaitListManagerTest() override = default;

  void SetUp() override {
    EXPECT_CALL(writer_, IsWriteBlocked())
        .WillRepeatedly(ReturnPointee(&writer_is_blocked_));
    EXPECT_CALL(writer_, IsWriteBlockedDataBuffered())
        .WillRepeatedly(Return(false));
  }

  void AddConnectionId(QuicConnectionId connection_id) {
    AddConnectionId(connection_id, QuicVersionMax(),
                    /*connection_rejected_statelessly=*/false, nullptr);
  }

  void AddStatelessConnectionId(QuicConnectionId connection_id) {
    std::vector<std::unique_ptr<QuicEncryptedPacket>> termination_packets;
    termination_packets.push_back(std::unique_ptr<QuicEncryptedPacket>(
        new QuicEncryptedPacket(nullptr, 0, false)));
    time_wait_list_manager_.AddConnectionIdToTimeWait(
        connection_id, QuicVersionMax(), false,
        /*connection_rejected_statelessly=*/true, &termination_packets);
  }

  void AddConnectionId(
      QuicConnectionId connection_id,
      ParsedQuicVersion version,
      bool connection_rejected_statelessly,
      std::vector<std::unique_ptr<QuicEncryptedPacket>>* packets) {
    time_wait_list_manager_.AddConnectionIdToTimeWait(
        connection_id, version, version.transport_version == QUIC_VERSION_99,
        connection_rejected_statelessly, packets);
  }

  bool IsConnectionIdInTimeWait(QuicConnectionId connection_id) {
    return time_wait_list_manager_.IsConnectionIdInTimeWait(connection_id);
  }

  void ProcessPacket(QuicConnectionId connection_id) {
    time_wait_list_manager_.ProcessPacket(server_address_, client_address_,
                                          connection_id);
  }

  QuicEncryptedPacket* ConstructEncryptedPacket(
      QuicConnectionId connection_id,
      QuicPacketNumber packet_number) {
    return net::test::ConstructEncryptedPacket(connection_id, false, false,
                                               packet_number, "data");
  }

  NiceMock<MockFakeTimeEpollServer> epoll_server_;
  QuicEpollConnectionHelper helper_;
  QuicEpollAlarmFactory alarm_factory_;
  StrictMock<MockPacketWriter> writer_;
  StrictMock<MockQuicSessionVisitor> visitor_;
  QuicTimeWaitListManager time_wait_list_manager_;
  QuicConnectionId connection_id_;
  QuicSocketAddress server_address_;
  QuicSocketAddress client_address_;
  bool writer_is_blocked_;
};

bool ValidPublicResetPacketPredicate(
    QuicConnectionId expected_connection_id,
    const testing::tuple<const char*, int>& packet_buffer) {
  FramerVisitorCapturingPublicReset visitor;
  QuicFramer framer(AllSupportedVersions(), QuicTime::Zero(),
                    Perspective::IS_CLIENT);
  framer.set_visitor(&visitor);
  QuicEncryptedPacket encrypted(testing::get<0>(packet_buffer),
                                testing::get<1>(packet_buffer));
  framer.ProcessPacket(encrypted);
  QuicPublicResetPacket packet = visitor.public_reset_packet();
  bool public_reset_is_valid =
      expected_connection_id == packet.connection_id &&
      TestPeerIPAddress() == packet.client_address.host() &&
      kTestPort == packet.client_address.port();

  QuicIetfStatelessResetPacket stateless_reset =
      visitor.stateless_reset_packet();
  bool stateless_reset_is_valid =
      expected_connection_id == stateless_reset.header.connection_id &&
      stateless_reset.stateless_reset_token == kTestStatelessResetToken;

  return public_reset_is_valid || stateless_reset_is_valid;
}

Matcher<const testing::tuple<const char*, int>> PublicResetPacketEq(
    QuicConnectionId connection_id) {
  return Truly(
      [connection_id](const testing::tuple<const char*, int> packet_buffer) {
        return ValidPublicResetPacketPredicate(connection_id, packet_buffer);
      });
}

TEST_F(QuicTimeWaitListManagerTest, CheckConnectionIdInTimeWait) {
  EXPECT_FALSE(IsConnectionIdInTimeWait(connection_id_));
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id_));
  AddConnectionId(connection_id_);
  EXPECT_EQ(1u, time_wait_list_manager_.num_connections());
  EXPECT_TRUE(IsConnectionIdInTimeWait(connection_id_));
}

TEST_F(QuicTimeWaitListManagerTest, CheckStatelessConnectionIdInTimeWait) {
  EXPECT_FALSE(IsConnectionIdInTimeWait(connection_id_));
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id_));
  AddStatelessConnectionId(connection_id_);
  EXPECT_EQ(1u, time_wait_list_manager_.num_connections());
  EXPECT_TRUE(IsConnectionIdInTimeWait(connection_id_));
}

TEST_F(QuicTimeWaitListManagerTest, SendVersionNegotiationPacket) {
  std::unique_ptr<QuicEncryptedPacket> packet(
      QuicFramer::BuildVersionNegotiationPacket(connection_id_, false,
                                                AllSupportedVersions()));
  EXPECT_CALL(writer_, WritePacket(_, packet->length(), server_address_.host(),
                                   client_address_, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));

  time_wait_list_manager_.SendVersionNegotiationPacket(
      connection_id_, false, AllSupportedVersions(), server_address_,
      client_address_);
  EXPECT_EQ(0u, time_wait_list_manager_.num_connections());
}

TEST_F(QuicTimeWaitListManagerTest, SendConnectionClose) {
  const size_t kConnectionCloseLength = 100;
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id_));
  std::vector<std::unique_ptr<QuicEncryptedPacket>> termination_packets;
  termination_packets.push_back(
      std::unique_ptr<QuicEncryptedPacket>(new QuicEncryptedPacket(
          new char[kConnectionCloseLength], kConnectionCloseLength, true)));
  AddConnectionId(connection_id_, QuicVersionMax(),
                  /*connection_rejected_statelessly=*/false,
                  &termination_packets);
  EXPECT_CALL(writer_, WritePacket(_, kConnectionCloseLength,
                                   server_address_.host(), client_address_, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));

  ProcessPacket(connection_id_);
}

TEST_F(QuicTimeWaitListManagerTest, SendTwoConnectionCloses) {
  const size_t kConnectionCloseLength = 100;
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id_));
  std::vector<std::unique_ptr<QuicEncryptedPacket>> termination_packets;
  termination_packets.push_back(
      std::unique_ptr<QuicEncryptedPacket>(new QuicEncryptedPacket(
          new char[kConnectionCloseLength], kConnectionCloseLength, true)));
  termination_packets.push_back(
      std::unique_ptr<QuicEncryptedPacket>(new QuicEncryptedPacket(
          new char[kConnectionCloseLength], kConnectionCloseLength, true)));
  AddConnectionId(connection_id_, QuicVersionMax(),
                  /*connection_rejected_statelessly=*/false,
                  &termination_packets);
  EXPECT_CALL(writer_, WritePacket(_, kConnectionCloseLength,
                                   server_address_.host(), client_address_, _))
      .Times(2)
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));

  ProcessPacket(connection_id_);
}

TEST_F(QuicTimeWaitListManagerTest, SendPublicReset) {
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id_));
  AddConnectionId(connection_id_);
  EXPECT_CALL(writer_,
              WritePacket(_, _, server_address_.host(), client_address_, _))
      .With(Args<0, 1>(PublicResetPacketEq(connection_id_)))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 0)));

  ProcessPacket(connection_id_);
}

TEST_F(QuicTimeWaitListManagerTest, SendPublicResetWithExponentialBackOff) {
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id_));
  AddConnectionId(connection_id_);
  EXPECT_EQ(1u, time_wait_list_manager_.num_connections());
  for (int packet_number = 1; packet_number < 101; ++packet_number) {
    if ((packet_number & (packet_number - 1)) == 0) {
      EXPECT_CALL(writer_, WritePacket(_, _, _, _, _))
          .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));
    }
    ProcessPacket(connection_id_);
    // Send public reset with exponential back off.
    if ((packet_number & (packet_number - 1)) == 0) {
      EXPECT_TRUE(QuicTimeWaitListManagerPeer::ShouldSendResponse(
          &time_wait_list_manager_, packet_number));
    } else {
      EXPECT_FALSE(QuicTimeWaitListManagerPeer::ShouldSendResponse(
          &time_wait_list_manager_, packet_number));
    }
  }
}

TEST_F(QuicTimeWaitListManagerTest, NoPublicResetForStatelessConnections) {
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id_));
  AddStatelessConnectionId(connection_id_);

  EXPECT_CALL(writer_,
              WritePacket(_, _, server_address_.host(), client_address_, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));

  ProcessPacket(connection_id_);
}

TEST_F(QuicTimeWaitListManagerTest, CleanUpOldConnectionIds) {
  const size_t kConnectionIdCount = 100;
  const size_t kOldConnectionIdCount = 31;

  // Add connection_ids such that their expiry time is time_wait_period_.
  epoll_server_.set_now_in_usec(0);
  for (size_t connection_id = 1; connection_id <= kOldConnectionIdCount;
       ++connection_id) {
    EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id));
    AddConnectionId(connection_id);
  }
  EXPECT_EQ(kOldConnectionIdCount, time_wait_list_manager_.num_connections());

  // Add remaining connection_ids such that their add time is
  // 2 * time_wait_period_.
  const QuicTime::Delta time_wait_period =
      QuicTimeWaitListManagerPeer::time_wait_period(&time_wait_list_manager_);
  epoll_server_.set_now_in_usec(time_wait_period.ToMicroseconds());
  for (size_t connection_id = kOldConnectionIdCount + 1;
       connection_id <= kConnectionIdCount; ++connection_id) {
    EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id));
    AddConnectionId(connection_id);
  }
  EXPECT_EQ(kConnectionIdCount, time_wait_list_manager_.num_connections());

  QuicTime::Delta offset = QuicTime::Delta::FromMicroseconds(39);
  // Now set the current time as time_wait_period + offset usecs.
  epoll_server_.set_now_in_usec((time_wait_period + offset).ToMicroseconds());
  // After all the old connection_ids are cleaned up, check the next alarm
  // interval.
  int64_t next_alarm_time = epoll_server_.ApproximateNowInUsec() +
                            (time_wait_period - offset).ToMicroseconds();
  EXPECT_CALL(epoll_server_, RegisterAlarm(next_alarm_time, _));

  time_wait_list_manager_.CleanUpOldConnectionIds();
  for (size_t connection_id = 1; connection_id <= kConnectionIdCount;
       ++connection_id) {
    EXPECT_EQ(connection_id > kOldConnectionIdCount,
              IsConnectionIdInTimeWait(connection_id))
        << "kOldConnectionIdCount: " << kOldConnectionIdCount
        << " connection_id: " << connection_id;
  }
  EXPECT_EQ(kConnectionIdCount - kOldConnectionIdCount,
            time_wait_list_manager_.num_connections());
}

TEST_F(QuicTimeWaitListManagerTest, SendQueuedPackets) {
  QuicConnectionId connection_id = 1;
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id));
  AddConnectionId(connection_id);
  QuicPacketNumber packet_number = 234;
  std::unique_ptr<QuicEncryptedPacket> packet(
      ConstructEncryptedPacket(connection_id, packet_number));
  // Let first write through.
  EXPECT_CALL(writer_,
              WritePacket(_, _, server_address_.host(), client_address_, _))
      .With(Args<0, 1>(PublicResetPacketEq(connection_id)))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, packet->length())));
  ProcessPacket(connection_id);

  // write block for the next packet.
  EXPECT_CALL(writer_,
              WritePacket(_, _, server_address_.host(), client_address_, _))
      .With(Args<0, 1>(PublicResetPacketEq(connection_id)))
      .WillOnce(DoAll(Assign(&writer_is_blocked_, true),
                      Return(WriteResult(WRITE_STATUS_BLOCKED, EAGAIN))));
  EXPECT_CALL(visitor_, OnWriteBlocked(&time_wait_list_manager_));
  ProcessPacket(connection_id);
  // 3rd packet. No public reset should be sent;
  ProcessPacket(connection_id);

  // write packet should not be called since we are write blocked but the
  // should be queued.
  QuicConnectionId other_connection_id = 2;
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(other_connection_id));
  AddConnectionId(other_connection_id);
  QuicPacketNumber other_packet_number = 23423;
  std::unique_ptr<QuicEncryptedPacket> other_packet(
      ConstructEncryptedPacket(other_connection_id, other_packet_number));
  EXPECT_CALL(writer_, WritePacket(_, _, _, _, _)).Times(0);
  EXPECT_CALL(visitor_, OnWriteBlocked(&time_wait_list_manager_));
  ProcessPacket(other_connection_id);
  EXPECT_EQ(2u, time_wait_list_manager_.num_connections());

  // Now expect all the write blocked public reset packets to be sent again.
  writer_is_blocked_ = false;
  EXPECT_CALL(writer_,
              WritePacket(_, _, server_address_.host(), client_address_, _))
      .With(Args<0, 1>(PublicResetPacketEq(connection_id)))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, packet->length())));
  EXPECT_CALL(writer_,
              WritePacket(_, _, server_address_.host(), client_address_, _))
      .With(Args<0, 1>(PublicResetPacketEq(other_connection_id)))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, other_packet->length())));
  time_wait_list_manager_.OnBlockedWriterCanWrite();
}

TEST_F(QuicTimeWaitListManagerTest, GetQuicVersionFromMap) {
  const int kConnectionId1 = 123;
  const int kConnectionId2 = 456;
  const int kConnectionId3 = 789;

  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(kConnectionId1));
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(kConnectionId2));
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(kConnectionId3));
  AddConnectionId(kConnectionId1, QuicVersionMin(),
                  /*connection_rejected_statelessly=*/false, nullptr);
  AddConnectionId(kConnectionId2, QuicVersionMax(),
                  /*connection_rejected_statelessly=*/false, nullptr);
  AddConnectionId(kConnectionId3, QuicVersionMax(),
                  /*connection_rejected_statelessly=*/false, nullptr);

  EXPECT_EQ(QuicTransportVersionMin(),
            QuicTimeWaitListManagerPeer::GetQuicVersionFromConnectionId(
                &time_wait_list_manager_, kConnectionId1));
  EXPECT_EQ(QuicTransportVersionMax(),
            QuicTimeWaitListManagerPeer::GetQuicVersionFromConnectionId(
                &time_wait_list_manager_, kConnectionId2));
  EXPECT_EQ(QuicTransportVersionMax(),
            QuicTimeWaitListManagerPeer::GetQuicVersionFromConnectionId(
                &time_wait_list_manager_, kConnectionId3));
}

TEST_F(QuicTimeWaitListManagerTest, AddConnectionIdTwice) {
  // Add connection_ids such that their expiry time is time_wait_period_.
  epoll_server_.set_now_in_usec(0);
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id_));
  AddConnectionId(connection_id_);
  EXPECT_TRUE(IsConnectionIdInTimeWait(connection_id_));
  const size_t kConnectionCloseLength = 100;
  std::vector<std::unique_ptr<QuicEncryptedPacket>> termination_packets;
  termination_packets.push_back(
      std::unique_ptr<QuicEncryptedPacket>(new QuicEncryptedPacket(
          new char[kConnectionCloseLength], kConnectionCloseLength, true)));
  AddConnectionId(connection_id_, QuicVersionMax(),
                  /*connection_rejected_statelessly=*/false,
                  &termination_packets);
  EXPECT_TRUE(IsConnectionIdInTimeWait(connection_id_));
  EXPECT_EQ(1u, time_wait_list_manager_.num_connections());

  EXPECT_CALL(writer_, WritePacket(_, kConnectionCloseLength,
                                   server_address_.host(), client_address_, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));

  ProcessPacket(connection_id_);

  const QuicTime::Delta time_wait_period =
      QuicTimeWaitListManagerPeer::time_wait_period(&time_wait_list_manager_);

  QuicTime::Delta offset = QuicTime::Delta::FromMicroseconds(39);
  // Now set the current time as time_wait_period + offset usecs.
  epoll_server_.set_now_in_usec((time_wait_period + offset).ToMicroseconds());
  // After the connection_ids are cleaned up, check the next alarm interval.
  int64_t next_alarm_time =
      epoll_server_.ApproximateNowInUsec() + time_wait_period.ToMicroseconds();

  EXPECT_CALL(epoll_server_, RegisterAlarm(next_alarm_time, _));
  time_wait_list_manager_.CleanUpOldConnectionIds();
  EXPECT_FALSE(IsConnectionIdInTimeWait(connection_id_));
  EXPECT_EQ(0u, time_wait_list_manager_.num_connections());
}

TEST_F(QuicTimeWaitListManagerTest, ConnectionIdsOrderedByTime) {
  // Simple randomization: the values of connection_ids are randomly swapped.
  // If the container is broken, the test will be 50% flaky.
  const QuicConnectionId connection_id1 =
      QuicRandom::GetInstance()->RandUint64() % 2;
  const QuicConnectionId connection_id2 = 1 - connection_id1;

  // 1 will hash lower than 2, but we add it later. They should come out in the
  // add order, not hash order.
  epoll_server_.set_now_in_usec(0);
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id1));
  AddConnectionId(connection_id1);
  epoll_server_.set_now_in_usec(10);
  EXPECT_CALL(visitor_, OnConnectionAddedToTimeWaitList(connection_id2));
  AddConnectionId(connection_id2);
  EXPECT_EQ(2u, time_wait_list_manager_.num_connections());

  const QuicTime::Delta time_wait_period =
      QuicTimeWaitListManagerPeer::time_wait_period(&time_wait_list_manager_);
  epoll_server_.set_now_in_usec(time_wait_period.ToMicroseconds() + 1);

  EXPECT_CALL(epoll_server_, RegisterAlarm(_, _));

  time_wait_list_manager_.CleanUpOldConnectionIds();
  EXPECT_FALSE(IsConnectionIdInTimeWait(connection_id1));
  EXPECT_TRUE(IsConnectionIdInTimeWait(connection_id2));
  EXPECT_EQ(1u, time_wait_list_manager_.num_connections());
}

TEST_F(QuicTimeWaitListManagerTest, MaxConnectionsTest) {
  // Basically, shut off time-based eviction.
  FLAGS_quic_time_wait_list_seconds = 10000000000;
  FLAGS_quic_time_wait_list_max_connections = 5;

  QuicConnectionId current_connection_id = 0;
  // Add exactly the maximum number of connections
  for (int64_t i = 0; i < FLAGS_quic_time_wait_list_max_connections; ++i) {
    ++current_connection_id;
    EXPECT_FALSE(IsConnectionIdInTimeWait(current_connection_id));
    EXPECT_CALL(visitor_,
                OnConnectionAddedToTimeWaitList(current_connection_id));
    AddConnectionId(current_connection_id);
    EXPECT_EQ(current_connection_id, time_wait_list_manager_.num_connections());
    EXPECT_TRUE(IsConnectionIdInTimeWait(current_connection_id));
  }

  // Now keep adding.  Since we're already at the max, every new connection-id
  // will evict the oldest one.
  for (int64_t i = 0; i < FLAGS_quic_time_wait_list_max_connections; ++i) {
    ++current_connection_id;
    const QuicConnectionId id_to_evict =
        current_connection_id - FLAGS_quic_time_wait_list_max_connections;
    EXPECT_TRUE(IsConnectionIdInTimeWait(id_to_evict));
    EXPECT_FALSE(IsConnectionIdInTimeWait(current_connection_id));
    EXPECT_CALL(visitor_,
                OnConnectionAddedToTimeWaitList(current_connection_id));
    AddConnectionId(current_connection_id);
    EXPECT_EQ(static_cast<size_t>(FLAGS_quic_time_wait_list_max_connections),
              time_wait_list_manager_.num_connections());
    EXPECT_FALSE(IsConnectionIdInTimeWait(id_to_evict));
    EXPECT_TRUE(IsConnectionIdInTimeWait(current_connection_id));
  }
}

}  // namespace
}  // namespace test
}  // namespace net
