// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/websockets/DOMWebSocket.h"

#include <memory>

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/DOMTypedArray.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/SecurityContext.h"
#include "core/fileapi/Blob.h"
#include "core/inspector/ConsoleTypes.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;
using testing::Ref;
using testing::Return;

namespace blink {

namespace {

typedef testing::StrictMock<testing::MockFunction<void(int)>>
    Checkpoint;  // NOLINT

class MockWebSocketChannel : public WebSocketChannel {
 public:
  static MockWebSocketChannel* Create() {
    return new testing::StrictMock<MockWebSocketChannel>();
  }

  ~MockWebSocketChannel() override {}

  MOCK_METHOD2(Connect, bool(const KURL&, const String&));
  MOCK_METHOD1(Send, void(const CString&));
  MOCK_METHOD3(Send, void(const DOMArrayBuffer&, unsigned, unsigned));
  MOCK_METHOD1(SendMock, void(BlobDataHandle*));
  void Send(PassRefPtr<BlobDataHandle> handle) { SendMock(handle.Get()); }
  MOCK_METHOD1(SendTextAsCharVectorMock, void(Vector<char>*));
  void SendTextAsCharVector(std::unique_ptr<Vector<char>> vector) {
    SendTextAsCharVectorMock(vector.get());
  }
  MOCK_METHOD1(SendBinaryAsCharVectorMock, void(Vector<char>*));
  void SendBinaryAsCharVector(std::unique_ptr<Vector<char>> vector) {
    SendBinaryAsCharVectorMock(vector.get());
  }
  MOCK_CONST_METHOD0(BufferedAmount, unsigned());
  MOCK_METHOD2(Close, void(int, const String&));
  MOCK_METHOD3(FailMock, void(const String&, MessageLevel, SourceLocation*));
  void Fail(const String& reason,
            MessageLevel level,
            std::unique_ptr<SourceLocation> location) {
    FailMock(reason, level, location.get());
  }
  MOCK_METHOD0(Disconnect, void());

  MockWebSocketChannel() {}
};

class DOMWebSocketWithMockChannel final : public DOMWebSocket {
 public:
  static DOMWebSocketWithMockChannel* Create(ExecutionContext* context) {
    DOMWebSocketWithMockChannel* websocket =
        new DOMWebSocketWithMockChannel(context);
    websocket->SuspendIfNeeded();
    return websocket;
  }

  MockWebSocketChannel* Channel() { return channel_.Get(); }

  WebSocketChannel* CreateChannel(ExecutionContext*,
                                  WebSocketChannelClient*) override {
    DCHECK(!has_created_channel_);
    has_created_channel_ = true;
    return channel_.Get();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(channel_);
    DOMWebSocket::Trace(visitor);
  }

 private:
  explicit DOMWebSocketWithMockChannel(ExecutionContext* context)
      : DOMWebSocket(context),
        channel_(MockWebSocketChannel::Create()),
        has_created_channel_(false) {}

  Member<MockWebSocketChannel> channel_;
  bool has_created_channel_;
};

class DOMWebSocketTestScope {
 public:
  explicit DOMWebSocketTestScope(ExecutionContext* execution_context)
      : websocket_(DOMWebSocketWithMockChannel::Create(execution_context)) {}

  ~DOMWebSocketTestScope() {
    if (!websocket_)
      return;
    // These statements are needed to clear WebSocket::m_channel to
    // avoid ASSERTION failure on ~DOMWebSocket.
    DCHECK(Socket().Channel());
    ::testing::Mock::VerifyAndClear(Socket().Channel());
    EXPECT_CALL(Channel(), Disconnect()).Times(AnyNumber());

    Socket().DidClose(WebSocketChannelClient::kClosingHandshakeIncomplete, 1006,
                      "");
  }

  MockWebSocketChannel& Channel() { return *websocket_->Channel(); }
  DOMWebSocketWithMockChannel& Socket() { return *websocket_.Get(); }

