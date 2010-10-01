// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time.h"
#include "base/waitable_event.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"
#include "remoting/protocol/jingle_chromoting_connection.h"
#include "remoting/protocol/jingle_chromoting_server.h"
#include "remoting/protocol/session_manager_pair.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/p2p/client/basicportallocator.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SaveArg;
using testing::SetArgumentPointee;
using testing::WithArg;

namespace remoting {
class JingleChromotingConnectionTest;
}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::JingleChromotingConnectionTest);

namespace remoting {

namespace {
const int kConnectionTimeoutMs = 10000;
const int kSendTimeoutMs = 20000;

// Send 100 messages 1024 bytes each. For UDP messages are send with
// 10ms interval (about 1 seconds for 100 messages).
const int kMessageSize = 1024;
const int kMessages = 100;
const int kTestDataSize = kMessages * kMessageSize;
const int kUdpWriteDelayMs = 10;
}  // namespace

class MockServerCallback {
 public:
  MOCK_METHOD2(OnNewConnection, void(ChromotingConnection*, bool*));
};

class MockConnectionCallback {
 public:
  MOCK_METHOD1(OnStateChange, void(ChromotingConnection::State));
};

class JingleChromotingConnectionTest : public testing::Test {
 public:
  // Helper method to copy to set value of client_connection_.
  void SetHostConnection(ChromotingConnection* connection) {
    DCHECK(connection);
    host_connection_ = connection;
    host_connection_->SetStateChangeCallback(
        NewCallback(&host_connection_callback_,
                    &MockConnectionCallback::OnStateChange));
  }

 protected:
  virtual void SetUp() {
    thread_.Start();
  }

  virtual void TearDown() {
    CloseConnections();

    if (host_server_) {
      host_server_->Close(NewRunnableFunction(
          &JingleChromotingConnectionTest::DoNothing));
    }
    if (client_server_) {
      client_server_->Close(NewRunnableFunction(
          &JingleChromotingConnectionTest::DoNothing));
    }
    thread_.Stop();
  }

  void CreateServerPair() {
    // SessionManagerPair must be initialized on the jingle thread.
    thread_.message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(
            this, &JingleChromotingConnectionTest::DoCreateServerPair));
    SyncWithJingleThread();
  }

  void CloseConnections() {
    if (host_connection_) {
      host_connection_->Close(NewRunnableFunction(
      &JingleChromotingConnectionTest::DoNothing));
    }
    if (client_connection_) {
      client_connection_->Close(NewRunnableFunction(
          &JingleChromotingConnectionTest::DoNothing));
    }
    SyncWithJingleThread();
  }

  void DoCreateServerPair() {
    session_manager_pair_ = new SessionManagerPair(&thread_);
    session_manager_pair_->Init();
    host_server_ = new JingleChromotingServer(thread_.message_loop());
    host_server_->set_allow_local_ips(true);
    host_server_->Init(SessionManagerPair::kHostJid,
                       session_manager_pair_->host_session_manager(),
                       NewCallback(&host_server_callback_,
                                   &MockServerCallback::OnNewConnection));
    client_server_ = new JingleChromotingServer(thread_.message_loop());
    client_server_->set_allow_local_ips(true);
    client_server_->Init(SessionManagerPair::kClientJid,
                         session_manager_pair_->client_session_manager(),
                         NewCallback(&client_server_callback_,
                                     &MockServerCallback::OnNewConnection));
  }

  void InitiateConnection() {
    EXPECT_CALL(host_server_callback_, OnNewConnection(_, _))
        .WillOnce(DoAll(
            WithArg<0>(
                Invoke(this,
                       &JingleChromotingConnectionTest::SetHostConnection)),
            SetArgumentPointee<1>(true)));

    base::WaitableEvent host_connected_event(false, false);
    EXPECT_CALL(host_connection_callback_,
                OnStateChange(ChromotingConnection::CONNECTING))
        .Times(1);
    EXPECT_CALL(host_connection_callback_,
                OnStateChange(ChromotingConnection::CONNECTED))
        .Times(1)
        .WillOnce(InvokeWithoutArgs(&host_connected_event,
                                    &base::WaitableEvent::Signal));

    base::WaitableEvent client_connected_event(false, false);
    EXPECT_CALL(client_connection_callback_,
                OnStateChange(ChromotingConnection::CONNECTING))
        .Times(1);
    EXPECT_CALL(client_connection_callback_,
                OnStateChange(ChromotingConnection::CONNECTED))
        .Times(1)
        .WillOnce(InvokeWithoutArgs(&client_connected_event,
                                    &base::WaitableEvent::Signal));

    client_connection_ = client_server_->Connect(
        SessionManagerPair::kHostJid,
        NewCallback(&client_connection_callback_,
                    &MockConnectionCallback::OnStateChange));

    host_connected_event.TimedWait(
        base::TimeDelta::FromMilliseconds(kConnectionTimeoutMs));
    client_connected_event.TimedWait(
        base::TimeDelta::FromMilliseconds(kConnectionTimeoutMs));
  }

