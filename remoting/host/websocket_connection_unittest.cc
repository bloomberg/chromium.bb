// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/websocket_connection.h"

#include "base/message_loop.h"
#include "base/run_loop.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/socket/tcp_client_socket.h"
#include "remoting/base/socket_reader.h"
#include "remoting/host/websocket_listener.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

const int kPortRangeMin = 12800;
const int kPortRangeMax = 12810;
const char kHeaderEndMarker[] = "\r\n\r\n";

const uint8 kHelloMessage[] = { 0x81, 0x85, 0x37, 0xfa, 0x21, 0x3d, 0x7f,
                                0x9f, 0x4d, 0x51, 0x58 };
const char kHelloText[] = "Hello";

}  // namespace

class WebSocketTestReader {
 public:
  WebSocketTestReader(net::Socket* socket, const base::Closure& on_data)
      : on_data_(on_data),
        closed_(false),
        reading_header_(true) {
    reader_.Init(socket, base::Bind(&WebSocketTestReader::OnReadResult,
                                    base::Unretained(this)));
  }
  ~WebSocketTestReader() {
  }

  bool closed() { return closed_; }

  bool reading_header() { return reading_header_; }
  const std::string& header() { return header_; }
  const std::string& data_receved() { return data_receved_; }

 protected:
  void OnReadResult(scoped_refptr<net::IOBuffer> buffer, int result) {
    if (result <= 0) {
      closed_ = true;
    } else if (reading_header_) {
      header_.append(buffer->data(), buffer->data() + result);
      size_t end_pos = header_.find(kHeaderEndMarker);
      if (end_pos != std::string::npos) {
        data_receved_ = header_.substr(end_pos + strlen(kHeaderEndMarker));
        header_ = header_.substr(0, end_pos);
        reading_header_ = false;
      }
    } else {
      data_receved_.append(buffer->data(), buffer->data() + result);
    }
    on_data_.Run();
  }

 private:
  SocketReader reader_;
  base::Closure on_data_;
  bool closed_;
  bool reading_header_;
  std::string header_;
  std::string data_receved_;
};