 private:
  Persistent<DOMWebSocketWithMockChannel> websocket_;
};

TEST(DOMWebSocketTest, connectToBadURL) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  web_socket_scope.Socket().Connect("xxx", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(kSyntaxError, scope.GetExceptionState().Code());
  EXPECT_EQ("The URL 'xxx' is invalid.", scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kClosed, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, connectToNonWsURL) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  web_socket_scope.Socket().Connect("http://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(kSyntaxError, scope.GetExceptionState().Code());
  EXPECT_EQ(
      "The URL's scheme must be either 'ws' or 'wss'. 'http' is not allowed.",
      scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kClosed, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, connectToURLHavingFragmentIdentifier) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  web_socket_scope.Socket().Connect("ws://example.com/#fragment",
                                    Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(kSyntaxError, scope.GetExceptionState().Code());
  EXPECT_EQ(
      "The URL contains a fragment identifier ('fragment'). Fragment "
      "identifiers are not allowed in WebSocket URLs.",
      scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kClosed, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, invalidPort) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  web_socket_scope.Socket().Connect("ws://example.com:7", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(kSecurityError, scope.GetExceptionState().Code());
  EXPECT_EQ("The port 7 is not allowed.", scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kClosed, web_socket_scope.Socket().readyState());
}

// FIXME: Add a test for Content Security Policy.

TEST(DOMWebSocketTest, invalidSubprotocols) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  Vector<String> subprotocols;
  subprotocols.push_back("@subprotocol-|'\"x\x01\x02\x03x");

  web_socket_scope.Socket().Connect("ws://example.com/", subprotocols,
                                    scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(kSyntaxError, scope.GetExceptionState().Code());
  EXPECT_EQ(
      "The subprotocol '@subprotocol-|'\"x\\u0001\\u0002\\u0003x' is invalid.",
      scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kClosed, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, insecureRequestsUpgrade) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "wss://example.com/endpoint"), String()))
        .WillOnce(Return(true));
  }

  scope.GetDocument().SetInsecureRequestPolicy(kUpgradeInsecureRequests);
  web_socket_scope.Socket().Connect(
      "ws://example.com/endpoint", Vector<String>(), scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, web_socket_scope.Socket().readyState());
  EXPECT_EQ(KURL(KURL(), "wss://example.com/endpoint"),
            web_socket_scope.Socket().url());
}

TEST(DOMWebSocketTest, insecureRequestsDoNotUpgrade) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/endpoint"), String()))
        .WillOnce(Return(true));
  }

  scope.GetDocument().SetInsecureRequestPolicy(kLeaveInsecureRequestsAlone);
  web_socket_scope.Socket().Connect(
      "ws://example.com/endpoint", Vector<String>(), scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, web_socket_scope.Socket().readyState());
  EXPECT_EQ(KURL(KURL(), "ws://example.com/endpoint"),
            web_socket_scope.Socket().url());
}

TEST(DOMWebSocketTest, channelConnectSuccess) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  Vector<String> subprotocols;
  subprotocols.push_back("aa");
  subprotocols.push_back("bb");

  {
    InSequence s;
    EXPECT_CALL(
        web_socket_scope.Channel(),
        Connect(KURL(KURL(), "ws://example.com/hoge"), String("aa, bb")))
        .WillOnce(Return(true));
  }

  web_socket_scope.Socket().Connect("ws://example.com/hoge",
                                    Vector<String>(subprotocols),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, web_socket_scope.Socket().readyState());
  EXPECT_EQ(KURL(KURL(), "ws://example.com/hoge"),
            web_socket_scope.Socket().url());
}

