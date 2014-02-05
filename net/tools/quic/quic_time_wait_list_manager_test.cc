// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_time_wait_list_manager.h"

#include <errno.h>

#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_data_reader.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_packet_writer.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/quic/test_tools/mock_epoll_server.h"
#include "net/tools/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::FramerVisitorCapturingPublicReset;
using testing::_;
using testing::Args;
using testing::Assign;
using testing::DoAll;
using testing::Matcher;
using testing::MatcherInterface;
using testing::NiceMock;
using testing::Return;
using testing::ReturnPointee;
using testing::SetArgPointee;
using testing::StrictMock;
using testing::Truly;

namespace net {
namespace tools {
namespace test {

class QuicTimeWaitListManagerPeer {
 public:
  static bool ShouldSendResponse(QuicTimeWaitListManager* manager,
                                 int received_packet_count) {
    return manager->ShouldSendResponse(received_packet_count);
  }

  static QuicTime::Delta time_wait_period(QuicTimeWaitListManager* manager) {
    return manager->kTimeWaitPeriod_;
  }

  static QuicVersion GetQuicVersionFromGuid(QuicTimeWaitListManager* manager,
                                            QuicGuid guid) {
    return manager->GetQuicVersionFromGuid(guid);
  }
};

namespace {

class MockFakeTimeEpollServer : public FakeTimeEpollServer {
 public:
  MOCK_METHOD2(RegisterAlarm, void(int64 timeout_in_us,
                                   EpollAlarmCallbackInterface* alarm));
};

class QuicTimeWaitListManagerTest : public testing::Test {
 protected:
  QuicTimeWaitListManagerTest()
      : time_wait_list_manager_(&writer_, &visitor_,
                                &epoll_server_, QuicSupportedVersions()),
        framer_(QuicSupportedVersions(), QuicTime::Zero(), true),
        guid_(45),
        client_address_(net::test::TestPeerIPAddress(), kTestPort),
        writer_is_blocked_(false) {}

  virtual ~QuicTimeWaitListManagerTest() {}

  virtual void SetUp() {
    EXPECT_CALL(writer_, IsWriteBlocked())
        .WillRepeatedly(ReturnPointee(&writer_is_blocked_));
    EXPECT_CALL(writer_, IsWriteBlockedDataBuffered())
        .WillRepeatedly(Return(false));
  }

  void AddGuid(QuicGuid guid) {
    AddGuid(guid, net::test::QuicVersionMax(), NULL);
  }

  void AddGuid(QuicGuid guid,
               QuicVersion version,
               QuicEncryptedPacket* packet) {
    time_wait_list_manager_.AddGuidToTimeWait(guid, version, packet);
  }

  bool IsGuidInTimeWait(QuicGuid guid) {
    return time_wait_list_manager_.IsGuidInTimeWait(guid);
  }

  void ProcessPacket(QuicGuid guid, QuicPacketSequenceNumber sequence_number) {
    time_wait_list_manager_.ProcessPacket(server_address_,
                                          client_address_,
                                          guid,
                                          sequence_number);
  }

  QuicEncryptedPacket* ConstructEncryptedPacket(
      EncryptionLevel level,
      QuicGuid guid,
      QuicPacketSequenceNumber sequence_number) {
    QuicPacketHeader header;
    header.public_header.guid = guid;
    header.public_header.guid_length = PACKET_8BYTE_GUID;
    header.public_header.version_flag = false;
    header.public_header.reset_flag = false;
    header.public_header.sequence_number_length = PACKET_6BYTE_SEQUENCE_NUMBER;
    header.packet_sequence_number = sequence_number;
    header.entropy_flag = false;
    header.entropy_hash = 0;
    header.fec_flag = false;
    header.is_in_fec_group = NOT_IN_FEC_GROUP;
    header.fec_group = 0;
    QuicStreamFrame stream_frame(1, false, 0, MakeIOVector("data"));
    QuicFrame frame(&stream_frame);
    QuicFrames frames;
    frames.push_back(frame);
    scoped_ptr<QuicPacket> packet(
        framer_.BuildUnsizedDataPacket(header, frames).packet);
    EXPECT_TRUE(packet != NULL);
    QuicEncryptedPacket* encrypted = framer_.EncryptPacket(ENCRYPTION_NONE,
                                                           sequence_number,
                                                           *packet);
    EXPECT_TRUE(encrypted != NULL);
    return encrypted;
  }

