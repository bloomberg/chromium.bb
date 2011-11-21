// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/time.h"
#include "base/test/test_timeouts.h"
#include "crypto/nss_util.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"
#include "net/socket/stream_socket.h"
#include "remoting/protocol/jingle_session.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/jingle_glue/fake_signal_strategy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/p2p/client/basicportallocator.h"

using testing::_;
using testing::AtMost;
using testing::DeleteArg;
using testing::DoAll;
using testing::InSequence;
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
const char kChannelName[] = "test_channel";

const char kHostJid[] = "host1@gmail.com/123";
const char kClientJid[] = "host2@gmail.com/321";

const char kTestHostPublicKey[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA3nk/8ILc0JBqHgOS0UCOIl4m"
    "0GUd2FIiZ/6Fc9D/iiyUgli+FIY5dwsrSoNJ87sYGifVDh8a5fdZNV5y58CcrapI5fJI"
    "FpXviSW4g8d/t1gcZkoz1ppmjzbgXm6ckw9Td0yRD0cHu732Ijs+eo8wT0pt4KiHkbyR"
    "iAvjrvkNDlfiEk7tiY7YzD9zTi3146GX6KLz5GQAd/3I8I5QW3ftF1s/m93AHuc383GZ"
    "A78Oi+IbcJf/jJUZO119VNnRKGiPsf5GZIoHyXX8O5OUQk5soKdQPeK1FwWkeZu6fuXl"
    "QoU12I6podD6xMFa/PA/xefMwcpmuWTRhcso9bp10zVFGQIDAQAB";

const char kTestSharedSecret[] = "1234-1234-5678";
const char kTestSharedSecretBad[] = "0000-0000-0001";

void QuitCurrentThread() {
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

void OnTimeoutTerminateThread(bool* timeout) {
  *timeout = true;
  QuitCurrentThread();
}

bool RunMessageLoopWithTimeout(int timeout_ms) {
  bool timeout = false;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&OnTimeoutTerminateThread, &timeout), timeout_ms);
  MessageLoop::current()->Run();
  return !timeout;
}

ACTION_P(QuitThreadOnCounter, counter) {
  (*counter)--;
  EXPECT_GE(*counter, 0);
  if (*counter == 0)
    QuitCurrentThread();
}

class MockSessionManagerListener : public SessionManager::Listener {
 public:
  MOCK_METHOD0(OnSessionManagerInitialized, void());
  MOCK_METHOD2(OnIncomingSession,
               void(Session*,
                    SessionManager::IncomingSessionResponse*));
};

class MockSessionCallback {
 public:
  MOCK_METHOD1(OnStateChange, void(Session::State));
};

}  // namespace

class JingleSessionTest : public testing::Test {
 public:
  JingleSessionTest()
      : message_loop_(talk_base::Thread::Current()) {
  }

  // Helper method to copy to set value of client_connection_.
  void SetHostSession(Session* session) {
    DCHECK(session);
    host_session_.reset(session);
    host_session_->SetStateChangeCallback(
        base::Bind(&MockSessionCallback::OnStateChange,
                   base::Unretained(&host_connection_callback_)));

    session->set_config(SessionConfig::GetDefault());
    session->set_shared_secret(kTestSharedSecret);
  }

 protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
    CloseSessions();
    CloseSessionManager();
  }

  void CloseSessions() {
    if (host_session_.get()) {
      host_session_->Close();
      host_session_.reset();
    }
    if (client_session_.get()) {
      client_session_->Close();
      client_session_.reset();
    }
  }

  void CreateServerPair() {
    FilePath certs_dir;
    PathService::Get(base::DIR_SOURCE_ROOT, &certs_dir);
    certs_dir = certs_dir.AppendASCII("net");
    certs_dir = certs_dir.AppendASCII("data");
    certs_dir = certs_dir.AppendASCII("ssl");
    certs_dir = certs_dir.AppendASCII("certificates");

    FilePath cert_path = certs_dir.AppendASCII("unittest.selfsigned.der");
    std::string cert_der;
    ASSERT_TRUE(file_util::ReadFileToString(cert_path, &cert_der));

    FilePath key_path = certs_dir.AppendASCII("unittest.key.bin");
    std::string key_string;
    ASSERT_TRUE(file_util::ReadFileToString(key_path, &key_string));
    std::vector<uint8> key_vector(
        reinterpret_cast<const uint8*>(key_string.data()),
        reinterpret_cast<const uint8*>(key_string.data() +
                                       key_string.length()));
    scoped_ptr<crypto::RSAPrivateKey> private_key(
        crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(key_vector));

    host_signal_strategy_.reset(new FakeSignalStrategy(kHostJid));
    client_signal_strategy_.reset(new FakeSignalStrategy(kClientJid));
    FakeSignalStrategy::Connect(host_signal_strategy_.get(),
                                client_signal_strategy_.get());

    EXPECT_CALL(host_server_listener_, OnSessionManagerInitialized())
        .Times(1);
    host_server_.reset(new JingleSessionManager(
        base::MessageLoopProxy::current()));
    host_server_->set_allow_local_ips(true);
    host_server_->Init(
        kHostJid, host_signal_strategy_.get(), &host_server_listener_,
        private_key.release(), cert_der, false);

    EXPECT_CALL(client_server_listener_, OnSessionManagerInitialized())
        .Times(1);
    client_server_.reset(new JingleSessionManager(
        base::MessageLoopProxy::current()));
    client_server_->set_allow_local_ips(true);
    client_server_->Init(
        kClientJid, client_signal_strategy_.get(), &client_server_listener_,
        NULL, "", false);
  }

  void CloseSessionManager() {
    if (host_server_.get()) {
      host_server_->Close();
      host_server_.reset();
    }
    if (client_server_.get()) {
      client_server_->Close();
      client_server_.reset();
    }
    host_signal_strategy_.reset();
    client_signal_strategy_.reset();
  }

  bool InitiateConnection(const char* shared_secret) {
    int not_connected_peers = 2;

    EXPECT_CALL(host_server_listener_, OnIncomingSession(_, _))
        .WillOnce(DoAll(
            WithArg<0>(Invoke(
                this, &JingleSessionTest::SetHostSession)),
            SetArgumentPointee<1>(protocol::SessionManager::ACCEPT)));

    {
      InSequence dummy;

      if (shared_secret == kTestSharedSecret) {
        EXPECT_CALL(host_connection_callback_,
                    OnStateChange(Session::CONNECTED))
            .Times(1);
        EXPECT_CALL(host_connection_callback_,
                    OnStateChange(Session::CONNECTED_CHANNELS))
            .Times(1)
            .WillOnce(QuitThreadOnCounter(&not_connected_peers));
        // Expect that the connection will be closed eventually.
        EXPECT_CALL(host_connection_callback_,
                    OnStateChange(Session::CLOSED))
            .Times(AtMost(1));
      } else {
        // Might pass through the CONNECTED state.
        EXPECT_CALL(host_connection_callback_,
                    OnStateChange(Session::CONNECTED))
            .Times(AtMost(1));
        EXPECT_CALL(host_connection_callback_,
                    OnStateChange(Session::CONNECTED_CHANNELS))
            .Times(AtMost(1));
        // Expect that the connection will fail.
        EXPECT_CALL(host_connection_callback_,
                    OnStateChange(Session::FAILED))
            .Times(1)
            .WillOnce(InvokeWithoutArgs(&QuitCurrentThread));
      }
    }

    {
      InSequence dummy;

      EXPECT_CALL(client_connection_callback_,
                  OnStateChange(Session::CONNECTING))
          .Times(1);
      if (shared_secret == kTestSharedSecret) {
        EXPECT_CALL(client_connection_callback_,
                    OnStateChange(Session::CONNECTED))
            .Times(1);
        EXPECT_CALL(client_connection_callback_,
                    OnStateChange(Session::CONNECTED_CHANNELS))
            .Times(1)
            .WillOnce(QuitThreadOnCounter(&not_connected_peers));
      } else {
        EXPECT_CALL(client_connection_callback_,
                    OnStateChange(Session::CONNECTED))
            .Times(AtMost(1));
        EXPECT_CALL(client_connection_callback_,
                    OnStateChange(Session::CONNECTED_CHANNELS))
            .Times(AtMost(1));
      }
      // Expect that the connection will be closed eventually.
      EXPECT_CALL(client_connection_callback_,
                  OnStateChange(Session::CLOSED))
          .Times(AtMost(1));
    }

    client_session_.reset(client_server_->Connect(
        kHostJid, kTestHostPublicKey, kTestToken,
        CandidateSessionConfig::CreateDefault(),
        base::Bind(&MockSessionCallback::OnStateChange,
                   base::Unretained(&client_connection_callback_))));

    client_session_->set_shared_secret(shared_secret);

    return RunMessageLoopWithTimeout(TestTimeouts::action_max_timeout_ms());
  }

  static void DoNothing() { }

  JingleThreadMessageLoop message_loop_;

  scoped_ptr<FakeSignalStrategy> host_signal_strategy_;
  scoped_ptr<FakeSignalStrategy> client_signal_strategy_;

  scoped_ptr<JingleSessionManager> host_server_;
  MockSessionManagerListener host_server_listener_;
  scoped_ptr<JingleSessionManager> client_server_;
  MockSessionManagerListener client_server_listener_;

  scoped_ptr<Session> host_session_;
  MockSessionCallback host_connection_callback_;
  scoped_ptr<Session> client_session_;
  MockSessionCallback client_connection_callback_;
};

