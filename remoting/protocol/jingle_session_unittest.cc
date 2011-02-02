// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/nss_util.h"
#include "base/path_service.h"
#include "base/time.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"
#include "remoting/protocol/jingle_session.h"
#include "remoting/protocol/jingle_session_manager.h"
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
namespace protocol {
class JingleSessionTest;
}  // namespace protocol
}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::protocol::JingleSessionTest);

namespace remoting {
namespace protocol {

namespace {
// Send 100 messages 1024 bytes each. UDP messages are sent with 10ms delay
// between messages (about 1 second for 100 messages).
const int kMessageSize = 1024;
const int kMessages = 100;
const int kTestDataSize = kMessages * kMessageSize;
const int kUdpWriteDelayMs = 10;
const char kTestToken[] = "a_dummy_token";
}  // namespace

class MockSessionManagerCallback {
 public:
  MOCK_METHOD2(OnIncomingSession,
               void(Session*,
                    SessionManager::IncomingSessionResponse*));
};

class MockSessionCallback {
 public:
  MOCK_METHOD1(OnStateChange, void(Session::State));
};

class JingleSessionTest : public testing::Test {
 public:
  // Helper method to copy to set value of client_connection_.
  void SetHostSession(Session* session) {
    DCHECK(session);
    host_session_ = session;
    host_session_->SetStateChangeCallback(
        NewCallback(&host_connection_callback_,
                    &MockSessionCallback::OnStateChange));

    session->set_config(SessionConfig::CreateDefault());
  }

 protected:
  virtual void SetUp() {
    thread_.Start();
  }

  virtual void TearDown() {
    CloseSessions();

    if (host_server_) {
      host_server_->Close(NewRunnableFunction(
          &JingleSessionTest::DoNothing));
    }
    if (client_server_) {
      client_server_->Close(NewRunnableFunction(
          &JingleSessionTest::DoNothing));
    }
    thread_.Stop();
  }

  void CreateServerPair() {
    // SessionManagerPair must be initialized on the jingle thread.
    thread_.message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(
            this, &JingleSessionTest::DoCreateServerPair));
    SyncWithJingleThread();
  }

  void CloseSessions() {
    if (host_session_) {
      host_session_->Close(NewRunnableFunction(
          &JingleSessionTest::DoNothing));
    }
    if (client_session_) {
      client_session_->Close(NewRunnableFunction(
          &JingleSessionTest::DoNothing));
    }
    SyncWithJingleThread();
  }

  void DoCreateServerPair() {
    session_manager_pair_ = new SessionManagerPair(&thread_);
    session_manager_pair_->Init();
    host_server_ = new JingleSessionManager(&thread_);
    host_server_->set_allow_local_ips(true);
    host_server_->Init(SessionManagerPair::kHostJid,
                       session_manager_pair_->host_session_manager(),
                       NewCallback(&host_server_callback_,
                           &MockSessionManagerCallback::OnIncomingSession));

    client_server_ = new JingleSessionManager(&thread_);
    client_server_->set_allow_local_ips(true);
    client_server_->Init(
        SessionManagerPair::kClientJid,
        session_manager_pair_->client_session_manager(),
        NewCallback(&client_server_callback_,
                    &MockSessionManagerCallback::OnIncomingSession));

    FilePath certs_dir;
    PathService::Get(base::DIR_SOURCE_ROOT, &certs_dir);
    certs_dir = certs_dir.AppendASCII("net");
    certs_dir = certs_dir.AppendASCII("data");
    certs_dir = certs_dir.AppendASCII("ssl");
    certs_dir = certs_dir.AppendASCII("certificates");

    FilePath cert_path = certs_dir.AppendASCII("unittest.selfsigned.der");
    std::string cert_der;
    ASSERT_TRUE(file_util::ReadFileToString(cert_path, &cert_der));

    scoped_refptr<net::X509Certificate> cert =
        net::X509Certificate::CreateFromBytes(cert_der.data(),
                                              cert_der.size());

    FilePath key_path = certs_dir.AppendASCII("unittest.key.bin");
    std::string key_string;
    ASSERT_TRUE(file_util::ReadFileToString(key_path, &key_string));
    std::vector<uint8> key_vector(
        reinterpret_cast<const uint8*>(key_string.data()),
        reinterpret_cast<const uint8*>(key_string.data() +
                                       key_string.length()));

    scoped_ptr<base::RSAPrivateKey> private_key(
        base::RSAPrivateKey::CreateFromPrivateKeyInfo(key_vector));
    host_server_->SetCertificate(cert);
    host_server_->SetPrivateKey(private_key.release());
  }

