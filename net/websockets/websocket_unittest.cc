// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/task.h"
#include "net/base/completion_callback.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request_unittest.h"
#include "net/websockets/websocket.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

struct WebSocketEvent {
  enum EventType {
    EVENT_OPEN, EVENT_MESSAGE, EVENT_CLOSE,
  };

  WebSocketEvent(EventType type, net::WebSocket* websocket,
                 const std::string& websocket_msg)
      : event_type(type), socket(websocket), msg(websocket_msg) {}

  EventType event_type;
  net::WebSocket* socket;
  std::string msg;
};

class WebSocketEventRecorder : public net::WebSocket::Delegate {
 public:
  explicit WebSocketEventRecorder(net::CompletionCallback* callback)
      : onopen_(NULL),
        onmessage_(NULL),
        onclose_(NULL),
        callback_(callback) {}
  virtual ~WebSocketEventRecorder() {
    delete onopen_;
    delete onmessage_;
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
        WebSocketEvent(WebSocketEvent::EVENT_OPEN, socket, std::string()));
    if (onopen_)
      onopen_->Run(&events_.back());
  }

  virtual void OnMessage(net::WebSocket* socket, const std::string& msg) {
    events_.push_back(
        WebSocketEvent(WebSocketEvent::EVENT_MESSAGE, socket, msg));
    if (onmessage_)
      onmessage_->Run(&events_.back());
  }
  virtual void OnClose(net::WebSocket* socket) {
    events_.push_back(
        WebSocketEvent(WebSocketEvent::EVENT_CLOSE, socket, std::string()));
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
  Callback1<WebSocketEvent*>::Type* onclose_;
  net::CompletionCallback* callback_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketEventRecorder);
};

class WebSocketTest : public PlatformTest {
};

TEST_F(WebSocketTest, Connect) {
  net::MockClientSocketFactory mock_socket_factory;
  net::MockRead data_reads[] = {
    net::MockRead("HTTP/1.1 101 Web Socket Protocol\r\n"
                  "Upgrade: WebSocket\r\n"
                  "Connection: Upgrade\r\n"
                  "WebSocket-Origin: http://example.com\r\n"
                  "WebSocket-Location: ws://example.com/demo\r\n"
                  "WebSocket-Protocol: sample\r\n"
                  "\r\n"),
    // Server doesn't close the connection after handshake.
    net::MockRead(true, net::ERR_IO_PENDING),
  };
  net::MockWrite data_writes[] = {
    net::MockWrite("GET /demo HTTP/1.1\r\n"
                   "Upgrade: WebSocket\r\n"
                   "Connection: Upgrade\r\n"
                   "Host: example.com\r\n"
                   "Origin: http://example.com\r\n"
                   "WebSocket-Protocol: sample\r\n"
                   "\r\n"),
  };
  net::StaticMockSocket data(data_reads, data_writes);
  mock_socket_factory.AddMockSocket(&data);

  net::WebSocket::Request* request(
      new net::WebSocket::Request(GURL("ws://example.com/demo"),
                                  "sample",
                                  "http://example.com",
                                  "ws://example.com/demo",
                                  new TestURLRequestContext()));
  request->SetHostResolver(new net::MockHostResolver());
  request->SetClientSocketFactory(&mock_socket_factory);

  TestCompletionCallback callback;

  scoped_ptr<WebSocketEventRecorder> delegate(
      new WebSocketEventRecorder(&callback));
  delegate->SetOnOpen(NewCallback(delegate.get(),
                                  &WebSocketEventRecorder::DoClose));

  scoped_refptr<net::WebSocket> websocket(
      new net::WebSocket(request, delegate.get()));

  EXPECT_EQ(net::WebSocket::INITIALIZED, websocket->ready_state());
  websocket->Connect();

  callback.WaitForResult();

  const std::vector<WebSocketEvent>& events = delegate->GetSeenEvents();
  EXPECT_EQ(2U, events.size());

  EXPECT_EQ(WebSocketEvent::EVENT_OPEN, events[0].event_type);
  EXPECT_EQ(WebSocketEvent::EVENT_CLOSE, events[1].event_type);
}

TEST_F(WebSocketTest, ServerSentData) {
  net::MockClientSocketFactory mock_socket_factory;
  static const char kMessage[] = "Hello";
  static const char kFrame[] = "\x00Hello\xff";
  static const int kFrameLen = sizeof(kFrame) - 1;
  net::MockRead data_reads[] = {
    net::MockRead("HTTP/1.1 101 Web Socket Protocol\r\n"
                  "Upgrade: WebSocket\r\n"
                  "Connection: Upgrade\r\n"
                  "WebSocket-Origin: http://example.com\r\n"
                  "WebSocket-Location: ws://example.com/demo\r\n"
                  "WebSocket-Protocol: sample\r\n"
                  "\r\n"),
    net::MockRead(true, kFrame, kFrameLen),
    // Server doesn't close the connection after handshake.
    net::MockRead(true, net::ERR_IO_PENDING),
  };
  net::MockWrite data_writes[] = {
    net::MockWrite("GET /demo HTTP/1.1\r\n"
                   "Upgrade: WebSocket\r\n"
                   "Connection: Upgrade\r\n"
                   "Host: example.com\r\n"
                   "Origin: http://example.com\r\n"
                   "WebSocket-Protocol: sample\r\n"
                   "\r\n"),
  };
  net::StaticMockSocket data(data_reads, data_writes);
  mock_socket_factory.AddMockSocket(&data);

  net::WebSocket::Request* request(
      new net::WebSocket::Request(GURL("ws://example.com/demo"),
                                  "sample",
                                  "http://example.com",
                                  "ws://example.com/demo",
                                  new TestURLRequestContext()));
  request->SetHostResolver(new net::MockHostResolver());
  request->SetClientSocketFactory(&mock_socket_factory);

  TestCompletionCallback callback;

  scoped_ptr<WebSocketEventRecorder> delegate(
      new WebSocketEventRecorder(&callback));
  delegate->SetOnMessage(NewCallback(delegate.get(),
                                     &WebSocketEventRecorder::DoClose));

  scoped_refptr<net::WebSocket> websocket(
      new net::WebSocket(request, delegate.get()));

  EXPECT_EQ(net::WebSocket::INITIALIZED, websocket->ready_state());
  websocket->Connect();

  callback.WaitForResult();

  const std::vector<WebSocketEvent>& events = delegate->GetSeenEvents();
  EXPECT_EQ(3U, events.size());

  EXPECT_EQ(WebSocketEvent::EVENT_OPEN, events[0].event_type);
  EXPECT_EQ(WebSocketEvent::EVENT_MESSAGE, events[1].event_type);
  EXPECT_EQ(kMessage, events[1].msg);
  EXPECT_EQ(WebSocketEvent::EVENT_CLOSE, events[2].event_type);
}