  NiceMock<MockFakeTimeEpollServer> epoll_server_;
  StrictMock<MockPacketWriter> writer_;
  StrictMock<MockQuicServerSessionVisitor> visitor_;
  QuicTimeWaitListManager time_wait_list_manager_;
  QuicFramer framer_;
  QuicGuid guid_;
  IPEndPoint server_address_;
  IPEndPoint client_address_;
  bool writer_is_blocked_;
};

class ValidatePublicResetPacketPredicate
    : public MatcherInterface<const std::tr1::tuple<const char*, int> > {
 public:
  explicit ValidatePublicResetPacketPredicate(QuicGuid guid,
                                              QuicPacketSequenceNumber number)
      : guid_(guid), sequence_number_(number) {
  }

  virtual bool MatchAndExplain(
      const std::tr1::tuple<const char*, int> packet_buffer,
      testing::MatchResultListener* /* listener */) const {
    FramerVisitorCapturingPublicReset visitor;
    QuicFramer framer(QuicSupportedVersions(),
                      QuicTime::Zero(),
                      false);
    framer.set_visitor(&visitor);
    QuicEncryptedPacket encrypted(std::tr1::get<0>(packet_buffer),
                                  std::tr1::get<1>(packet_buffer));
    framer.ProcessPacket(encrypted);
    QuicPublicResetPacket packet = visitor.public_reset_packet();
    return guid_ == packet.public_header.guid &&
        packet.public_header.reset_flag && !packet.public_header.version_flag &&
        sequence_number_ == packet.rejected_sequence_number &&
        net::test::TestPeerIPAddress() == packet.client_address.address() &&
        kTestPort == packet.client_address.port();
  }

  virtual void DescribeTo(::std::ostream* os) const { }

  virtual void DescribeNegationTo(::std::ostream* os) const { }

 private:
  QuicGuid guid_;
  QuicPacketSequenceNumber sequence_number_;
};


Matcher<const std::tr1::tuple<const char*, int> > PublicResetPacketEq(
    QuicGuid guid,
    QuicPacketSequenceNumber sequence_number) {
  return MakeMatcher(new ValidatePublicResetPacketPredicate(guid,
                                                            sequence_number));
}

TEST_F(QuicTimeWaitListManagerTest, CheckGuidInTimeWait) {
  EXPECT_FALSE(IsGuidInTimeWait(guid_));
  AddGuid(guid_);
  EXPECT_TRUE(IsGuidInTimeWait(guid_));
}

TEST_F(QuicTimeWaitListManagerTest, SendConnectionClose) {
  size_t kConnectionCloseLength = 100;
  AddGuid(guid_,
          net::test::QuicVersionMax(),
          new QuicEncryptedPacket(
              new char[kConnectionCloseLength], kConnectionCloseLength, true));
  const int kRandomSequenceNumber = 1;
  EXPECT_CALL(writer_, WritePacket(_, kConnectionCloseLength,
                                   server_address_.address(),
                                   client_address_))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));

  ProcessPacket(guid_, kRandomSequenceNumber);
}

TEST_F(QuicTimeWaitListManagerTest, SendPublicReset) {
  AddGuid(guid_);
  const int kRandomSequenceNumber = 1;
  EXPECT_CALL(writer_, WritePacket(_, _,
                                   server_address_.address(),
                                   client_address_))
      .With(Args<0, 1>(PublicResetPacketEq(guid_,
                                           kRandomSequenceNumber)))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 0)));

  ProcessPacket(guid_, kRandomSequenceNumber);
}

TEST_F(QuicTimeWaitListManagerTest, SendPublicResetWithExponentialBackOff) {
  AddGuid(guid_);
  for (int sequence_number = 1; sequence_number < 101; ++sequence_number) {
    if ((sequence_number & (sequence_number - 1)) == 0) {
      EXPECT_CALL(writer_, WritePacket(_, _, _, _))
          .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));
    }
    ProcessPacket(guid_, sequence_number);
    // Send public reset with exponential back off.
    if ((sequence_number & (sequence_number - 1)) == 0) {
      EXPECT_TRUE(QuicTimeWaitListManagerPeer::ShouldSendResponse(
                      &time_wait_list_manager_, sequence_number));
    } else {
      EXPECT_FALSE(QuicTimeWaitListManagerPeer::ShouldSendResponse(
                       &time_wait_list_manager_, sequence_number));
    }
  }
}