  bool InitiateConnection() {
    EXPECT_CALL(host_server_callback_, OnIncomingSession(_, _))
        .WillOnce(DoAll(
            WithArg<0>(Invoke(
                this, &JingleSessionTest::SetHostSession)),
            SetArgumentPointee<1>(protocol::SessionManager::ACCEPT)));

    base::WaitableEvent host_connected_event(false, false);
    EXPECT_CALL(host_connection_callback_,
                OnStateChange(Session::CONNECTING))
        .Times(1);
    EXPECT_CALL(host_connection_callback_,
                OnStateChange(Session::CONNECTED))
        .Times(1)
        .WillOnce(InvokeWithoutArgs(&host_connected_event,
                                    &base::WaitableEvent::Signal));
    // Expect that the connection will be closed eventually.
    EXPECT_CALL(host_connection_callback_,
                OnStateChange(Session::CLOSED))
        .Times(1);

    base::WaitableEvent client_connected_event(false, false);
    EXPECT_CALL(client_connection_callback_,
                OnStateChange(Session::CONNECTING))
        .Times(1);
    EXPECT_CALL(client_connection_callback_,
                OnStateChange(Session::CONNECTED))
        .Times(1)
        .WillOnce(InvokeWithoutArgs(&client_connected_event,
                                    &base::WaitableEvent::Signal));
    // Expect that the connection will be closed eventually.
    EXPECT_CALL(client_connection_callback_,
                OnStateChange(Session::CLOSED))
        .Times(1);

    client_session_ = client_server_->Connect(
        SessionManagerPair::kHostJid,
        kTestToken,
        CandidateSessionConfig::CreateDefault(),
        NewCallback(&client_connection_callback_,
                    &MockSessionCallback::OnStateChange));

    return host_connected_event.TimedWait(base::TimeDelta::FromMilliseconds(
            TestTimeouts::action_max_timeout_ms())) &&
        client_connected_event.TimedWait(base::TimeDelta::FromMilliseconds(
            TestTimeouts::action_max_timeout_ms()));
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
  scoped_refptr<JingleSessionManager> host_server_;
  MockSessionManagerCallback host_server_callback_;
  scoped_refptr<JingleSessionManager> client_server_;
  MockSessionManagerCallback client_server_callback_;

  scoped_refptr<Session> host_session_;
  MockSessionCallback host_connection_callback_;
  scoped_refptr<Session> client_session_;
  MockSessionCallback client_connection_callback_;
};

class ChannelTesterBase : public base::RefCountedThreadSafe<ChannelTesterBase> {
 public:
  enum ChannelType {
    CONTROL,
    EVENT,
    VIDEO,
    VIDEO_RTP,
    VIDEO_RTCP,
  };

  ChannelTesterBase(MessageLoop* message_loop,
                    Session* host_session,
                    Session* client_session)
      : message_loop_(message_loop),
        host_session_(host_session),
        client_session_(client_session),
        done_event_(true, false) {
  }

  virtual ~ChannelTesterBase() { }

  void Start(ChannelType channel) {
    message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &ChannelTesterBase::DoStart,
                                     channel));
  }

  bool WaitFinished() {
    return done_event_.TimedWait(base::TimeDelta::FromMilliseconds(
        TestTimeouts::action_max_timeout_ms()));
  }

  virtual void CheckResults() = 0;

 protected:
  void DoStart(ChannelType channel)  {
    socket_1_ = SelectChannel(host_session_, channel);
    socket_2_ = SelectChannel(client_session_, channel);

    InitBuffers();
    DoRead();
    DoWrite();
  }

  virtual void InitBuffers() = 0;
  virtual void DoWrite() = 0;
  virtual void DoRead() = 0;

  net::Socket* SelectChannel(Session* session,
                             ChannelType channel) {
    switch (channel) {
      case CONTROL:
        return session->control_channel();
      case EVENT:
        return session->event_channel();
      case VIDEO:
        return session->video_channel();
      case VIDEO_RTP:
        return session->video_rtp_channel();
      case VIDEO_RTCP:
        return session->video_rtcp_channel();
      default:
        NOTREACHED();
        return NULL;
    }
  }

  MessageLoop* message_loop_;
  scoped_refptr<Session> host_session_;
  scoped_refptr<Session> client_session_;
  net::Socket* socket_1_;
  net::Socket* socket_2_;
  base::WaitableEvent done_event_;
};

class TCPChannelTester : public ChannelTesterBase {
 public:
  TCPChannelTester(MessageLoop* message_loop,
                   Session* host_session,
                   Session* client_session)
      : ChannelTesterBase(message_loop, host_session, client_session),
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

    ASSERT_EQ(kTestDataSize + kMessageSize, input_buffer_->capacity());