class WebSocketConnectionTest : public testing::Test,
                                public WebSocketConnection::Delegate {
 public:
  virtual void OnWebSocketMessage(const std::string& message) OVERRIDE {
    last_message_ = message;
    if (message_run_loop_.get()) {
      message_run_loop_->Quit();
    }
  }

  virtual void OnWebSocketClosed() OVERRIDE {
    connection_.reset();
    if (closed_run_loop_.get()) {
      closed_run_loop_->Quit();
    }
  }

 protected:
  void Initialize() {
    listener_.reset(new WebSocketListener());
    net::IPAddressNumber localhost;
    ASSERT_TRUE(net::ParseIPLiteralToNumber("127.0.0.1", &localhost));
    for (int port = kPortRangeMin; port < kPortRangeMax; ++port) {
      endpoint_ = net::IPEndPoint(localhost, port);
      if (listener_->Listen(
              endpoint_, base::Bind(&WebSocketConnectionTest::OnNewConnection,
                                    base::Unretained(this))) == net::OK) {
        return;
      }
    }
  }

  void OnNewConnection(scoped_ptr<WebSocketConnection> connection) {
    EXPECT_TRUE(connection_.get() == NULL);
    connection_ = connection.Pass();
    connection_->Accept(this);
  }

  void ConnectSocket() {
    client_.reset(new net::TCPClientSocket(
        net::AddressList(endpoint_), NULL, net::NetLog::Source()));
    client_connect_result_ = -1;
    client_->Connect(base::Bind(&WebSocketConnectionTest::OnClientConnected,
                                base::Unretained(this)));
    connect_run_loop_.reset(new base::RunLoop());
    connect_run_loop_->Run();
    ASSERT_EQ(client_connect_result_, 0);
    client_writer_.reset(new protocol::BufferedSocketWriter());
    client_writer_->Init(
        client_.get(), protocol::BufferedSocketWriter::WriteFailedCallback());
    client_reader_.reset(new WebSocketTestReader(
        client_.get(),
        base::Bind(&WebSocketConnectionTest::OnClientDataReceived,
                   base::Unretained(this))));
  }

  void OnClientConnected(int result) {
    client_connect_result_ = result;
    connect_run_loop_->Quit();
  }

  void OnClientDataReceived() {
    if (handshake_run_loop_.get() && !client_reader_->reading_header()) {
      handshake_run_loop_->Quit();
    }
    if (closed_run_loop_.get() && client_reader_->closed()) {
      closed_run_loop_->Quit();
    }
    if (data_received_run_loop_.get()) {
      data_received_run_loop_->Quit();
    }
  }

  void Send(const std::string& data) {
    scoped_refptr<net::IOBufferWithSize> buffer =
        new net::IOBufferWithSize(data.size());
    memcpy(buffer->data(), data.data(), data.size());
    client_writer_->Write(buffer, base::Closure());
  }

  void Handshake() {
    Send("GET /chat HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Origin: http://example.com\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n");
    handshake_run_loop_.reset(new base::RunLoop());
    handshake_run_loop_->Run();
    EXPECT_EQ("http://example.com", connection_->origin());
    EXPECT_EQ("server.example.com", connection_->request_host());
    EXPECT_EQ("/chat", connection_->request_path());
    EXPECT_EQ("HTTP/1.1 101 Switching Protocol\r\n"
              "Upgrade: websocket\r\n"
              "Connection: Upgrade\r\n"
              "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=",
              client_reader_->header());
  }

  void ReceiveFrame(std::string expected_frame) {
    while (client_reader_->data_receved().size() < expected_frame.size()) {
      data_received_run_loop_.reset(new base::RunLoop());
      data_received_run_loop_->Run();
    }
    EXPECT_EQ(expected_frame, client_reader_->data_receved());
  }

  MessageLoopForIO message_loop_;

  scoped_ptr<base::RunLoop> connect_run_loop_;
  scoped_ptr<base::RunLoop> handshake_run_loop_;
  scoped_ptr<base::RunLoop> closed_run_loop_;
  scoped_ptr<base::RunLoop> data_received_run_loop_;
  scoped_ptr<base::RunLoop> message_run_loop_;

  scoped_ptr<WebSocketListener> listener_;
  net::IPEndPoint endpoint_;
  scoped_ptr<WebSocketConnection> connection_;
  scoped_ptr<net::TCPClientSocket> client_;
  scoped_ptr<protocol::BufferedSocketWriter> client_writer_;
  scoped_ptr<WebSocketTestReader> client_reader_;
  int client_connect_result_;
  std::string last_message_;
};

TEST_F(WebSocketConnectionTest, ConnectSocket) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  ASSERT_NO_FATAL_FAILURE(ConnectSocket());
}

TEST_F(WebSocketConnectionTest, SuccessfulHandshake) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  ASSERT_NO_FATAL_FAILURE(ConnectSocket());
  ASSERT_NO_FATAL_FAILURE(Handshake());
  ASSERT_TRUE(connection_.get() != NULL);
}

TEST_F(WebSocketConnectionTest, ConnectAndReconnect) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  ASSERT_NO_FATAL_FAILURE(ConnectSocket());
  ASSERT_NO_FATAL_FAILURE(Handshake());
  ASSERT_TRUE(connection_.get() != NULL);

  client_.reset();
  closed_run_loop_.reset(new base::RunLoop());
  closed_run_loop_->Run();
  EXPECT_TRUE(connection_.get() == NULL);

  ASSERT_NO_FATAL_FAILURE(ConnectSocket());
  ASSERT_NO_FATAL_FAILURE(Handshake());
  ASSERT_TRUE(connection_.get() != NULL);
}

TEST_F(WebSocketConnectionTest, CloseFrameIsSent) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  ASSERT_NO_FATAL_FAILURE(ConnectSocket());
  ASSERT_NO_FATAL_FAILURE(Handshake());

  connection_->Close();

  // Expect to receive Close frame.
  uint8 expected_frame[] = { 0x88, 0x00 };
  ReceiveFrame(std::string(expected_frame,
                           expected_frame + sizeof(expected_frame)));
}

