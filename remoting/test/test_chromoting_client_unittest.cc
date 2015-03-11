// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop/message_loop.h"
#include "remoting/protocol/fake_connection_to_host.h"
#include "remoting/test/test_chromoting_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTestUserName[] = "test_user@faux_address.com";
const char kAccessToken[] = "faux_access_token";
}

namespace remoting {
namespace test {

using testing::_;

// Provides base functionality for the TestChromotingClient Tests below.  This
// fixture also creates an IO MessageLoop for use by the TestChromotingClient.
// Overrides a subset of the RemoteConnectionObserver interface to track
// connection status changes for result verification.
class TestChromotingClientTest : public ::testing::Test,
                                 public RemoteConnectionObserver {
 public:
  TestChromotingClientTest();
  ~TestChromotingClientTest() override;

 protected:
  // testing::Test interface.
  void SetUp() override;
  void TearDown() override;

  // Used for result verification.
  bool is_connected_to_host_;
  protocol::ConnectionToHost::State connection_state_;
  protocol::ErrorCode error_code_;

  // Used for simulating different conditions for the TestChromotingClient.
  RemoteHostInfo remote_host_info_;
  FakeConnectionToHost* fake_connection_to_host_;

  scoped_ptr<TestChromotingClient> test_chromoting_client_;

 private:
  // RemoteConnectionObserver interface.
  void ConnectionStateChanged(protocol::ConnectionToHost::State state,
                              protocol::ErrorCode error_code) override;
  void ConnectionReady(bool ready) override;

  base::MessageLoopForIO message_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestChromotingClientTest);
};

TestChromotingClientTest::TestChromotingClientTest()
    : is_connected_to_host_(false),
      connection_state_(protocol::ConnectionToHost::INITIALIZING),
      error_code_(protocol::OK),
      fake_connection_to_host_(nullptr) {
}

TestChromotingClientTest::~TestChromotingClientTest() {
}

void TestChromotingClientTest::SetUp() {
  test_chromoting_client_.reset(new TestChromotingClient());
  test_chromoting_client_->AddRemoteConnectionObserver(this);

  // Pass ownership of the FakeConnectionToHost to the chromoting instance but
  // keep the ptr around so we can use it to simulate state changes.  It will
  // remain valid until |test_chromoting_client_| is destroyed.
  fake_connection_to_host_ = new FakeConnectionToHost();
  test_chromoting_client_->SetConnectionToHostForTests(
      make_scoped_ptr(fake_connection_to_host_));

  remote_host_info_.remote_host_status = kRemoteHostStatusReady;
}

void TestChromotingClientTest::TearDown() {
  test_chromoting_client_->RemoveRemoteConnectionObserver(this);
  fake_connection_to_host_ = nullptr;

  // The chromoting instance must be destroyed before the message loop.
  test_chromoting_client_.reset();

  // The LibjingleTransportFactory destroys the PortAllocator via a DeleteSoon
  // operation.  If we do not allow the message loop to run here, we run the
  // risk of the DeleteSoon task being dropped and incurring a memory leak.
  message_loop_.RunUntilIdle();
}

void TestChromotingClientTest::ConnectionStateChanged(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error_code) {
  connection_state_ = state;
  error_code_ = error_code;

  if (state != protocol::ConnectionToHost::State::CONNECTED ||
      error_code != protocol::OK) {
    is_connected_to_host_ = false;
  }
}

void TestChromotingClientTest::ConnectionReady(bool ready) {
  if (ready) {
    is_connected_to_host_ = true;
  }
}

TEST_F(TestChromotingClientTest, StartConnectionAndDisconnect) {
  test_chromoting_client_->StartConnection(kTestUserName, kAccessToken,
                                           remote_host_info_);
  EXPECT_EQ(protocol::ConnectionToHost::State::CONNECTING, connection_state_);
  EXPECT_EQ(protocol::OK, error_code_);
  EXPECT_FALSE(is_connected_to_host_);

  // Simulate an AUTHENTICATED message being sent from the Jingle session.
  fake_connection_to_host_->SignalStateChange(protocol::Session::AUTHENTICATED,
                                              protocol::OK);
  EXPECT_EQ(protocol::ConnectionToHost::State::AUTHENTICATED,
            connection_state_);
  EXPECT_EQ(protocol::OK, error_code_);
  EXPECT_FALSE(is_connected_to_host_);

  // Simulate a CONNECTED message being sent from the Jingle session.
  fake_connection_to_host_->SignalStateChange(protocol::Session::CONNECTED,
                                              protocol::OK);
  EXPECT_EQ(protocol::ConnectionToHost::State::CONNECTED, connection_state_);
  EXPECT_EQ(protocol::OK, error_code_);
  EXPECT_FALSE(is_connected_to_host_);

  fake_connection_to_host_->SignalConnectionReady(true);
  EXPECT_EQ(protocol::ConnectionToHost::State::CONNECTED, connection_state_);
  EXPECT_EQ(protocol::OK, error_code_);
  EXPECT_TRUE(is_connected_to_host_);

  test_chromoting_client_->EndConnection();
  EXPECT_EQ(protocol::ConnectionToHost::State::CLOSED, connection_state_);
  EXPECT_EQ(protocol::OK, error_code_);
  EXPECT_FALSE(is_connected_to_host_);
}

TEST_F(TestChromotingClientTest,
       StartConnectionThenFailWithAuthenticationError) {
  test_chromoting_client_->StartConnection(kTestUserName, kAccessToken,
                                           remote_host_info_);
  EXPECT_EQ(protocol::ConnectionToHost::State::CONNECTING, connection_state_);
  EXPECT_EQ(protocol::OK, error_code_);
  EXPECT_FALSE(is_connected_to_host_);

  fake_connection_to_host_->SignalStateChange(protocol::Session::FAILED,
                                              protocol::AUTHENTICATION_FAILED);
  EXPECT_EQ(protocol::ConnectionToHost::State::FAILED, connection_state_);
  EXPECT_EQ(protocol::ErrorCode::AUTHENTICATION_FAILED, error_code_);
  EXPECT_FALSE(is_connected_to_host_);

  // Close the connection via the TestChromotingClient and verify the error
  // state is persisted.
  test_chromoting_client_->EndConnection();
  EXPECT_EQ(protocol::ConnectionToHost::State::FAILED, connection_state_);
  EXPECT_EQ(protocol::ErrorCode::AUTHENTICATION_FAILED, error_code_);
  EXPECT_FALSE(is_connected_to_host_);
}

TEST_F(TestChromotingClientTest, StartConnectionThenFailWithUnknownError) {
  test_chromoting_client_->StartConnection(kTestUserName, kAccessToken,
                                           remote_host_info_);
  EXPECT_EQ(protocol::ConnectionToHost::State::CONNECTING, connection_state_);
  EXPECT_EQ(protocol::OK, error_code_);
  EXPECT_FALSE(is_connected_to_host_);

  // Simulate an AUTHENTICATED message being sent from the Jingle session.
  fake_connection_to_host_->SignalStateChange(protocol::Session::AUTHENTICATED,
                                              protocol::OK);
  EXPECT_EQ(protocol::ConnectionToHost::State::AUTHENTICATED,
            connection_state_);
  EXPECT_EQ(protocol::OK, error_code_);
  EXPECT_FALSE(is_connected_to_host_);

  // Simulate a CONNECTED message being sent from the Jingle session.
  fake_connection_to_host_->SignalStateChange(protocol::Session::CONNECTED,
                                              protocol::OK);
  EXPECT_EQ(protocol::ConnectionToHost::State::CONNECTED, connection_state_);
  EXPECT_EQ(protocol::OK, error_code_);
  EXPECT_FALSE(is_connected_to_host_);

  fake_connection_to_host_->SignalConnectionReady(true);
  EXPECT_EQ(protocol::ConnectionToHost::State::CONNECTED, connection_state_);
  EXPECT_EQ(protocol::OK, error_code_);
  EXPECT_TRUE(is_connected_to_host_);

  fake_connection_to_host_->SignalStateChange(protocol::Session::FAILED,
                                              protocol::UNKNOWN_ERROR);
  EXPECT_EQ(protocol::ConnectionToHost::State::FAILED, connection_state_);
  EXPECT_EQ(protocol::ErrorCode::UNKNOWN_ERROR, error_code_);
  EXPECT_FALSE(is_connected_to_host_);

  // Close the connection via the TestChromotingClient and verify the error
  // state is persisted.
  test_chromoting_client_->EndConnection();
  EXPECT_EQ(protocol::ConnectionToHost::State::FAILED, connection_state_);
  EXPECT_EQ(protocol::ErrorCode::UNKNOWN_ERROR, error_code_);
  EXPECT_FALSE(is_connected_to_host_);
}

}  // namespace test
}  // namespace remoting