class ChannelTesterBase : public base::RefCountedThreadSafe<ChannelTesterBase> {
 public:
  ChannelTesterBase(Session* host_session,
                    Session* client_session)
      : host_session_(host_session),
        client_session_(client_session),
        done_(false) {
  }

  virtual ~ChannelTesterBase() { }

  void Start() {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&ChannelTesterBase::DoStart, this));
  }

  bool WaitFinished() {
    return RunMessageLoopWithTimeout(TestTimeouts::action_max_timeout_ms());
  }

  virtual void CheckResults() = 0;

 protected:
  void DoStart() {
    InitChannels();
  }

  virtual void InitChannels() = 0;

  void Done() {
    done_ = true;
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(&QuitCurrentThread));
  }

  virtual void InitBuffers() = 0;
  virtual void DoWrite() = 0;
  virtual void DoRead() = 0;

  Session* host_session_;
  Session* client_session_;
  scoped_ptr<net::Socket> sockets_[2];
  bool done_;
};

class TCPChannelTester : public ChannelTesterBase {
 public:
  TCPChannelTester(Session* host_session,
                   Session* client_session,
                   int message_size,
                   int message_count)
      : ChannelTesterBase(host_session, client_session),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            write_cb_(this, &TCPChannelTester::OnWritten)),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            read_cb_(this, &TCPChannelTester::OnRead)),
        write_errors_(0),
        read_errors_(0),
        message_size_(message_size),
        message_count_(message_count),
        test_data_size_(message_size * message_count) {
  }

  virtual ~TCPChannelTester() { }

  virtual void CheckResults() {
    EXPECT_EQ(0, write_errors_);
    EXPECT_EQ(0, read_errors_);

    ASSERT_EQ(test_data_size_, input_buffer_->offset());

    output_buffer_->SetOffset(0);
    ASSERT_EQ(test_data_size_, output_buffer_->size());

    EXPECT_EQ(0, memcmp(output_buffer_->data(),
                        input_buffer_->StartOfBuffer(), test_data_size_));
  }

 protected:
  virtual void InitChannels() OVERRIDE {
    host_session_->CreateStreamChannel(
        kChannelName,
        base::Bind(&TCPChannelTester::OnChannelReady,
                   base::Unretained(this), 0));
    client_session_->CreateStreamChannel(
        kChannelName,
        base::Bind(&TCPChannelTester::OnChannelReady,
                   base::Unretained(this), 1));
  }

  void OnChannelReady(int id, net::StreamSocket* socket) {
    ASSERT_TRUE(socket);
    if (!socket) {
      Done();
      return;
    }

    DCHECK(id >= 0 && id < 2);
    sockets_[id].reset(socket);

    if (sockets_[0].get() && sockets_[1].get()) {
      InitBuffers();
      DoRead();
      DoWrite();
    }
  }

  virtual void InitBuffers() {
    output_buffer_ = new net::DrainableIOBuffer(
        new net::IOBuffer(test_data_size_), test_data_size_);
    memset(output_buffer_->data(), 123, test_data_size_);

    input_buffer_ = new net::GrowableIOBuffer();
  }

  virtual void DoWrite() {
    int result = 1;
    while (result > 0) {
      if (output_buffer_->BytesRemaining() == 0)
        break;
      int bytes_to_write = std::min(output_buffer_->BytesRemaining(),
                                    message_size_);
      result = sockets_[0]->Write(output_buffer_, bytes_to_write, &write_cb_);
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
      Done();
    } else if (result > 0) {
      output_buffer_->DidConsume(result);
    }
  }

  virtual void DoRead() {
    int result = 1;
    while (result > 0) {
      input_buffer_->SetCapacity(input_buffer_->offset() + message_size_);
      result = sockets_[1]->Read(input_buffer_, message_size_, &read_cb_);
      HandleReadResult(result);
    };
  }

  void OnRead(int result) {
    HandleReadResult(result);
    if (!done_)
      DoRead();  // Don't try to read again when we are done reading.
  }

  void HandleReadResult(int result) {
    if (result <= 0 && result != net::ERR_IO_PENDING) {
      if (!done_) {
        LOG(ERROR) << "Received error " << result << " when trying to read";
        read_errors_++;
        Done();
      }
    } else if (result > 0) {
      // Allocate memory for the next read.
      input_buffer_->set_offset(input_buffer_->offset() + result);
      if (input_buffer_->offset() == test_data_size_)
        Done();
    }
  }

  scoped_refptr<net::DrainableIOBuffer> output_buffer_;
  scoped_refptr<net::GrowableIOBuffer> input_buffer_;

  net::OldCompletionCallbackImpl<TCPChannelTester> write_cb_;
  net::OldCompletionCallbackImpl<TCPChannelTester> read_cb_;
  int write_errors_;
  int read_errors_;
  int message_size_;
  int message_count_;
  int test_data_size_;
};