TEST_F(WebSocketConnectionTest, ConnectFailsOnNonWebSocketHeader) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  ASSERT_NO_FATAL_FAILURE(ConnectSocket());
  EXPECT_TRUE(connection_.get() == NULL);
  Send("GET /chat HTTP/1.1\r\n"
       "Host: server.example.com\r\n\r\n");
  closed_run_loop_.reset(new base::RunLoop());
  closed_run_loop_->Run();
  EXPECT_TRUE(connection_.get() == NULL);
}

TEST_F(WebSocketConnectionTest, SendMessage) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  ASSERT_NO_FATAL_FAILURE(ConnectSocket());
  ASSERT_NO_FATAL_FAILURE(Handshake());
  connection_->SendText(kHelloText);
  uint8 expected_frame[] = { 0x81, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f };
  ReceiveFrame(std::string(expected_frame,
                           expected_frame + sizeof(expected_frame)));
}

TEST_F(WebSocketConnectionTest, ReceiveMessage) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  ASSERT_NO_FATAL_FAILURE(ConnectSocket());
  ASSERT_NO_FATAL_FAILURE(Handshake());
  uint8 kHelloMessage[] = { 0x81, 0x85, 0x37, 0xfa, 0x21, 0x3d, 0x7f,
                            0x9f, 0x4d, 0x51, 0x58 };
  Send(std::string(kHelloMessage, kHelloMessage + sizeof(kHelloMessage)));

  message_run_loop_.reset(new base::RunLoop());
  message_run_loop_->Run();

  EXPECT_EQ(kHelloText, last_message_);
}

TEST_F(WebSocketConnectionTest, FragmentedMessage) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  ASSERT_NO_FATAL_FAILURE(ConnectSocket());
  ASSERT_NO_FATAL_FAILURE(Handshake());

  uint8 fragment1[] = { 0x01, 0x83, 0x37, 0xfa, 0x21, 0x3d, 0x7f, 0x9f, 0x4d };
  Send(std::string(fragment1, fragment1 + sizeof(fragment1)));
  uint8 fragment2[] = { 0x80, 0x82, 0x3d, 0x37, 0x12, 0x42, 0x51, 0x58 };
  Send(std::string(fragment2, fragment2 + sizeof(fragment2)));

  message_run_loop_.reset(new base::RunLoop());
  message_run_loop_->Run();

  EXPECT_EQ(kHelloText, last_message_);
}

TEST_F(WebSocketConnectionTest, PingResponse) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  ASSERT_NO_FATAL_FAILURE(ConnectSocket());
  ASSERT_NO_FATAL_FAILURE(Handshake());

  uint8 ping_frame[] = { 0x89, 0x85, 0x37, 0xfa, 0x21, 0x3d, 0x7f, 0x9f,
                        0x4d, 0x51, 0x58 };
  Send(std::string(ping_frame, ping_frame + sizeof(ping_frame)));

  uint8 expected_frame[] = { 0x8a, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f };
  ReceiveFrame(std::string(expected_frame,
                           expected_frame + sizeof(expected_frame)));
}

TEST_F(WebSocketConnectionTest, MessageSizeLimit) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  ASSERT_NO_FATAL_FAILURE(ConnectSocket());
  ASSERT_NO_FATAL_FAILURE(Handshake());

  connection_->set_maximum_message_size(strlen(kHelloText));
  Send(std::string(kHelloMessage, kHelloMessage + sizeof(kHelloMessage)));

  message_run_loop_.reset(new base::RunLoop());
  message_run_loop_->Run();

  EXPECT_EQ(kHelloText, last_message_);

  // Set lower message size limit and try sending the same message again.
  connection_->set_maximum_message_size(strlen(kHelloText) - 1);

  Send(std::string(kHelloMessage, kHelloMessage + sizeof(kHelloMessage)));

  // Expect to receive Close frame.
  uint8 expected_frame[] = { 0x88, 0x00 };
  ReceiveFrame(std::string(expected_frame,
                           expected_frame + sizeof(expected_frame)));
}

}  // namespace remoting
