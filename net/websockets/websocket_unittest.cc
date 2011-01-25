// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/callback.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request_test_util.h"
#include "net/websockets/websocket.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

struct WebSocketEvent {
  enum EventType {
    EVENT_OPEN, EVENT_MESSAGE, EVENT_ERROR, EVENT_CLOSE,
  };

  WebSocketEvent(EventType type, net::WebSocket* websocket,
                 const std::string& websocket_msg, bool websocket_flag)
      : event_type(type), socket(websocket), msg(websocket_msg),
        flag(websocket_flag) {}

  EventType event_type;
  net::WebSocket* socket;
  std::string msg;
  bool flag;
};

class WebSocketEventRecorder : public net::WebSocketDelegate {
 public:
  explicit WebSocketEventRecorder(net::CompletionCallback* callback)
      : onopen_(NULL),
        onmessage_(NULL),
        onerror_(NULL),
        onclose_(NULL),
        callback_(callback) {}
  virtual ~WebSocketEventRecorder() {
    delete onopen_;
    delete onmessage_;
    delete onerror_;
    delete onclose_;
  }

  void SetOnOpen(Callback1<WebSocketEvent*>::Type* callback) {
    onopen_ = callback;
  }
  void SetOnMessage(Callback1<WebSocketEvent*>::Type* callback) {
    onmessage_ = callback;
  }
  void SetOnClose(Callback1<WebSocketEvent*>::Type* callback) {
    onclose_ = callback;
  }

  virtual void OnOpen(net::WebSocket* socket) {
    events_.push_back(
        WebSocketEvent(WebSocketEvent::EVENT_OPEN, socket,
                       std::string(), false));
    if (onopen_)
      onopen_->Run(&events_.back());
  }

  virtual void OnMessage(net::WebSocket* socket, const std::string& msg) {
    events_.push_back(
        WebSocketEvent(WebSocketEvent::EVENT_MESSAGE, socket, msg, false));
    if (onmessage_)
      onmessage_->Run(&events_.back());
  }
  virtual void OnError(net::WebSocket* socket) {
    events_.push_back(
        WebSocketEvent(WebSocketEvent::EVENT_ERROR, socket,
                       std::string(), false));
    if (onerror_)
      onerror_->Run(&events_.back());
  }
  virtual void OnClose(net::WebSocket* socket, bool was_clean) {
    events_.push_back(
        WebSocketEvent(WebSocketEvent::EVENT_CLOSE, socket,
                       std::string(), was_clean));
    if (onclose_)
      onclose_->Run(&events_.back());
    if (callback_)
      callback_->Run(net::OK);
  }

  void DoClose(WebSocketEvent* event) {
    event->socket->Close();
  }

  const std::vector<WebSocketEvent>& GetSeenEvents() const {
    return events_;
  }

 private:
  std::vector<WebSocketEvent> events_;
  Callback1<WebSocketEvent*>::Type* onopen_;
  Callback1<WebSocketEvent*>::Type* onmessage_;
  Callback1<WebSocketEvent*>::Type* onerror_;
  Callback1<WebSocketEvent*>::Type* onclose_;
  net::CompletionCallback* callback_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketEventRecorder);
};

namespace net {

class WebSocketTest : public PlatformTest {
 protected:
  void InitReadBuf(WebSocket* websocket) {
    // Set up |current_read_buf_|.
    websocket->current_read_buf_ = new GrowableIOBuffer();
  }
  void SetReadConsumed(WebSocket* websocket, int consumed) {
    websocket->read_consumed_len_ = consumed;
  }
  void AddToReadBuf(WebSocket* websocket, const char* data, int len) {
    websocket->AddToReadBuffer(data, len);
  }