  static void SignalEvent(base::WaitableEvent* event) {
    event->Signal();
  }

  static void DoNothing() { }

  void SyncWithJingleThread() {
    base::WaitableEvent event(true, false);
    thread_.message_loop()->PostTask(
        FROM_HERE, NewRunnableFunction(&SignalEvent, &event));
    event.Wait();
  }

  JingleThread thread_;
  scoped_refptr<SessionManagerPair> session_manager_pair_;
  scoped_refptr<JingleChromotingServer> host_server_;
  MockServerCallback host_server_callback_;
  scoped_refptr<JingleChromotingServer> client_server_;
  MockServerCallback client_server_callback_;

  scoped_refptr<ChromotingConnection> host_connection_;
  MockConnectionCallback host_connection_callback_;
  scoped_refptr<ChromotingConnection> client_connection_;
  MockConnectionCallback client_connection_callback_;
};

class ChannelTesterBase : public base::RefCountedThreadSafe<ChannelTesterBase> {
 public:
  enum ChannelType {
    EVENTS,
    VIDEO,
    VIDEO_RTP,
    VIDEO_RTCP,
  };

  ChannelTesterBase(MessageLoop* message_loop,
                    ChromotingConnection* host_connection,
                    ChromotingConnection* client_connection)
      : message_loop_(message_loop),
        host_connection_(host_connection),
        client_connection_(client_connection),
        done_event_(true, false) {
  }

  virtual ~ChannelTesterBase() { }

  void Start(ChannelType channel) {
    message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &ChannelTesterBase::DoStart,
                                     channel));
  }

  void WaitFinished() {
    done_event_.TimedWait(base::TimeDelta::FromMilliseconds(kSendTimeoutMs));
  }

  virtual void CheckResults() = 0;

 protected:
  void DoStart(ChannelType channel)  {
    socket_1_ = SelectChannel(host_connection_, channel);
    socket_2_ = SelectChannel(client_connection_, channel);

    InitBuffers();
    DoRead();
    DoWrite();
  }

  virtual void InitBuffers() = 0;
  virtual void DoWrite() = 0;
  virtual void DoRead() = 0;

  net::Socket* SelectChannel(ChromotingConnection* connection,
                             ChannelType channel) {
    switch (channel) {
      case EVENTS:
        return connection->GetEventsChannel();
      case VIDEO:
        return connection->GetVideoChannel();
      case VIDEO_RTP:
        return connection->GetVideoRtpChannel();
      case VIDEO_RTCP:
        return connection->GetVideoRtcpChannel();
      default:
        NOTREACHED();
        return NULL;
    }
  }

  MessageLoop* message_loop_;
  scoped_refptr<ChromotingConnection> host_connection_;
  scoped_refptr<ChromotingConnection> client_connection_;
  net::Socket* socket_1_;
  net::Socket* socket_2_;
  base::WaitableEvent done_event_;
};

