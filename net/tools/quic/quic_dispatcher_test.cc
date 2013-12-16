// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_dispatcher.h"

#include <string>

#include "base/strings/string_piece.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/quic_crypto_server_config.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_crypto_stream.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/quic/quic_time_wait_list_manager.h"
#include "net/tools/quic/test_tools/quic_dispatcher_peer.h"
#include "net/tools/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using net::EpollServer;
using net::test::MockSession;
using net::tools::test::MockConnection;
using std::make_pair;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::InSequence;
using testing::Return;
using testing::WithoutArgs;

namespace net {
namespace tools {
namespace test {
namespace {

class TestDispatcher : public QuicDispatcher {
 public:
  explicit TestDispatcher(const QuicConfig& config,
                          const QuicCryptoServerConfig& crypto_config,
                          EpollServer* eps)
      : QuicDispatcher(config, crypto_config, QuicSupportedVersions(), 1, eps) {
  }

  MOCK_METHOD3(CreateQuicSession, QuicSession*(
      QuicGuid guid,
      const IPEndPoint& server_address,
      const IPEndPoint& client_address));
  using QuicDispatcher::write_blocked_list;
};

// A Connection class which unregisters the session from the dispatcher
// when sending connection close.
// It'd be slightly more realistic to do this from the Session but it would
// involve a lot more mocking.
class MockServerConnection : public MockConnection {
 public:
  MockServerConnection(QuicGuid guid,
                       QuicDispatcher* dispatcher)
      : MockConnection(guid, true),
        dispatcher_(dispatcher) {
  }
  void UnregisterOnConnectionClosed() {
    LOG(ERROR) << "Unregistering " << guid();
    dispatcher_->OnConnectionClosed(guid(), QUIC_NO_ERROR);
  }
 private:
  QuicDispatcher* dispatcher_;
};

QuicSession* CreateSession(QuicDispatcher* dispatcher,
                           QuicGuid guid,
                           const IPEndPoint& addr,
                           MockSession** session) {
  MockServerConnection* connection = new MockServerConnection(guid, dispatcher);
  *session = new MockSession(connection);
  ON_CALL(*connection, SendConnectionClose(_)).WillByDefault(
      WithoutArgs(Invoke(
          connection, &MockServerConnection::UnregisterOnConnectionClosed)));
  EXPECT_CALL(*reinterpret_cast<MockConnection*>((*session)->connection()),
              ProcessUdpPacket(_, addr, _));

  return *session;
}

class QuicDispatcherTest : public ::testing::Test {
 public:
  QuicDispatcherTest()
      : crypto_config_(QuicCryptoServerConfig::TESTING,
                       QuicRandom::GetInstance()),
        dispatcher_(config_, crypto_config_, &eps_),
        session1_(NULL),
        session2_(NULL) {
  }

  virtual ~QuicDispatcherTest() {}

  MockConnection* connection1() {
    return reinterpret_cast<MockConnection*>(session1_->connection());
  }

  MockConnection* connection2() {
    return reinterpret_cast<MockConnection*>(session2_->connection());
  }

  void ProcessPacket(IPEndPoint addr,
                     QuicGuid guid,
                     bool has_version_flag,
                     const string& data) {
    dispatcher_.ProcessPacket(
        IPEndPoint(), addr, guid, has_version_flag,
        QuicEncryptedPacket(data.data(), data.length()));
  }

  void ValidatePacket(const QuicEncryptedPacket& packet) {
    EXPECT_TRUE(packet.AsStringPiece().find(data_) != StringPiece::npos);
  }

  EpollServer eps_;
  QuicConfig config_;
  QuicCryptoServerConfig crypto_config_;
  TestDispatcher dispatcher_;
  MockSession* session1_;
  MockSession* session2_;
  string data_;
};

TEST_F(QuicDispatcherTest, ProcessPackets) {
  IPEndPoint addr(net::test::Loopback4(), 1);

  EXPECT_CALL(dispatcher_, CreateQuicSession(1, _, addr))
      .WillOnce(testing::Return(CreateSession(
          &dispatcher_, 1, addr, &session1_)));
  ProcessPacket(addr, 1, true, "foo");

  EXPECT_CALL(dispatcher_, CreateQuicSession(2, _, addr))
      .WillOnce(testing::Return(CreateSession(
                    &dispatcher_, 2, addr, &session2_)));
  ProcessPacket(addr, 2, true, "bar");

  data_ = "eep";
  EXPECT_CALL(*reinterpret_cast<MockConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _)).Times(1).
      WillOnce(testing::WithArgs<2>(Invoke(
          this, &QuicDispatcherTest::ValidatePacket)));
  ProcessPacket(addr, 1, false, "eep");
}

TEST_F(QuicDispatcherTest, Shutdown) {
  IPEndPoint addr(net::test::Loopback4(), 1);

  EXPECT_CALL(dispatcher_, CreateQuicSession(_, _, addr))
      .WillOnce(testing::Return(CreateSession(
                    &dispatcher_, 1, addr, &session1_)));

  ProcessPacket(addr, 1, true, "foo");

  EXPECT_CALL(*reinterpret_cast<MockConnection*>(session1_->connection()),
              SendConnectionClose(QUIC_PEER_GOING_AWAY));

  dispatcher_.Shutdown();
}

class MockTimeWaitListManager : public QuicTimeWaitListManager {
 public:
  MockTimeWaitListManager(QuicPacketWriter* writer,
                          EpollServer* eps)
      : QuicTimeWaitListManager(writer, eps, QuicSupportedVersions()) {
  }