TEST(DOMWebSocketTest, channelConnectFail) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  Vector<String> subprotocols;
  subprotocols.push_back("aa");
  subprotocols.push_back("bb");

  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String("aa, bb")))
        .WillOnce(Return(false));
    EXPECT_CALL(web_socket_scope.Channel(), Disconnect());
  }

  web_socket_scope.Socket().Connect("ws://example.com/",
                                    Vector<String>(subprotocols),
                                    scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(kSecurityError, scope.GetExceptionState().Code());
  EXPECT_EQ(
      "An insecure WebSocket connection may not be initiated from a page "
      "loaded over HTTPS.",
      scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kClosed, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, isValidSubprotocolString) {
  EXPECT_TRUE(DOMWebSocket::IsValidSubprotocolString("Helloworld!!"));
  EXPECT_FALSE(DOMWebSocket::IsValidSubprotocolString("Hello, world!!"));
  EXPECT_FALSE(DOMWebSocket::IsValidSubprotocolString(String()));
  EXPECT_FALSE(DOMWebSocket::IsValidSubprotocolString(""));

  const char kValidCharacters[] =
      "!#$%&'*+-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ^_`"
      "abcdefghijklmnopqrstuvwxyz|~";
  size_t length = strlen(kValidCharacters);
  for (size_t i = 0; i < length; ++i) {
    String s;
    s.append(static_cast<UChar>(kValidCharacters[i]));
    EXPECT_TRUE(DOMWebSocket::IsValidSubprotocolString(s));
  }
  for (size_t i = 0; i < 256; ++i) {
    if (std::find(kValidCharacters, kValidCharacters + length,
                  static_cast<char>(i)) != kValidCharacters + length) {
      continue;
    }
    String s;
    s.append(static_cast<UChar>(i));
    EXPECT_FALSE(DOMWebSocket::IsValidSubprotocolString(s));
  }
}

TEST(DOMWebSocketTest, connectSuccess) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  Vector<String> subprotocols;
  subprotocols.push_back("aa");
  subprotocols.push_back("bb");
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String("aa, bb")))
        .WillOnce(Return(true));
  }
  web_socket_scope.Socket().Connect("ws://example.com/", subprotocols,
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, web_socket_scope.Socket().readyState());

  web_socket_scope.Socket().DidConnect("bb", "cc");

  EXPECT_EQ(DOMWebSocket::kOpen, web_socket_scope.Socket().readyState());
  EXPECT_EQ("bb", web_socket_scope.Socket().protocol());
  EXPECT_EQ("cc", web_socket_scope.Socket().extensions());
}