  void TestProcessFrameData(WebSocket* websocket,
                            const char* expected_remaining_data,
                            int expected_remaining_len) {
    websocket->ProcessFrameData();

    const char* actual_remaining_data =
        websocket->current_read_buf_->StartOfBuffer()
        + websocket->read_consumed_len_;
    int actual_remaining_len =
        websocket->current_read_buf_->offset() - websocket->read_consumed_len_;

    EXPECT_EQ(expected_remaining_len, actual_remaining_len);
    EXPECT_TRUE(!memcmp(expected_remaining_data, actual_remaining_data,
                        expected_remaining_len));
  }
};

TEST_F(WebSocketTest, Connect) {
  MockClientSocketFactory mock_socket_factory;
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 101 Web Socket Protocol Handshake\r\n"
             "Upgrade: WebSocket\r\n"
             "Connection: Upgrade\r\n"
             "WebSocket-Origin: http://example.com\r\n"
             "WebSocket-Location: ws://example.com/demo\r\n"
             "WebSocket-Protocol: sample\r\n"
             "\r\n"),
    // Server doesn't close the connection after handshake.
    MockRead(true, ERR_IO_PENDING),
  };
  MockWrite data_writes[] = {
    MockWrite("GET /demo HTTP/1.1\r\n"
              "Upgrade: WebSocket\r\n"
              "Connection: Upgrade\r\n"
              "Host: example.com\r\n"
              "Origin: http://example.com\r\n"
              "WebSocket-Protocol: sample\r\n"
              "\r\n"),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  mock_socket_factory.AddSocketDataProvider(&data);
  MockHostResolver host_resolver;

  WebSocket::Request* request(
      new WebSocket::Request(GURL("ws://example.com/demo"),
                             "sample",
                             "http://example.com",
                             "ws://example.com/demo",
                             WebSocket::DRAFT75,
                             new TestURLRequestContext()));
  request->SetHostResolver(&host_resolver);
  request->SetClientSocketFactory(&mock_socket_factory);

  TestCompletionCallback callback;

  scoped_ptr<WebSocketEventRecorder> delegate(
      new WebSocketEventRecorder(&callback));
  delegate->SetOnOpen(NewCallback(delegate.get(),
                                  &WebSocketEventRecorder::DoClose));

  scoped_refptr<WebSocket> websocket(
      new WebSocket(request, delegate.get()));

  EXPECT_EQ(WebSocket::INITIALIZED, websocket->ready_state());
  websocket->Connect();

  callback.WaitForResult();

  const std::vector<WebSocketEvent>& events = delegate->GetSeenEvents();
  EXPECT_EQ(2U, events.size());

  EXPECT_EQ(WebSocketEvent::EVENT_OPEN, events[0].event_type);
  EXPECT_EQ(WebSocketEvent::EVENT_CLOSE, events[1].event_type);
}

TEST_F(WebSocketTest, ServerSentData) {
  MockClientSocketFactory mock_socket_factory;
  static const char kMessage[] = "Hello";
  static const char kFrame[] = "\x00Hello\xff";
  static const int kFrameLen = sizeof(kFrame) - 1;
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 101 Web Socket Protocol Handshake\r\n"
             "Upgrade: WebSocket\r\n"
             "Connection: Upgrade\r\n"
             "WebSocket-Origin: http://example.com\r\n"
             "WebSocket-Location: ws://example.com/demo\r\n"
             "WebSocket-Protocol: sample\r\n"
             "\r\n"),
    MockRead(true, kFrame, kFrameLen),
    // Server doesn't close the connection after handshake.
    MockRead(true, ERR_IO_PENDING),
  };
  MockWrite data_writes[] = {
    MockWrite("GET /demo HTTP/1.1\r\n"
              "Upgrade: WebSocket\r\n"
              "Connection: Upgrade\r\n"
              "Host: example.com\r\n"
              "Origin: http://example.com\r\n"
              "WebSocket-Protocol: sample\r\n"
              "\r\n"),
  };
  StaticSocketDataProvider data(data_reads, arraysize(data_reads),
                                data_writes, arraysize(data_writes));
  mock_socket_factory.AddSocketDataProvider(&data);
  MockHostResolver host_resolver;

  WebSocket::Request* request(
      new WebSocket::Request(GURL("ws://example.com/demo"),
                             "sample",
                             "http://example.com",
                             "ws://example.com/demo",
                             WebSocket::DRAFT75,
                             new TestURLRequestContext()));
  request->SetHostResolver(&host_resolver);
  request->SetClientSocketFactory(&mock_socket_factory);

  TestCompletionCallback callback;

  scoped_ptr<WebSocketEventRecorder> delegate(
      new WebSocketEventRecorder(&callback));
  delegate->SetOnMessage(NewCallback(delegate.get(),
                                     &WebSocketEventRecorder::DoClose));

  scoped_refptr<WebSocket> websocket(
      new WebSocket(request, delegate.get()));

  EXPECT_EQ(WebSocket::INITIALIZED, websocket->ready_state());
  websocket->Connect();

  callback.WaitForResult();

  const std::vector<WebSocketEvent>& events = delegate->GetSeenEvents();
  EXPECT_EQ(3U, events.size());

  EXPECT_EQ(WebSocketEvent::EVENT_OPEN, events[0].event_type);
  EXPECT_EQ(WebSocketEvent::EVENT_MESSAGE, events[1].event_type);
  EXPECT_EQ(kMessage, events[1].msg);
  EXPECT_EQ(WebSocketEvent::EVENT_CLOSE, events[2].event_type);
}