class ChannelSpeedTester : public TCPChannelTester {
 public:
  ChannelSpeedTester(Session* host_session,
                     Session* client_session,
                     int message_size)
      : TCPChannelTester(host_session, client_session, message_size, 1) {
    CHECK(message_size >= 8);
  }

  virtual ~ChannelSpeedTester() { }

  virtual void CheckResults() {
  }

  base::TimeDelta GetElapsedTime() {
    return base::Time::Now() - start_time_;
  }

 protected:
  virtual void InitBuffers() {
    TCPChannelTester::InitBuffers();

    start_time_ = base::Time::Now();
  }

  base::Time start_time_;
};

class UDPChannelTester : public ChannelTesterBase {
 public:
  UDPChannelTester(Session* host_session,
                   Session* client_session)
      : ChannelTesterBase(host_session, client_session),
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
  virtual void InitChannels() OVERRIDE {
    host_session_->CreateDatagramChannel(
        kChannelName,
        base::Bind(&UDPChannelTester::OnChannelReady,
                   base::Unretained(this), 0));
    client_session_->CreateDatagramChannel(
        kChannelName,
        base::Bind(&UDPChannelTester::OnChannelReady,
                   base::Unretained(this), 1));
  }

  void OnChannelReady(int id, net::Socket* socket) {
    ASSERT_TRUE(socket);
    if (!socket) {
      Done();
      return;
    }

    DCHECK(id >= 0 && id < 2);
    sockets_[id].reset(socket);

    if (sockets_[0].get() && sockets_[1].get()) {
      InitBuffers();
      DoRead();
      DoWrite();
    }
  }


  virtual void InitBuffers() {
  }

  virtual void DoWrite() {
    if (packets_sent_ >= kMessages) {
      Done();
      return;
    }

    scoped_refptr<net::IOBuffer> packet(new net::IOBuffer(kMessageSize));
    memset(packet->data(), 123, kMessageSize);
    sent_packets_[packets_sent_] = packet;
    // Put index of this packet in the beginning of the packet body.
    memcpy(packet->data(), &packets_sent_, sizeof(packets_sent_));

    int result = sockets_[0]->Write(packet, kMessageSize, &write_cb_);
    HandleWriteResult(result);
  }

  void OnWritten(int result) {
    HandleWriteResult(result);
  }