TEST_F(QuicTimeWaitListManagerTest, CleanUpOldGuids) {
  const int kGuidCount = 100;
  const int kOldGuidCount = 31;

  // Add guids such that their expiry time is kTimeWaitPeriod_.
  epoll_server_.set_now_in_usec(0);
  for (int guid = 1; guid <= kOldGuidCount; ++guid) {
    AddGuid(guid);
  }

  // Add remaining guids such that their add time is 2 * kTimeWaitPeriod.
  const QuicTime::Delta time_wait_period =
      QuicTimeWaitListManagerPeer::time_wait_period(&time_wait_list_manager_);
  epoll_server_.set_now_in_usec(time_wait_period.ToMicroseconds());
  for (int guid = kOldGuidCount + 1; guid <= kGuidCount; ++guid) {
    AddGuid(guid);
  }

  QuicTime::Delta offset = QuicTime::Delta::FromMicroseconds(39);
  // Now set the current time as time_wait_period + offset usecs.
  epoll_server_.set_now_in_usec(time_wait_period.Add(offset).ToMicroseconds());
  // After all the old guids are cleaned up, check the next alarm interval.
  int64 next_alarm_time = epoll_server_.ApproximateNowInUsec() +
      time_wait_period.Subtract(offset).ToMicroseconds();
  EXPECT_CALL(epoll_server_, RegisterAlarm(next_alarm_time, _));

  time_wait_list_manager_.CleanUpOldGuids();
  for (int guid = 1; guid <= kGuidCount; ++guid) {
    EXPECT_EQ(guid > kOldGuidCount, IsGuidInTimeWait(guid))
        << "kOldGuidCount: " << kOldGuidCount
        << " guid: " <<  guid;
  }
}

TEST_F(QuicTimeWaitListManagerTest, SendQueuedPackets) {
  QuicGuid guid = 1;
  AddGuid(guid);
  QuicPacketSequenceNumber sequence_number = 234;
  scoped_ptr<QuicEncryptedPacket> packet(
      ConstructEncryptedPacket(ENCRYPTION_NONE, guid, sequence_number));
  // Let first write through.
  EXPECT_CALL(writer_, WritePacket(_, _,
                                   server_address_.address(),
                                   client_address_))
      .With(Args<0, 1>(PublicResetPacketEq(guid,
                                           sequence_number)))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, packet->length())));
  ProcessPacket(guid, sequence_number);

  // write block for the next packet.
  EXPECT_CALL(writer_, WritePacket(_, _,
                                   server_address_.address(),
                                   client_address_))
      .With(Args<0, 1>(PublicResetPacketEq(guid,
                                           sequence_number)))
      .WillOnce(DoAll(
          Assign(&writer_is_blocked_, true),
          Return(WriteResult(WRITE_STATUS_BLOCKED, EAGAIN))));
  EXPECT_CALL(visitor_, OnWriteBlocked(&time_wait_list_manager_));
  ProcessPacket(guid, sequence_number);
  // 3rd packet. No public reset should be sent;
  ProcessPacket(guid, sequence_number);

  // write packet should not be called since we are write blocked but the
  // should be queued.
  QuicGuid other_guid = 2;
  AddGuid(other_guid);
  QuicPacketSequenceNumber other_sequence_number = 23423;
  scoped_ptr<QuicEncryptedPacket> other_packet(
      ConstructEncryptedPacket(
          ENCRYPTION_NONE, other_guid, other_sequence_number));
  EXPECT_CALL(writer_, WritePacket(_, _, _, _))
      .Times(0);
  EXPECT_CALL(visitor_, OnWriteBlocked(&time_wait_list_manager_));
  ProcessPacket(other_guid, other_sequence_number);

  // Now expect all the write blocked public reset packets to be sent again.
  writer_is_blocked_ = false;
  EXPECT_CALL(writer_, WritePacket(_, _,
                                   server_address_.address(),
                                   client_address_))
      .With(Args<0, 1>(PublicResetPacketEq(guid,
                                           sequence_number)))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, packet->length())));
  EXPECT_CALL(writer_, WritePacket(_, _,
                                   server_address_.address(),
                                   client_address_))
      .With(Args<0, 1>(PublicResetPacketEq(other_guid,
                                           other_sequence_number)))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK,
                                   other_packet->length())));
  time_wait_list_manager_.OnCanWrite();
}