class TCPChannelTester : public ChannelTesterBase {
 public:
  TCPChannelTester(MessageLoop* message_loop,
                   ChromotingConnection* host_connection,
                   ChromotingConnection* client_connection)
      : ChannelTesterBase(message_loop, host_connection, client_connection),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            write_cb_(this, &TCPChannelTester::OnWritten)),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            read_cb_(this, &TCPChannelTester::OnRead)),
        write_errors_(0),
        read_errors_(0) {
  }

  virtual ~TCPChannelTester() { }

  virtual void CheckResults() {
    EXPECT_EQ(0, write_errors_);
    EXPECT_EQ(0, read_errors_);

    input_buffer_->set_offset(0);
    ASSERT_EQ(kTestDataSize + kMessageSize, input_buffer_->capacity());

    output_buffer_->SetOffset(0);
    ASSERT_EQ(kTestDataSize, output_buffer_->size());

    EXPECT_EQ(0, memcmp(output_buffer_->data(),
                        input_buffer_->data(), kTestDataSize));
  }

 protected:
  virtual void InitBuffers() {
    output_buffer_ = new net::DrainableIOBuffer(
        new net::IOBuffer(kTestDataSize), kTestDataSize);
    memset(output_buffer_->data(), 123, kTestDataSize);

    input_buffer_ = new net::GrowableIOBuffer();
    // Always keep kMessageSize bytes available at the end of the input buffer.
    input_buffer_->SetCapacity(kMessageSize);
  }

  virtual void DoWrite() {
    int result = 1;
    while (result > 0) {
      if (output_buffer_->BytesRemaining() == 0)
        break;

      int bytes_to_write = std::min(output_buffer_->BytesRemaining(),
                                    kMessageSize);
      result = socket_1_->Write(output_buffer_, bytes_to_write, &write_cb_);
      HandleWriteResult(result);
    };
  }

  void OnWritten(int result) {
    HandleWriteResult(result);
    DoWrite();
  }

  void HandleWriteResult(int result) {
    if (result <= 0 && result != net::ERR_IO_PENDING) {
      LOG(ERROR) << "Received error " << result << " when trying to write";
      write_errors_++;
      done_event_.Signal();
    } else if (result > 0) {
      output_buffer_->DidConsume(result);
    }
  }

  virtual void DoRead() {
    int result = 1;
    while (result > 0) {
      input_buffer_->set_offset(input_buffer_->capacity() - kMessageSize);

      result = socket_2_->Read(input_buffer_, kMessageSize, &read_cb_);
      HandleReadResult(result);
    };
  }

  void OnRead(int result) {
    HandleReadResult(result);
    DoRead();
  }

  void HandleReadResult(int result) {
    if (result <= 0 && result != net::ERR_IO_PENDING) {
      if (!done_event_.IsSignaled()) {
        LOG(ERROR) << "Received error " << result << " when trying to read";
        read_errors_++;
        done_event_.Signal();
      }
    } else if (result > 0) {
      // Allocate memory for the next read.
      input_buffer_->SetCapacity(input_buffer_->capacity() + result);
      if (input_buffer_->capacity() == kTestDataSize + kMessageSize)
        done_event_.Signal();
    }
  }

 private:
  scoped_refptr<net::DrainableIOBuffer> output_buffer_;
  scoped_refptr<net::GrowableIOBuffer> input_buffer_;

  net::CompletionCallbackImpl<TCPChannelTester> write_cb_;
  net::CompletionCallbackImpl<TCPChannelTester> read_cb_;
  int write_errors_;
  int read_errors_;
};

class UDPChannelTester : public ChannelTesterBase {
 public:
  UDPChannelTester(MessageLoop* message_loop,
                   ChromotingConnection* host_connection,
                   ChromotingConnection* client_connection)
      : ChannelTesterBase(message_loop, host_connection, client_connection),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            write_cb_(this, &UDPChannelTester::OnWritten)),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            read_cb_(this, &UDPChannelTester::OnRead)),
        write_errors_(0),
        read_errors_(0),
        packets_sent_(0),
        packets_received_(0),
        broken_packets_(0) {
  }

  virtual ~UDPChannelTester() { }

  virtual void CheckResults() {
    EXPECT_EQ(0, write_errors_);
    EXPECT_EQ(0, read_errors_);

    EXPECT_EQ(0, broken_packets_);

    // Verify that we've received at least one packet.
    EXPECT_GT(packets_received_, 0);
    LOG(INFO) << "Received " << packets_received_ << " packets out of "
              << kMessages;
  }

 protected:
  virtual void InitBuffers() {
  }

  virtual void DoWrite() {
    if (packets_sent_ >= kMessages) {
      done_event_.Signal();
      return;
    }

    scoped_refptr<net::IOBuffer> packet = new net::IOBuffer(kMessageSize);
    memset(packet->data(), 123, kMessageSize);
    sent_packets_[packets_sent_] = packet;
    // Put index of this packet in the beginning of the packet body.
    memcpy(packet->data(), &packets_sent_, sizeof(packets_sent_));

    int result = socket_1_->Write(packet, kMessageSize, &write_cb_);
    HandleWriteResult(result);
  }

  void OnWritten(int result) {
    HandleWriteResult(result);
  }

  void HandleWriteResult(int result) {
    if (result <= 0 && result != net::ERR_IO_PENDING) {
      LOG(ERROR) << "Received error " << result << " when trying to write";
      write_errors_++;
      done_event_.Signal();
    } else if (result > 0) {
      EXPECT_EQ(kMessageSize, result);
      packets_sent_++;
      message_loop_->PostDelayedTask(
          FROM_HERE, NewRunnableMethod(this, &UDPChannelTester::DoWrite),
          kUdpWriteDelayMs);
    }
  }

  virtual void DoRead() {
    int result = 1;
    while (result > 0) {
      int kReadSize = kMessageSize * 2;
      read_buffer_ = new net::IOBuffer(kReadSize);

      result = socket_2_->Read(read_buffer_, kReadSize, &read_cb_);
      HandleReadResult(result);
    };
  }

  void OnRead(int result) {
    HandleReadResult(result);
    DoRead();
  }

  void HandleReadResult(int result) {
    if (result <= 0 && result != net::ERR_IO_PENDING) {
      // Error will be received after the socket is closed.
      if (!done_event_.IsSignaled()) {
        LOG(ERROR) << "Received error " << result << " when trying to read";
        read_errors_++;
        done_event_.Signal();
      }
    } else if (result > 0) {
      packets_received_++;
      if (kMessageSize != result) {
        // Invalid packet size;
        broken_packets_++;
      } else {
        // Validate packet body.
        int packet_id;
        memcpy(&packet_id, read_buffer_->data(), sizeof(packet_id));
        if (packet_id < 0 || packet_id >= kMessages) {
          broken_packets_++;
        } else {
          if (memcmp(read_buffer_->data(), sent_packets_[packet_id]->data(),
                     kMessageSize) != 0)
            broken_packets_++;
        }
      }
    }
  }

 private:
  scoped_refptr<net::IOBuffer> sent_packets_[kMessages];
  scoped_refptr<net::IOBuffer> read_buffer_;

  net::CompletionCallbackImpl<UDPChannelTester> write_cb_;
  net::CompletionCallbackImpl<UDPChannelTester> read_cb_;
  int write_errors_;
  int read_errors_;
  int packets_sent_;
  int packets_received_;
  int broken_packets_;
};