  void HandleWriteResult(int result) {
    if (result <= 0 && result != net::ERR_IO_PENDING) {
      LOG(ERROR) << "Received error " << result << " when trying to write";
      write_errors_++;
      Done();
    } else if (result > 0) {
      EXPECT_EQ(kMessageSize, result);
      packets_sent_++;
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE, base::Bind(&UDPChannelTester::DoWrite, this),
          kUdpWriteDelayMs);
    }
  }

  virtual void DoRead() {
    int result = 1;
    while (result > 0) {
      int kReadSize = kMessageSize * 2;
      read_buffer_ = new net::IOBuffer(kReadSize);

      result = sockets_[1]->Read(read_buffer_, kReadSize, &read_cb_);
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
      if (!done_) {
        LOG(ERROR) << "Received error " << result << " when trying to read";
        read_errors_++;
        Done();
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

  net::OldCompletionCallbackImpl<UDPChannelTester> write_cb_;
  net::OldCompletionCallbackImpl<UDPChannelTester> read_cb_;
  int write_errors_;
  int read_errors_;
  int packets_sent_;
  int packets_received_;
  int broken_packets_;
};

// Verify that we can create and destory server objects without a connection.
TEST_F(JingleSessionTest, CreateAndDestoy) {
  CreateServerPair();
}

// Verify that incoming session can be rejected, and that the status
// of the connection is set to CLOSED in this case.
TEST_F(JingleSessionTest, RejectConnection) {
  CreateServerPair();

  // Reject incoming session.
  EXPECT_CALL(host_server_listener_, OnIncomingSession(_, _))
      .WillOnce(SetArgumentPointee<1>(protocol::SessionManager::DECLINE));

  {
    InSequence dummy;

    EXPECT_CALL(client_connection_callback_,
                OnStateChange(Session::CONNECTING))
        .Times(1);
    EXPECT_CALL(client_connection_callback_,
                OnStateChange(Session::CLOSED))
        .Times(1)
        .WillOnce(InvokeWithoutArgs(&QuitCurrentThread));
  }

  client_session_.reset(client_server_->Connect(
      kHostJid, kTestHostPublicKey, kTestToken,
      CandidateSessionConfig::CreateDefault(),
      base::Bind(&MockSessionCallback::OnStateChange,
                 base::Unretained(&client_connection_callback_))));

  ASSERT_TRUE(RunMessageLoopWithTimeout(TestTimeouts::action_max_timeout_ms()));
}

// Verify that we can connect two endpoints.
TEST_F(JingleSessionTest, Connect) {
  CreateServerPair();
  ASSERT_TRUE(InitiateConnection(kTestSharedSecret));
}

// Verify that we can't connect two endpoints with mismatched secrets.
TEST_F(JingleSessionTest, ConnectBadChannelAuth) {
  CreateServerPair();
  ASSERT_TRUE(InitiateConnection(kTestSharedSecretBad));
}

// Verify that data can be transmitted over the event channel.
TEST_F(JingleSessionTest, TestTcpChannel) {
  CreateServerPair();
  ASSERT_TRUE(InitiateConnection(kTestSharedSecret));
  scoped_refptr<TCPChannelTester> tester(
      new TCPChannelTester(host_session_.get(), client_session_.get(),
                           kMessageSize, kMessages));
  tester->Start();
  ASSERT_TRUE(tester->WaitFinished());
  tester->CheckResults();

  // Connections must be closed while |tester| still exists.
  CloseSessions();
}

// Verify that data can be transmitted over the video RTP channel.
TEST_F(JingleSessionTest, TestUdpChannel) {
  CreateServerPair();
  ASSERT_TRUE(InitiateConnection(kTestSharedSecret));
  scoped_refptr<UDPChannelTester> tester(
      new UDPChannelTester(host_session_.get(), client_session_.get()));
  tester->Start();
  ASSERT_TRUE(tester->WaitFinished());
  tester->CheckResults();

  // Connections must be closed while |tester| still exists.
  CloseSessions();
}

// Send packets of different size to get the latency for sending data
// using sockets from JingleSession.
TEST_F(JingleSessionTest, FLAKY_TestSpeed) {
  CreateServerPair();
  ASSERT_TRUE(InitiateConnection(kTestSharedSecret));
  scoped_refptr<ChannelSpeedTester> tester;

  tester = new ChannelSpeedTester(host_session_.get(),
                                  client_session_.get(), 512);
  tester->Start();
  ASSERT_TRUE(tester->WaitFinished());
  LOG(INFO) << "Time for 512 bytes "
            << tester->GetElapsedTime().InMilliseconds() << " ms.";

  CloseSessions();
  ASSERT_TRUE(InitiateConnection(kTestSharedSecret));

  tester = new ChannelSpeedTester(host_session_.get(),
                                  client_session_.get(), 1024);
  tester->Start();
  ASSERT_TRUE(tester->WaitFinished());
  LOG(INFO) << "Time for 1024 bytes "
            << tester->GetElapsedTime().InMilliseconds() << " ms.";

  CloseSessions();
  ASSERT_TRUE(InitiateConnection(kTestSharedSecret));

  tester = new ChannelSpeedTester(host_session_.get(),
                                  client_session_.get(), 51200);
  tester->Start();
  ASSERT_TRUE(tester->WaitFinished());
  LOG(INFO) << "Time for 50k bytes "
            << tester->GetElapsedTime().InMilliseconds() << " ms.";

  CloseSessions();
  ASSERT_TRUE(InitiateConnection(kTestSharedSecret));

  tester = new ChannelSpeedTester(host_session_.get(),
                                  client_session_.get(), 512000);
  tester->Start();
  ASSERT_TRUE(tester->WaitFinished());
  LOG(INFO) << "Time for 500k bytes "
            << tester->GetElapsedTime().InMilliseconds() << " ms.";

  // Connections must be closed while |tester| still exists.
  CloseSessions();
}

}  // namespace protocol
}  // namespace remoting