TEST_F(QuicTimeWaitListManagerTest, GetQuicVersionFromMap) {
  const int kGuid1 = 123;
  const int kGuid2 = 456;
  const int kGuid3 = 789;

  AddGuid(kGuid1, net::test::QuicVersionMin(), NULL);
  AddGuid(kGuid2, net::test::QuicVersionMax(), NULL);
  AddGuid(kGuid3, net::test::QuicVersionMax(), NULL);

  EXPECT_EQ(net::test::QuicVersionMin(),
            QuicTimeWaitListManagerPeer::GetQuicVersionFromGuid(
                &time_wait_list_manager_, kGuid1));
  EXPECT_EQ(net::test::QuicVersionMax(),
            QuicTimeWaitListManagerPeer::GetQuicVersionFromGuid(
                &time_wait_list_manager_, kGuid2));
  EXPECT_EQ(net::test::QuicVersionMax(),
            QuicTimeWaitListManagerPeer::GetQuicVersionFromGuid(
                &time_wait_list_manager_, kGuid3));
}

TEST_F(QuicTimeWaitListManagerTest, AddGuidTwice) {
  // Add guids such that their expiry time is kTimeWaitPeriod_.
  epoll_server_.set_now_in_usec(0);
  AddGuid(guid_);
  EXPECT_TRUE(IsGuidInTimeWait(guid_));
  size_t kConnectionCloseLength = 100;
  AddGuid(guid_,
          net::test::QuicVersionMax(),
          new QuicEncryptedPacket(
              new char[kConnectionCloseLength], kConnectionCloseLength, true));
  EXPECT_TRUE(IsGuidInTimeWait(guid_));

  EXPECT_CALL(writer_, WritePacket(_,
                                   kConnectionCloseLength,
                                   server_address_.address(),
                                   client_address_))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 1)));

  const int kRandomSequenceNumber = 1;
  ProcessPacket(guid_, kRandomSequenceNumber);

  const QuicTime::Delta time_wait_period =
      QuicTimeWaitListManagerPeer::time_wait_period(&time_wait_list_manager_);

  QuicTime::Delta offset = QuicTime::Delta::FromMicroseconds(39);
  // Now set the current time as time_wait_period + offset usecs.
  epoll_server_.set_now_in_usec(time_wait_period.Add(offset).ToMicroseconds());
  // After the guids are cleaned up, check the next alarm interval.
  int64 next_alarm_time = epoll_server_.ApproximateNowInUsec() +
      time_wait_period.ToMicroseconds();

  EXPECT_CALL(epoll_server_, RegisterAlarm(next_alarm_time, _));
  time_wait_list_manager_.CleanUpOldGuids();
  EXPECT_FALSE(IsGuidInTimeWait(guid_));
}

TEST_F(QuicTimeWaitListManagerTest, GuidsOrderedByTime) {
  // Simple randomization: the values of guids are swapped based on the current
  // seconds on the clock. If the container is broken, the test will be 50%
  // flaky.
  int odd_second = static_cast<int>(epoll_server_.ApproximateNowInUsec()) % 2;
  EXPECT_TRUE(odd_second == 0 || odd_second == 1);
  const QuicGuid kGuid1 = odd_second;
  const QuicGuid kGuid2 = 1 - odd_second;

  // 1 will hash lower than 2, but we add it later. They should come out in the
  // add order, not hash order.
  epoll_server_.set_now_in_usec(0);
  AddGuid(kGuid1);
  epoll_server_.set_now_in_usec(10);
  AddGuid(kGuid2);

  const QuicTime::Delta time_wait_period =
      QuicTimeWaitListManagerPeer::time_wait_period(&time_wait_list_manager_);
  epoll_server_.set_now_in_usec(time_wait_period.ToMicroseconds() + 1);

  EXPECT_CALL(epoll_server_, RegisterAlarm(_, _));

  time_wait_list_manager_.CleanUpOldGuids();
  EXPECT_FALSE(IsGuidInTimeWait(kGuid1));
  EXPECT_TRUE(IsGuidInTimeWait(kGuid2));
}
}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