TEST_F(WebSocketTest, ProcessFrameDataForLengthCalculation) {
  WebSocket::Request* request(
      new WebSocket::Request(GURL("ws://example.com/demo"),
                             "sample",
                             "http://example.com",
                             "ws://example.com/demo",
                             WebSocket::DRAFT75,
                             new TestURLRequestContext()));
  TestCompletionCallback callback;
  scoped_ptr<WebSocketEventRecorder> delegate(
      new WebSocketEventRecorder(&callback));

  scoped_refptr<WebSocket> websocket(
      new WebSocket(request, delegate.get()));

  // Frame data: skip length 1 ('x'), and try to skip length 129
  // (1 * 128 + 1) bytes after \x81\x01, but buffer is too short to skip.
  static const char kTestLengthFrame[] =
      "\x80\x01x\x80\x81\x01\x01\x00unexpected data\xFF";
  const int kTestLengthFrameLength = sizeof(kTestLengthFrame) - 1;
  InitReadBuf(websocket.get());
  AddToReadBuf(websocket.get(), kTestLengthFrame, kTestLengthFrameLength);
  SetReadConsumed(websocket.get(), 0);

  static const char kExpectedRemainingFrame[] =
      "\x80\x81\x01\x01\x00unexpected data\xFF";
  const int kExpectedRemainingLength = sizeof(kExpectedRemainingFrame) - 1;
  TestProcessFrameData(websocket.get(),
                       kExpectedRemainingFrame, kExpectedRemainingLength);
  // No onmessage event expected.
  const std::vector<WebSocketEvent>& events = delegate->GetSeenEvents();
  EXPECT_EQ(1U, events.size());

  EXPECT_EQ(WebSocketEvent::EVENT_ERROR, events[0].event_type);

  websocket->DetachDelegate();
}

TEST_F(WebSocketTest, ProcessFrameDataForUnterminatedString) {
  WebSocket::Request* request(
      new WebSocket::Request(GURL("ws://example.com/demo"),
                             "sample",
                             "http://example.com",
                             "ws://example.com/demo",
                             WebSocket::DRAFT75,
                             new TestURLRequestContext()));
  TestCompletionCallback callback;
  scoped_ptr<WebSocketEventRecorder> delegate(
      new WebSocketEventRecorder(&callback));

  scoped_refptr<WebSocket> websocket(
      new WebSocket(request, delegate.get()));

  static const char kTestUnterminatedFrame[] =
      "\x00unterminated frame";
  const int kTestUnterminatedFrameLength = sizeof(kTestUnterminatedFrame) - 1;
  InitReadBuf(websocket.get());
  AddToReadBuf(websocket.get(), kTestUnterminatedFrame,
               kTestUnterminatedFrameLength);
  SetReadConsumed(websocket.get(), 0);
  TestProcessFrameData(websocket.get(),
                       kTestUnterminatedFrame, kTestUnterminatedFrameLength);
  {
    // No onmessage event expected.
    const std::vector<WebSocketEvent>& events = delegate->GetSeenEvents();
    EXPECT_EQ(0U, events.size());
  }

  static const char kTestTerminateFrame[] = " is terminated in next read\xff";
  const int kTestTerminateFrameLength = sizeof(kTestTerminateFrame) - 1;
  AddToReadBuf(websocket.get(), kTestTerminateFrame,
               kTestTerminateFrameLength);
  TestProcessFrameData(websocket.get(), "", 0);

  static const char kExpectedMsg[] =
      "unterminated frame is terminated in next read";
  {
    const std::vector<WebSocketEvent>& events = delegate->GetSeenEvents();
    EXPECT_EQ(1U, events.size());

    EXPECT_EQ(WebSocketEvent::EVENT_MESSAGE, events[0].event_type);
    EXPECT_EQ(kExpectedMsg, events[0].msg);
  }

  websocket->DetachDelegate();
}

}  // namespace net