  MOCK_METHOD4(ProcessPacket, void(const IPEndPoint& server_address,
                                   const IPEndPoint& client_address,
                                   QuicGuid guid,
                                   const QuicEncryptedPacket& packet));
};

TEST_F(QuicDispatcherTest, TimeWaitListManager) {
  MockTimeWaitListManager* time_wait_list_manager =
      new MockTimeWaitListManager(
          QuicDispatcherPeer::GetWriter(&dispatcher_), &eps_);
  // dispatcher takes the ownership of time_wait_list_manager.
  QuicDispatcherPeer::SetTimeWaitListManager(&dispatcher_,
                                             time_wait_list_manager);
  // Create a new session.
  IPEndPoint addr(net::test::Loopback4(), 1);
  QuicGuid guid = 1;
  EXPECT_CALL(dispatcher_, CreateQuicSession(guid, _, addr))
      .WillOnce(testing::Return(CreateSession(
                    &dispatcher_, guid, addr, &session1_)));
  ProcessPacket(addr, guid, true, "foo");

  // Close the connection by sending public reset packet.
  QuicPublicResetPacket packet;
  packet.public_header.guid = guid;
  packet.public_header.reset_flag = true;
  packet.public_header.version_flag = false;
  packet.rejected_sequence_number = 19191;
  packet.nonce_proof = 132232;
  scoped_ptr<QuicEncryptedPacket> encrypted(
      QuicFramer::BuildPublicResetPacket(packet));
  EXPECT_CALL(*session1_, OnConnectionClosed(QUIC_PUBLIC_RESET, true)).Times(1)
      .WillOnce(WithoutArgs(Invoke(
          reinterpret_cast<MockServerConnection*>(session1_->connection()),
          &MockServerConnection::UnregisterOnConnectionClosed)));
  EXPECT_CALL(*reinterpret_cast<MockConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(Invoke(
          reinterpret_cast<MockConnection*>(session1_->connection()),
          &MockConnection::ReallyProcessUdpPacket));
  dispatcher_.ProcessPacket(IPEndPoint(), addr, guid, true, *encrypted);
  EXPECT_TRUE(time_wait_list_manager->IsGuidInTimeWait(guid));

  // Dispatcher forwards subsequent packets for this guid to the time wait list
  // manager.
  EXPECT_CALL(*time_wait_list_manager, ProcessPacket(_, _, guid, _)).Times(1);
  ProcessPacket(addr, guid, true, "foo");
}

TEST_F(QuicDispatcherTest, StrayPacketToTimeWaitListManager) {
  MockTimeWaitListManager* time_wait_list_manager =
      new MockTimeWaitListManager(
          QuicDispatcherPeer::GetWriter(&dispatcher_), &eps_);
  // dispatcher takes the ownership of time_wait_list_manager.
  QuicDispatcherPeer::SetTimeWaitListManager(&dispatcher_,
                                             time_wait_list_manager);

  IPEndPoint addr(net::test::Loopback4(), 1);
  QuicGuid guid = 1;
  // Dispatcher forwards all packets for this guid to the time wait list
  // manager.
  EXPECT_CALL(dispatcher_, CreateQuicSession(_, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager, ProcessPacket(_, _, guid, _)).Times(1);
  string data = "foo";
  ProcessPacket(addr, guid, false, "foo");
}

class QuicWriteBlockedListTest : public QuicDispatcherTest {
 public:
  virtual void SetUp() {
    IPEndPoint addr(net::test::Loopback4(), 1);

    EXPECT_CALL(dispatcher_, CreateQuicSession(_, _, addr))
        .WillOnce(testing::Return(CreateSession(
                      &dispatcher_, 1, addr, &session1_)));
    ProcessPacket(addr, 1, true, "foo");

    EXPECT_CALL(dispatcher_, CreateQuicSession(_, _, addr))
        .WillOnce(testing::Return(CreateSession(
                      &dispatcher_, 2, addr, &session2_)));
    ProcessPacket(addr, 2, true, "bar");

    blocked_list_ = dispatcher_.write_blocked_list();
  }

  virtual void TearDown() {
    EXPECT_CALL(*connection1(), SendConnectionClose(QUIC_PEER_GOING_AWAY));
    EXPECT_CALL(*connection2(), SendConnectionClose(QUIC_PEER_GOING_AWAY));
    dispatcher_.Shutdown();
  }

  bool SetBlocked() {
    QuicDispatcherPeer::SetWriteBlocked(&dispatcher_);
    return true;
  }

 protected:
  QuicDispatcher::WriteBlockedList* blocked_list_;
};

TEST_F(QuicWriteBlockedListTest, BasicOnCanWrite) {
  // No OnCanWrite calls because no connections are blocked.
  dispatcher_.OnCanWrite();

  // Register connection 1 for events, and make sure it's nofitied.
  blocked_list_->insert(make_pair(connection1(), true));
  EXPECT_CALL(*connection1(), OnCanWrite());
  dispatcher_.OnCanWrite();

  // It should get only one notification.
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(0);
  EXPECT_FALSE(dispatcher_.OnCanWrite());
}

TEST_F(QuicWriteBlockedListTest, OnCanWriteOrder) {
  // Make sure we handle events in order.
  InSequence s;
  blocked_list_->insert(make_pair(connection1(), true));
  blocked_list_->insert(make_pair(connection2(), true));
  EXPECT_CALL(*connection1(), OnCanWrite());
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_.OnCanWrite();

  // Check the other ordering.
  blocked_list_->insert(make_pair(connection2(), true));
  blocked_list_->insert(make_pair(connection1(), true));
  EXPECT_CALL(*connection2(), OnCanWrite());
  EXPECT_CALL(*connection1(), OnCanWrite());
  dispatcher_.OnCanWrite();
}

TEST_F(QuicWriteBlockedListTest, OnCanWriteRemove) {
  // Add and remove one connction.
  blocked_list_->insert(make_pair(connection1(), true));
  blocked_list_->erase(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(0);
  dispatcher_.OnCanWrite();

  // Add and remove one connction and make sure it doesn't affect others.
  blocked_list_->insert(make_pair(connection1(), true));
  blocked_list_->insert(make_pair(connection2(), true));
  blocked_list_->erase(connection1());
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_.OnCanWrite();

  // Add it, remove it, and add it back and make sure things are OK.
  blocked_list_->insert(make_pair(connection1(), true));
  blocked_list_->erase(connection1());
  blocked_list_->insert(make_pair(connection1(), true));
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(1);
  dispatcher_.OnCanWrite();
}

TEST_F(QuicWriteBlockedListTest, DoubleAdd) {
  // Make sure a double add does not necessitate a double remove.
  blocked_list_->insert(make_pair(connection1(), true));
  blocked_list_->insert(make_pair(connection1(), true));
  blocked_list_->erase(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(0);
  dispatcher_.OnCanWrite();

  // Make sure a double add does not result in two OnCanWrite calls.
  blocked_list_->insert(make_pair(connection1(), true));
  blocked_list_->insert(make_pair(connection1(), true));
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(1);
  dispatcher_.OnCanWrite();
}

TEST_F(QuicWriteBlockedListTest, OnCanWriteHandleBlock) {
  // Finally make sure if we write block on a write call, we stop calling.
  InSequence s;
  blocked_list_->insert(make_pair(connection1(), true));
  blocked_list_->insert(make_pair(connection2(), true));
  EXPECT_CALL(*connection1(), OnCanWrite()).WillOnce(
      Invoke(this, &QuicWriteBlockedListTest::SetBlocked));
  EXPECT_CALL(*connection2(), OnCanWrite()).Times(0);
  dispatcher_.OnCanWrite();

  // And we'll resume where we left off when we get another call.
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_.OnCanWrite();
}

TEST_F(QuicWriteBlockedListTest, LimitedWrites) {
  // Make sure we call both writers.  The first will register for more writing
  // but should not be immediately called due to limits.
  InSequence s;
  blocked_list_->insert(make_pair(connection1(), true));
  blocked_list_->insert(make_pair(connection2(), true));
  EXPECT_CALL(*connection1(), OnCanWrite()).WillOnce(Return(true));
  EXPECT_CALL(*connection2(), OnCanWrite()).WillOnce(Return(false));
  dispatcher_.OnCanWrite();

  // Now call OnCanWrite again, and connection1 should get its second chance
  EXPECT_CALL(*connection1(), OnCanWrite());
  dispatcher_.OnCanWrite();
}

TEST_F(QuicWriteBlockedListTest, TestWriteLimits) {
  // Finally make sure if we write block on a write call, we stop calling.
  InSequence s;
  blocked_list_->insert(make_pair(connection1(), true));
  blocked_list_->insert(make_pair(connection2(), true));
  EXPECT_CALL(*connection1(), OnCanWrite()).WillOnce(
      Invoke(this, &QuicWriteBlockedListTest::SetBlocked));
  EXPECT_CALL(*connection2(), OnCanWrite()).Times(0);
  dispatcher_.OnCanWrite();

  // And we'll resume where we left off when we get another call.
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_.OnCanWrite();
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