TEST(DOMWebSocketTest, didClose) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(web_socket_scope.Channel(), Disconnect());
  }
  web_socket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, web_socket_scope.Socket().readyState());

  web_socket_scope.Socket().DidClose(
      WebSocketChannelClient::kClosingHandshakeIncomplete, 1006, "");

  EXPECT_EQ(DOMWebSocket::kClosed, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, maximumReasonSize) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(web_socket_scope.Channel(), FailMock(_, _, _));
  }
  StringBuilder reason;
  for (size_t i = 0; i < 123; ++i)
    reason.Append('a');
  web_socket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, web_socket_scope.Socket().readyState());

  web_socket_scope.Socket().close(1000, reason.ToString(),
                                  scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, reasonSizeExceeding) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String()))
        .WillOnce(Return(true));
  }
  StringBuilder reason;
  for (size_t i = 0; i < 124; ++i)
    reason.Append('a');
  web_socket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, web_socket_scope.Socket().readyState());

  web_socket_scope.Socket().close(1000, reason.ToString(),
                                  scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(kSyntaxError, scope.GetExceptionState().Code());
  EXPECT_EQ("The message must not be greater than 123 bytes.",
            scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kConnecting, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, closeWhenConnecting) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(
        web_socket_scope.Channel(),
        FailMock(
            String("WebSocket is closed before the connection is established."),
            kWarningMessageLevel, _));
  }
  web_socket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, web_socket_scope.Socket().readyState());

  web_socket_scope.Socket().close(1000, "bye", scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, close) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(web_socket_scope.Channel(), Close(3005, String("bye")));
  }
  web_socket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, web_socket_scope.Socket().readyState());

  web_socket_scope.Socket().DidConnect("", "");
  EXPECT_EQ(DOMWebSocket::kOpen, web_socket_scope.Socket().readyState());
  web_socket_scope.Socket().close(3005, "bye", scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, closeWithoutReason) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(web_socket_scope.Channel(), Close(3005, String()));
  }
  web_socket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, web_socket_scope.Socket().readyState());

  web_socket_scope.Socket().DidConnect("", "");
  EXPECT_EQ(DOMWebSocket::kOpen, web_socket_scope.Socket().readyState());
  web_socket_scope.Socket().close(3005, scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, closeWithoutCodeAndReason) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(web_socket_scope.Channel(), Close(-1, String()));
  }
  web_socket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, web_socket_scope.Socket().readyState());

  web_socket_scope.Socket().DidConnect("", "");
  EXPECT_EQ(DOMWebSocket::kOpen, web_socket_scope.Socket().readyState());
  web_socket_scope.Socket().close(scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, closeWhenClosing) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(web_socket_scope.Channel(), Close(-1, String()));
  }
  web_socket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, web_socket_scope.Socket().readyState());

  web_socket_scope.Socket().DidConnect("", "");
  EXPECT_EQ(DOMWebSocket::kOpen, web_socket_scope.Socket().readyState());
  web_socket_scope.Socket().close(scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, web_socket_scope.Socket().readyState());

  web_socket_scope.Socket().close(scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, closeWhenClosed) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(web_socket_scope.Channel(), Close(-1, String()));
    EXPECT_CALL(web_socket_scope.Channel(), Disconnect());
  }
  web_socket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, web_socket_scope.Socket().readyState());

  web_socket_scope.Socket().DidConnect("", "");
  EXPECT_EQ(DOMWebSocket::kOpen, web_socket_scope.Socket().readyState());
  web_socket_scope.Socket().close(scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, web_socket_scope.Socket().readyState());

  web_socket_scope.Socket().DidClose(
      WebSocketChannelClient::kClosingHandshakeComplete, 1000, String());
  EXPECT_EQ(DOMWebSocket::kClosed, web_socket_scope.Socket().readyState());
  web_socket_scope.Socket().close(scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosed, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, sendStringWhenConnecting) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String()))
        .WillOnce(Return(true));
  }
  web_socket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  web_socket_scope.Socket().send("hello", scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(kInvalidStateError, scope.GetExceptionState().Code());
  EXPECT_EQ("Still in CONNECTING state.", scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kConnecting, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, sendStringWhenClosing) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(web_socket_scope.Channel(), FailMock(_, _, _));
  }
  web_socket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  web_socket_scope.Socket().close(scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  web_socket_scope.Socket().send("hello", scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, sendStringWhenClosed) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(web_socket_scope.Channel(), Disconnect());
    EXPECT_CALL(checkpoint, Call(1));
  }
  web_socket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  web_socket_scope.Socket().DidClose(
      WebSocketChannelClient::kClosingHandshakeIncomplete, 1006, "");
  checkpoint.Call(1);

  web_socket_scope.Socket().send("hello", scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosed, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, sendStringSuccess) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(web_socket_scope.Channel(), Send(CString("hello")));
  }
  web_socket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  web_socket_scope.Socket().DidConnect("", "");
  web_socket_scope.Socket().send("hello", scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kOpen, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, sendNonLatin1String) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(web_socket_scope.Channel(),
                Send(CString("\xe7\x8b\x90\xe0\xa4\x94")));
  }
  web_socket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  web_socket_scope.Socket().DidConnect("", "");
  UChar non_latin1_string[] = {0x72d0, 0x0914, 0x0000};
  web_socket_scope.Socket().send(non_latin1_string, scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kOpen, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, sendArrayBufferWhenConnecting) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  DOMArrayBufferView* view = DOMUint8Array::Create(8);
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String()))
        .WillOnce(Return(true));
  }
  web_socket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  web_socket_scope.Socket().send(view->buffer(), scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(kInvalidStateError, scope.GetExceptionState().Code());
  EXPECT_EQ("Still in CONNECTING state.", scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kConnecting, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, sendArrayBufferWhenClosing) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  DOMArrayBufferView* view = DOMUint8Array::Create(8);
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(web_socket_scope.Channel(), FailMock(_, _, _));
  }
  web_socket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  web_socket_scope.Socket().close(scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  web_socket_scope.Socket().send(view->buffer(), scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, sendArrayBufferWhenClosed) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  Checkpoint checkpoint;
  DOMArrayBufferView* view = DOMUint8Array::Create(8);
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(web_socket_scope.Channel(), Disconnect());
    EXPECT_CALL(checkpoint, Call(1));
  }
  web_socket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  web_socket_scope.Socket().DidClose(
      WebSocketChannelClient::kClosingHandshakeIncomplete, 1006, "");
  checkpoint.Call(1);

  web_socket_scope.Socket().send(view->buffer(), scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosed, web_socket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, sendArrayBufferSuccess) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  DOMArrayBufferView* view = DOMUint8Array::Create(8);
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(web_socket_scope.Channel(), Send(Ref(*view->buffer()), 0, 8));
  }
  web_socket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  web_socket_scope.Socket().DidConnect("", "");
  web_socket_scope.Socket().send(view->buffer(), scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kOpen, web_socket_scope.Socket().readyState());
}