// Verify that we can create and destory server objects without a connection.
TEST_F(JingleChromotingConnectionTest, CreateAndDestoy) {
  CreateServerPair();
}

// Verify that incoming connection can be rejected, and that the status
// of the connection is set to CLOSED in this case.
TEST_F(JingleChromotingConnectionTest, RejectConnection) {
  CreateServerPair();

  // Reject incoming connection.
  EXPECT_CALL(host_server_callback_, OnNewConnection(_, _))
      .WillOnce(SetArgumentPointee<1>(false));

  base::WaitableEvent done_event(false, false);
  EXPECT_CALL(client_connection_callback_,
              OnStateChange(ChromotingConnection::CONNECTING))
      .Times(1);
  EXPECT_CALL(client_connection_callback_,
              OnStateChange(ChromotingConnection::CLOSED))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal));

  client_connection_ = client_server_->Connect(
      SessionManagerPair::kHostJid,
      NewCallback(&client_connection_callback_,
                  &MockConnectionCallback::OnStateChange));

  done_event.TimedWait(
      base::TimeDelta::FromMilliseconds(kConnectionTimeoutMs));
}

// Verify that we can connect two endpoints.
TEST_F(JingleChromotingConnectionTest, Connect) {
  CreateServerPair();
  InitiateConnection();
}

// Verify that data can be transmitted over the video channel.
TEST_F(JingleChromotingConnectionTest, TestVideoChannel) {
  CreateServerPair();
  InitiateConnection();
  scoped_refptr<TCPChannelTester> tester =
      new TCPChannelTester(thread_.message_loop(), host_connection_,
                           client_connection_);
  tester->Start(ChannelTesterBase::VIDEO);
  tester->WaitFinished();
  tester->CheckResults();

  // Connections must be closed while |tester| still exists.
  CloseConnections();
}

// Verify that data can be transmitted over the events channel.
TEST_F(JingleChromotingConnectionTest, TestEventsChannel) {
  CreateServerPair();
  InitiateConnection();
  scoped_refptr<TCPChannelTester> tester =
      new TCPChannelTester(thread_.message_loop(), host_connection_,
                           client_connection_);
  tester->Start(ChannelTesterBase::EVENTS);
  tester->WaitFinished();
  tester->CheckResults();

  // Connections must be closed while |tester| still exists.
  CloseConnections();
}

// Verify that data can be transmitted over the video RTP channel.
TEST_F(JingleChromotingConnectionTest, TestVideoRtpChannel) {
  CreateServerPair();
  InitiateConnection();
  scoped_refptr<UDPChannelTester> tester =
      new UDPChannelTester(thread_.message_loop(), host_connection_,
                           client_connection_);
  tester->Start(ChannelTesterBase::VIDEO_RTP);
  tester->WaitFinished();
  tester->CheckResults();

  // Connections must be closed while |tester| still exists.
  CloseConnections();
}

}  // namespace remoting