    output_buffer_->SetOffset(0);
    ASSERT_EQ(kTestDataSize, output_buffer_->size());

    EXPECT_EQ(0, memcmp(output_buffer_->data(),
                        input_buffer_->StartOfBuffer(), kTestDataSize));
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
    if (!done_event_.IsSignaled())
      DoRead();  // Don't try to read again when we are done reading.
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
                   Session* host_session,
                   Session* client_session)
      : ChannelTesterBase(message_loop, host_session, client_session),
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

    scoped_refptr<net::IOBuffer> packet(new net::IOBuffer(kMessageSize));
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

// Mac needs to implement X509Certificate::CreateSelfSigned to enable these
// tests.
#if defined(USE_NSS) || defined(OS_WIN)

// Verify that we can create and destory server objects without a connection.
TEST_F(JingleSessionTest, CreateAndDestoy) {
  CreateServerPair();
}

// Verify that incoming session can be rejected, and that the status
// of the connection is set to CLOSED in this case.
TEST_F(JingleSessionTest, RejectConnection) {
  CreateServerPair();

  // Reject incoming session.
  EXPECT_CALL(host_server_callback_, OnIncomingSession(_, _))
      .WillOnce(SetArgumentPointee<1>(protocol::SessionManager::DECLINE));

  base::WaitableEvent done_event(false, false);
  EXPECT_CALL(client_connection_callback_,
              OnStateChange(Session::CONNECTING))
      .Times(1);
  EXPECT_CALL(client_connection_callback_,
              OnStateChange(Session::CLOSED))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(&done_event, &base::WaitableEvent::Signal));

  client_session_ = client_server_->Connect(
      SessionManagerPair::kHostJid,
      kTestToken,
      CandidateSessionConfig::CreateDefault(),
      NewCallback(&client_connection_callback_,
                  &MockSessionCallback::OnStateChange));

  ASSERT_TRUE(
      done_event.TimedWait(base::TimeDelta::FromMilliseconds(
          TestTimeouts::action_max_timeout_ms())));
}

// Verify that we can connect two endpoints.
// Disabled, http://crbug.com/70225.
TEST_F(JingleSessionTest, DISABLED_Connect) {
  CreateServerPair();
  ASSERT_TRUE(InitiateConnection());
}

// Verify that data can be transmitted over the event channel.
// Disabled, http://crbug.com/70225.
TEST_F(JingleSessionTest, DISABLED_TestControlChannel) {
  CreateServerPair();
  ASSERT_TRUE(InitiateConnection());
  scoped_refptr<TCPChannelTester> tester(
      new TCPChannelTester(thread_.message_loop(), host_session_,
                           client_session_));
  tester->Start(ChannelTesterBase::CONTROL);
  ASSERT_TRUE(tester->WaitFinished());
  tester->CheckResults();

  // Connections must be closed while |tester| still exists.
  CloseSessions();
}

// Verify that data can be transmitted over the video channel.
TEST_F(JingleSessionTest, TestVideoChannel) {
  CreateServerPair();
  ASSERT_TRUE(InitiateConnection());
  scoped_refptr<TCPChannelTester> tester(
      new TCPChannelTester(thread_.message_loop(), host_session_,
                           client_session_));
  tester->Start(ChannelTesterBase::VIDEO);
  ASSERT_TRUE(tester->WaitFinished());
  tester->CheckResults();

  // Connections must be closed while |tester| still exists.
  CloseSessions();
}

// Verify that data can be transmitted over the event channel.
TEST_F(JingleSessionTest, TestEventChannel) {
  CreateServerPair();
  ASSERT_TRUE(InitiateConnection());
  scoped_refptr<TCPChannelTester> tester(
      new TCPChannelTester(thread_.message_loop(), host_session_,
                           client_session_));
  tester->Start(ChannelTesterBase::EVENT);
  ASSERT_TRUE(tester->WaitFinished());
  tester->CheckResults();

  // Connections must be closed while |tester| still exists.
  CloseSessions();
}

// Verify that data can be transmitted over the video RTP channel.
TEST_F(JingleSessionTest, TestVideoRtpChannel) {
  CreateServerPair();
  ASSERT_TRUE(InitiateConnection());
  scoped_refptr<UDPChannelTester> tester(
      new UDPChannelTester(thread_.message_loop(), host_session_,
                           client_session_));
  tester->Start(ChannelTesterBase::VIDEO_RTP);
  ASSERT_TRUE(tester->WaitFinished());
  tester->CheckResults();

  // Connections must be closed while |tester| still exists.
  CloseSessions();
}

#endif

}  // namespace protocol
}  // namespace remoting