// FIXME: We should have Blob tests here.
// We can't create a Blob because the blob registration cannot be mocked yet.

// FIXME: We should add tests for bufferedAmount.

// FIXME: We should add tests for data receiving.

TEST(DOMWebSocketTest, binaryType) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  EXPECT_EQ("blob", web_socket_scope.Socket().binaryType());

  web_socket_scope.Socket().setBinaryType("arraybuffer");

  EXPECT_EQ("arraybuffer", web_socket_scope.Socket().binaryType());

  web_socket_scope.Socket().setBinaryType("blob");

  EXPECT_EQ("blob", web_socket_scope.Socket().binaryType());
}

// FIXME: We should add tests for suspend / resume.

class DOMWebSocketValidClosingTest
    : public ::testing::TestWithParam<unsigned short> {};

TEST_P(DOMWebSocketValidClosingTest, test) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(web_socket_scope.Channel(), FailMock(_, _, _));
  }
  web_socket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, web_socket_scope.Socket().readyState());

  web_socket_scope.Socket().close(GetParam(), "bye", scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, web_socket_scope.Socket().readyState());
}

INSTANTIATE_TEST_CASE_P(DOMWebSocketValidClosing,
                        DOMWebSocketValidClosingTest,
                        ::testing::Values(1000, 3000, 3001, 4998, 4999));

class DOMWebSocketInvalidClosingCodeTest
    : public ::testing::TestWithParam<unsigned short> {};

TEST_P(DOMWebSocketInvalidClosingCodeTest, test) {
  V8TestingScope scope;
  DOMWebSocketTestScope web_socket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(web_socket_scope.Channel(),
                Connect(KURL(KURL(), "ws://example.com/"), String()))
        .WillOnce(Return(true));
  }
  web_socket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                    scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, web_socket_scope.Socket().readyState());

  web_socket_scope.Socket().close(GetParam(), "bye", scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(kInvalidAccessError, scope.GetExceptionState().Code());
  EXPECT_EQ(String::Format("The code must be either 1000, or between 3000 and "
                           "4999. %d is neither.",
                           GetParam()),
            scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kConnecting, web_socket_scope.Socket().readyState());
}

INSTANTIATE_TEST_CASE_P(
    DOMWebSocketInvalidClosingCode,
    DOMWebSocketInvalidClosingCodeTest,
    ::testing::Values(0, 1, 998, 999, 1001, 2999, 5000, 9999, 65535));

}  // namespace

}  // namespace blink
