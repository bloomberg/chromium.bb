// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/websockets/DocumentWebSocketChannel.h"

#include <stdint.h>
#include <memory>
#include "core/dom/Document.h"
#include "core/fileapi/Blob.h"
#include "core/testing/DummyPageHolder.h"
#include "core/typed_arrays/DOMArrayBuffer.h"
#include "modules/websockets/WebSocketChannel.h"
#include "modules/websockets/WebSocketChannelClient.h"
#include "modules/websockets/WebSocketHandle.h"
#include "modules/websockets/WebSocketHandleClient.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebCallbacks.h"
#include "public/platform/WebSocketHandshakeThrottle.h"
#include "public/platform/WebURL.h"
#include "public/web/WebLocalFrame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::PrintToString;
using ::testing::AnyNumber;
using ::testing::SaveArg;

namespace blink {

namespace {

typedef ::testing::StrictMock<::testing::MockFunction<void(int)>> Checkpoint;

class MockWebSocketChannelClient
    : public GarbageCollectedFinalized<MockWebSocketChannelClient>,
      public WebSocketChannelClient {
  USING_GARBAGE_COLLECTED_MIXIN(MockWebSocketChannelClient);

 public:
  static MockWebSocketChannelClient* Create() {
    return new ::testing::StrictMock<MockWebSocketChannelClient>();
  }

  MockWebSocketChannelClient() {}

  ~MockWebSocketChannelClient() override {}

  MOCK_METHOD2(DidConnect, void(const String&, const String&));
  MOCK_METHOD1(DidReceiveTextMessage, void(const String&));
  void DidReceiveBinaryMessage(std::unique_ptr<Vector<char>> payload) override {
    DidReceiveBinaryMessageMock(*payload);
  }
  MOCK_METHOD1(DidReceiveBinaryMessageMock, void(const Vector<char>&));
  MOCK_METHOD0(DidError, void());
  MOCK_METHOD1(DidConsumeBufferedAmount, void(uint64_t));
  MOCK_METHOD0(DidStartClosingHandshake, void());
  MOCK_METHOD3(DidClose,
               void(ClosingHandshakeCompletionStatus,
                    unsigned short,
                    const String&));

  virtual void Trace(blink::Visitor* visitor) {
    WebSocketChannelClient::Trace(visitor);
  }
};

class MockWebSocketHandle : public WebSocketHandle {
 public:
  static MockWebSocketHandle* Create() {
    return new ::testing::StrictMock<MockWebSocketHandle>();
  }

  MockWebSocketHandle() {}

  ~MockWebSocketHandle() override {}

  MOCK_METHOD1(DoInitialize, void(mojom::blink::WebSocketPtr*));
  void Initialize(mojom::blink::WebSocketPtr websocket) override {
    DoInitialize(&websocket);
  }

  MOCK_METHOD7(Connect,
               void(const KURL&,
                    const Vector<String>&,
                    SecurityOrigin*,
                    const KURL&,
                    const String&,
                    WebSocketHandleClient*,
                    WebTaskRunner*));
  MOCK_METHOD4(Send,
               void(bool, WebSocketHandle::MessageType, const char*, size_t));
  MOCK_METHOD1(FlowControl, void(int64_t));
  MOCK_METHOD2(Close, void(unsigned short, const String&));
};

class MockWebSocketHandshakeThrottle : public WebSocketHandshakeThrottle {
 public:
  static MockWebSocketHandshakeThrottle* Create() {
    return new ::testing::StrictMock<MockWebSocketHandshakeThrottle>();
  }
  MockWebSocketHandshakeThrottle() {}
  ~MockWebSocketHandshakeThrottle() override { Destructor(); }

  MOCK_METHOD3(ThrottleHandshake,
               void(const WebURL&,
                    WebLocalFrame*,
                    WebCallbacks<void, const WebString&>*));

  // This method is used to allow us to require that the destructor is called at
  // a particular time.
  MOCK_METHOD0(Destructor, void());
};

class DocumentWebSocketChannelTest : public ::testing::Test {
 public:
  DocumentWebSocketChannelTest()
      : page_holder_(DummyPageHolder::Create()),
        channel_client_(MockWebSocketChannelClient::Create()),
        handle_(MockWebSocketHandle::Create()),
        handshake_throttle_(nullptr),
        sum_of_consumed_buffered_amount_(0) {
    ON_CALL(*ChannelClient(), DidConsumeBufferedAmount(_))
        .WillByDefault(Invoke(
            this, &DocumentWebSocketChannelTest::DidConsumeBufferedAmount));
  }

  ~DocumentWebSocketChannelTest() override { Channel()->Disconnect(); }

  void SetUp() override {
    channel_ = DocumentWebSocketChannel::CreateForTesting(
        &page_holder_->GetDocument(), channel_client_.Get(),
        SourceLocation::Capture(), Handle(),
        WTF::WrapUnique(handshake_throttle_));
  }

  MockWebSocketChannelClient* ChannelClient() { return channel_client_.Get(); }

  WebSocketChannel* Channel() {
    return static_cast<WebSocketChannel*>(channel_.Get());
  }

  WebSocketHandleClient* HandleClient() {
    return static_cast<WebSocketHandleClient*>(channel_.Get());
  }

  WebCallbacks<void, const WebString&>* WebCallbacks() {
    return channel_.Get();
  }

  MockWebSocketHandle* Handle() { return handle_; }

  void DidConsumeBufferedAmount(unsigned long a) {
    sum_of_consumed_buffered_amount_ += a;
  }

  void Connect() {
    {
      InSequence s;
      EXPECT_CALL(*Handle(), DoInitialize(_));
      EXPECT_CALL(*Handle(), Connect(KURL(NullURL(), "ws://localhost/"), _, _,
                                     _, _, HandleClient(), _));
      EXPECT_CALL(*Handle(), FlowControl(65536));
      EXPECT_CALL(*ChannelClient(), DidConnect(String("a"), String("b")));
    }
    EXPECT_TRUE(Channel()->Connect(KURL(NullURL(), "ws://localhost/"), "x"));
    HandleClient()->DidConnect(Handle(), String("a"), String("b"));
    ::testing::Mock::VerifyAndClearExpectations(this);
  }

  std::unique_ptr<DummyPageHolder> page_holder_;
  Persistent<MockWebSocketChannelClient> channel_client_;
  MockWebSocketHandle* handle_;
  // |handshake_throttle_| is owned by |channel_| once SetUp() has been called.
  MockWebSocketHandshakeThrottle* handshake_throttle_;
  Persistent<DocumentWebSocketChannel> channel_;
  unsigned long sum_of_consumed_buffered_amount_;
};

MATCHER_P2(MemEq,
           p,
           len,
           std::string("pointing to memory") + (negation ? " not" : "") +
               " equal to \"" +
               std::string(p, len) +
               "\" (length=" +
               PrintToString(len) +
               ")") {
  return memcmp(arg, p, len) == 0;
}

MATCHER_P(KURLEq,
          url_string,
          std::string(negation ? "doesn't equal" : "equals") + " to \"" +
              url_string +
              "\"") {
  KURL url(NullURL(), url_string);
  *result_listener << "where the url is \"" << arg.GetString().Utf8().data()
                   << "\"";
  return arg == url;
}

TEST_F(DocumentWebSocketChannelTest, connectSuccess) {
  Vector<String> protocols;
  scoped_refptr<SecurityOrigin> origin;

  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*Handle(), DoInitialize(_));
    EXPECT_CALL(*Handle(),
                Connect(KURLEq("ws://localhost/"), _, _,
                        KURLEq("http://example.com/"), _, HandleClient(), _))
        .WillOnce(DoAll(SaveArg<1>(&protocols), SaveArg<2>(&origin)));
    EXPECT_CALL(*Handle(), FlowControl(65536));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*ChannelClient(), DidConnect(String("a"), String("b")));
  }

  KURL page_url(NullURL(), "http://example.com/");
  page_holder_->GetFrame().GetSecurityContext()->SetSecurityOrigin(
      SecurityOrigin::Create(page_url));
  Document& document = page_holder_->GetDocument();
  document.SetURL(page_url);
  // Make sure that firstPartyForCookies() is set to the given value.
  EXPECT_STREQ("http://example.com/",
               document.SiteForCookies().GetString().Utf8().data());

  EXPECT_TRUE(Channel()->Connect(KURL(NullURL(), "ws://localhost/"), "x"));

  EXPECT_EQ(1U, protocols.size());
  EXPECT_STREQ("x", protocols[0].Utf8().data());

  EXPECT_STREQ("http://example.com", origin->ToString().Utf8().data());

  checkpoint.Call(1);
  HandleClient()->DidConnect(Handle(), String("a"), String("b"));
}

TEST_F(DocumentWebSocketChannelTest, sendText) {
  Connect();
  {
    InSequence s;
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeText,
                                MemEq("foo", 3), 3));
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeText,
                                MemEq("bar", 3), 3));
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeText,
                                MemEq("baz", 3), 3));
  }

  HandleClient()->DidReceiveFlowControl(Handle(), 16);
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  Channel()->Send("foo");
  Channel()->Send("bar");
  Channel()->Send("baz");

  EXPECT_EQ(9ul, sum_of_consumed_buffered_amount_);
}

TEST_F(DocumentWebSocketChannelTest, sendTextContinuation) {
  Connect();
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*Handle(), Send(false, WebSocketHandle::kMessageTypeText,
                                MemEq("0123456789abcdef", 16), 16));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeContinuation,
                                MemEq("g", 1), 1));
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeText,
                                MemEq("hijk", 4), 4));
    EXPECT_CALL(*Handle(), Send(false, WebSocketHandle::kMessageTypeText,
                                MemEq("lmnopqrstuv", 11), 11));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*Handle(),
                Send(false, WebSocketHandle::kMessageTypeContinuation,
                     MemEq("wxyzABCDEFGHIJKL", 16), 16));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeContinuation,
                                MemEq("MNOPQRSTUVWXYZ", 14), 14));
  }

  HandleClient()->DidReceiveFlowControl(Handle(), 16);
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  Channel()->Send("0123456789abcdefg");
  Channel()->Send("hijk");
  Channel()->Send("lmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
  checkpoint.Call(1);
  HandleClient()->DidReceiveFlowControl(Handle(), 16);
  checkpoint.Call(2);
  HandleClient()->DidReceiveFlowControl(Handle(), 16);
  checkpoint.Call(3);
  HandleClient()->DidReceiveFlowControl(Handle(), 16);

  EXPECT_EQ(62ul, sum_of_consumed_buffered_amount_);
}

TEST_F(DocumentWebSocketChannelTest, sendBinaryInVector) {
  Connect();
  {
    InSequence s;
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeBinary,
                                MemEq("foo", 3), 3));
  }

  HandleClient()->DidReceiveFlowControl(Handle(), 16);
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  Vector<char> foo_vector;
  foo_vector.Append("foo", 3);
  Channel()->SendBinaryAsCharVector(std::make_unique<Vector<char>>(foo_vector));

  EXPECT_EQ(3ul, sum_of_consumed_buffered_amount_);
}

TEST_F(DocumentWebSocketChannelTest, sendBinaryInVectorWithNullBytes) {
  Connect();
  {
    InSequence s;
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeBinary,
                                MemEq("\0ar", 3), 3));
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeBinary,
                                MemEq("b\0z", 3), 3));
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeBinary,
                                MemEq("qu\0", 3), 3));
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeBinary,
                                MemEq("\0\0\0", 3), 3));
  }

  HandleClient()->DidReceiveFlowControl(Handle(), 16);
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  {
    Vector<char> v;
    v.Append("\0ar", 3);
    Channel()->SendBinaryAsCharVector(std::make_unique<Vector<char>>(v));
  }
  {
    Vector<char> v;
    v.Append("b\0z", 3);
    Channel()->SendBinaryAsCharVector(std::make_unique<Vector<char>>(v));
  }
  {
    Vector<char> v;
    v.Append("qu\0", 3);
    Channel()->SendBinaryAsCharVector(std::make_unique<Vector<char>>(v));
  }
  {
    Vector<char> v;
    v.Append("\0\0\0", 3);
    Channel()->SendBinaryAsCharVector(std::make_unique<Vector<char>>(v));
  }

  EXPECT_EQ(12ul, sum_of_consumed_buffered_amount_);
}

TEST_F(DocumentWebSocketChannelTest, sendBinaryInVectorNonLatin1UTF8) {
  Connect();
  EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeBinary,
                              MemEq("\xe7\x8b\x90", 3), 3));

  HandleClient()->DidReceiveFlowControl(Handle(), 16);
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  Vector<char> v;
  v.Append("\xe7\x8b\x90", 3);
  Channel()->SendBinaryAsCharVector(std::make_unique<Vector<char>>(v));

  EXPECT_EQ(3ul, sum_of_consumed_buffered_amount_);
}

TEST_F(DocumentWebSocketChannelTest, sendBinaryInVectorNonUTF8) {
  Connect();
  EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeBinary,
                              MemEq("\x80\xff\xe7", 3), 3));

  HandleClient()->DidReceiveFlowControl(Handle(), 16);
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  Vector<char> v;
  v.Append("\x80\xff\xe7", 3);
  Channel()->SendBinaryAsCharVector(std::make_unique<Vector<char>>(v));

  EXPECT_EQ(3ul, sum_of_consumed_buffered_amount_);
}

TEST_F(DocumentWebSocketChannelTest,
       sendBinaryInVectorNonLatin1UTF8Continuation) {
  Connect();
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*Handle(), Send(false, WebSocketHandle::kMessageTypeBinary,
                                MemEq("\xe7\x8b\x90\xe7\x8b\x90\xe7\x8b\x90\xe7"
                                      "\x8b\x90\xe7\x8b\x90\xe7",
                                      16),
                                16));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeContinuation,
                                MemEq("\x8b\x90", 2), 2));
  }

  HandleClient()->DidReceiveFlowControl(Handle(), 16);
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  Vector<char> v;
  v.Append(
      "\xe7\x8b\x90\xe7\x8b\x90\xe7\x8b\x90\xe7\x8b\x90\xe7\x8b\x90\xe7\x8b"
      "\x90",
      18);
  Channel()->SendBinaryAsCharVector(std::make_unique<Vector<char>>(v));
  checkpoint.Call(1);

  HandleClient()->DidReceiveFlowControl(Handle(), 16);

  EXPECT_EQ(18ul, sum_of_consumed_buffered_amount_);
}

TEST_F(DocumentWebSocketChannelTest, sendBinaryInArrayBuffer) {
  Connect();
  {
    InSequence s;
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeBinary,
                                MemEq("foo", 3), 3));
  }

  HandleClient()->DidReceiveFlowControl(Handle(), 16);
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  DOMArrayBuffer* foo_buffer = DOMArrayBuffer::Create("foo", 3);
  Channel()->Send(*foo_buffer, 0, 3);

  EXPECT_EQ(3ul, sum_of_consumed_buffered_amount_);
}

TEST_F(DocumentWebSocketChannelTest, sendBinaryInArrayBufferPartial) {
  Connect();
  {
    InSequence s;
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeBinary,
                                MemEq("foo", 3), 3));
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeBinary,
                                MemEq("bar", 3), 3));
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeBinary,
                                MemEq("baz", 3), 3));
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeBinary,
                                MemEq("a", 1), 1));
  }

  HandleClient()->DidReceiveFlowControl(Handle(), 16);
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  DOMArrayBuffer* foobar_buffer = DOMArrayBuffer::Create("foobar", 6);
  DOMArrayBuffer* qbazux_buffer = DOMArrayBuffer::Create("qbazux", 6);
  Channel()->Send(*foobar_buffer, 0, 3);
  Channel()->Send(*foobar_buffer, 3, 3);
  Channel()->Send(*qbazux_buffer, 1, 3);
  Channel()->Send(*qbazux_buffer, 2, 1);

  EXPECT_EQ(10ul, sum_of_consumed_buffered_amount_);
}

TEST_F(DocumentWebSocketChannelTest, sendBinaryInArrayBufferWithNullBytes) {
  Connect();
  {
    InSequence s;
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeBinary,
                                MemEq("\0ar", 3), 3));
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeBinary,
                                MemEq("b\0z", 3), 3));
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeBinary,
                                MemEq("qu\0", 3), 3));
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeBinary,
                                MemEq("\0\0\0", 3), 3));
  }

  HandleClient()->DidReceiveFlowControl(Handle(), 16);
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  {
    DOMArrayBuffer* b = DOMArrayBuffer::Create("\0ar", 3);
    Channel()->Send(*b, 0, 3);
  }
  {
    DOMArrayBuffer* b = DOMArrayBuffer::Create("b\0z", 3);
    Channel()->Send(*b, 0, 3);
  }
  {
    DOMArrayBuffer* b = DOMArrayBuffer::Create("qu\0", 3);
    Channel()->Send(*b, 0, 3);
  }
  {
    DOMArrayBuffer* b = DOMArrayBuffer::Create("\0\0\0", 3);
    Channel()->Send(*b, 0, 3);
  }

  EXPECT_EQ(12ul, sum_of_consumed_buffered_amount_);
}

TEST_F(DocumentWebSocketChannelTest, sendBinaryInArrayBufferNonLatin1UTF8) {
  Connect();
  EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeBinary,
                              MemEq("\xe7\x8b\x90", 3), 3));

  HandleClient()->DidReceiveFlowControl(Handle(), 16);
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  DOMArrayBuffer* b = DOMArrayBuffer::Create("\xe7\x8b\x90", 3);
  Channel()->Send(*b, 0, 3);

  EXPECT_EQ(3ul, sum_of_consumed_buffered_amount_);
}

TEST_F(DocumentWebSocketChannelTest, sendBinaryInArrayBufferNonUTF8) {
  Connect();
  EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeBinary,
                              MemEq("\x80\xff\xe7", 3), 3));

  HandleClient()->DidReceiveFlowControl(Handle(), 16);
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  DOMArrayBuffer* b = DOMArrayBuffer::Create("\x80\xff\xe7", 3);
  Channel()->Send(*b, 0, 3);

  EXPECT_EQ(3ul, sum_of_consumed_buffered_amount_);
}

TEST_F(DocumentWebSocketChannelTest,
       sendBinaryInArrayBufferNonLatin1UTF8Continuation) {
  Connect();
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*Handle(), Send(false, WebSocketHandle::kMessageTypeBinary,
                                MemEq("\xe7\x8b\x90\xe7\x8b\x90\xe7\x8b\x90\xe7"
                                      "\x8b\x90\xe7\x8b\x90\xe7",
                                      16),
                                16));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*Handle(), Send(true, WebSocketHandle::kMessageTypeContinuation,
                                MemEq("\x8b\x90", 2), 2));
  }

  HandleClient()->DidReceiveFlowControl(Handle(), 16);
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  DOMArrayBuffer* b = DOMArrayBuffer::Create(
      "\xe7\x8b\x90\xe7\x8b\x90\xe7\x8b\x90\xe7\x8b\x90\xe7\x8b\x90\xe7\x8b"
      "\x90",
      18);
  Channel()->Send(*b, 0, 18);
  checkpoint.Call(1);

  HandleClient()->DidReceiveFlowControl(Handle(), 16);

  EXPECT_EQ(18ul, sum_of_consumed_buffered_amount_);
}

// FIXME: Add tests for WebSocketChannel::send(scoped_refptr<BlobDataHandle>)

TEST_F(DocumentWebSocketChannelTest, receiveText) {
  Connect();
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("FOO")));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("BAR")));
  }

  HandleClient()->DidReceiveData(Handle(), true,
                                 WebSocketHandle::kMessageTypeText, "FOOX", 3);
  HandleClient()->DidReceiveData(Handle(), true,
                                 WebSocketHandle::kMessageTypeText, "BARX", 3);
}

TEST_F(DocumentWebSocketChannelTest, receiveTextContinuation) {
  Connect();
  EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("BAZ")));

  HandleClient()->DidReceiveData(Handle(), false,
                                 WebSocketHandle::kMessageTypeText, "BX", 1);
  HandleClient()->DidReceiveData(
      Handle(), false, WebSocketHandle::kMessageTypeContinuation, "AX", 1);
  HandleClient()->DidReceiveData(
      Handle(), true, WebSocketHandle::kMessageTypeContinuation, "ZX", 1);
}

TEST_F(DocumentWebSocketChannelTest, receiveTextNonLatin1) {
  Connect();
  UChar non_latin1_string[] = {0x72d0, 0x0914, 0x0000};
  EXPECT_CALL(*ChannelClient(),
              DidReceiveTextMessage(String(non_latin1_string)));

  HandleClient()->DidReceiveData(Handle(), true,
                                 WebSocketHandle::kMessageTypeText,
                                 "\xe7\x8b\x90\xe0\xa4\x94", 6);
}

TEST_F(DocumentWebSocketChannelTest, receiveTextNonLatin1Continuation) {
  Connect();
  UChar non_latin1_string[] = {0x72d0, 0x0914, 0x0000};
  EXPECT_CALL(*ChannelClient(),
              DidReceiveTextMessage(String(non_latin1_string)));

  HandleClient()->DidReceiveData(
      Handle(), false, WebSocketHandle::kMessageTypeText, "\xe7\x8b", 2);
  HandleClient()->DidReceiveData(Handle(), false,
                                 WebSocketHandle::kMessageTypeContinuation,
                                 "\x90\xe0", 2);
  HandleClient()->DidReceiveData(
      Handle(), false, WebSocketHandle::kMessageTypeContinuation, "\xa4", 1);
  HandleClient()->DidReceiveData(
      Handle(), true, WebSocketHandle::kMessageTypeContinuation, "\x94", 1);
}

TEST_F(DocumentWebSocketChannelTest, receiveBinary) {
  Connect();
  Vector<char> foo_vector;
  foo_vector.Append("FOO", 3);
  EXPECT_CALL(*ChannelClient(), DidReceiveBinaryMessageMock(foo_vector));

  HandleClient()->DidReceiveData(
      Handle(), true, WebSocketHandle::kMessageTypeBinary, "FOOx", 3);
}

TEST_F(DocumentWebSocketChannelTest, receiveBinaryContinuation) {
  Connect();
  Vector<char> baz_vector;
  baz_vector.Append("BAZ", 3);
  EXPECT_CALL(*ChannelClient(), DidReceiveBinaryMessageMock(baz_vector));

  HandleClient()->DidReceiveData(Handle(), false,
                                 WebSocketHandle::kMessageTypeBinary, "Bx", 1);
  HandleClient()->DidReceiveData(
      Handle(), false, WebSocketHandle::kMessageTypeContinuation, "Ax", 1);
  HandleClient()->DidReceiveData(
      Handle(), true, WebSocketHandle::kMessageTypeContinuation, "Zx", 1);
}

TEST_F(DocumentWebSocketChannelTest, receiveBinaryWithNullBytes) {
  Connect();
  {
    InSequence s;
    {
      Vector<char> v;
      v.Append("\0AR", 3);
      EXPECT_CALL(*ChannelClient(), DidReceiveBinaryMessageMock(v));
    }
    {
      Vector<char> v;
      v.Append("B\0Z", 3);
      EXPECT_CALL(*ChannelClient(), DidReceiveBinaryMessageMock(v));
    }
    {
      Vector<char> v;
      v.Append("QU\0", 3);
      EXPECT_CALL(*ChannelClient(), DidReceiveBinaryMessageMock(v));
    }
    {
      Vector<char> v;
      v.Append("\0\0\0", 3);
      EXPECT_CALL(*ChannelClient(), DidReceiveBinaryMessageMock(v));
    }
  }

  HandleClient()->DidReceiveData(
      Handle(), true, WebSocketHandle::kMessageTypeBinary, "\0AR", 3);
  HandleClient()->DidReceiveData(
      Handle(), true, WebSocketHandle::kMessageTypeBinary, "B\0Z", 3);
  HandleClient()->DidReceiveData(
      Handle(), true, WebSocketHandle::kMessageTypeBinary, "QU\0", 3);
  HandleClient()->DidReceiveData(
      Handle(), true, WebSocketHandle::kMessageTypeBinary, "\0\0\0", 3);
}

TEST_F(DocumentWebSocketChannelTest, receiveBinaryNonLatin1UTF8) {
  Connect();
  Vector<char> v;
  v.Append("\xe7\x8b\x90\xe0\xa4\x94", 6);
  EXPECT_CALL(*ChannelClient(), DidReceiveBinaryMessageMock(v));

  HandleClient()->DidReceiveData(Handle(), true,
                                 WebSocketHandle::kMessageTypeBinary,
                                 "\xe7\x8b\x90\xe0\xa4\x94", 6);
}

TEST_F(DocumentWebSocketChannelTest, receiveBinaryNonLatin1UTF8Continuation) {
  Connect();
  Vector<char> v;
  v.Append("\xe7\x8b\x90\xe0\xa4\x94", 6);
  EXPECT_CALL(*ChannelClient(), DidReceiveBinaryMessageMock(v));

  HandleClient()->DidReceiveData(
      Handle(), false, WebSocketHandle::kMessageTypeBinary, "\xe7\x8b", 2);
  HandleClient()->DidReceiveData(Handle(), false,
                                 WebSocketHandle::kMessageTypeContinuation,
                                 "\x90\xe0", 2);
  HandleClient()->DidReceiveData(
      Handle(), false, WebSocketHandle::kMessageTypeContinuation, "\xa4", 1);
  HandleClient()->DidReceiveData(
      Handle(), true, WebSocketHandle::kMessageTypeContinuation, "\x94", 1);
}

TEST_F(DocumentWebSocketChannelTest, receiveBinaryNonUTF8) {
  Connect();
  Vector<char> v;
  v.Append("\x80\xff", 2);
  EXPECT_CALL(*ChannelClient(), DidReceiveBinaryMessageMock(v));

  HandleClient()->DidReceiveData(
      Handle(), true, WebSocketHandle::kMessageTypeBinary, "\x80\xff", 2);
}

TEST_F(DocumentWebSocketChannelTest, closeFromBrowser) {
  Connect();
  Checkpoint checkpoint;
  {
    InSequence s;

    EXPECT_CALL(*ChannelClient(), DidStartClosingHandshake());
    EXPECT_CALL(checkpoint, Call(1));

    EXPECT_CALL(*Handle(), Close(WebSocketChannel::kCloseEventCodeNormalClosure,
                                 String("close reason")));
    EXPECT_CALL(checkpoint, Call(2));

    EXPECT_CALL(*ChannelClient(),
                DidClose(WebSocketChannelClient::kClosingHandshakeComplete,
                         WebSocketChannel::kCloseEventCodeNormalClosure,
                         String("close reason")));
    EXPECT_CALL(checkpoint, Call(3));
  }

  HandleClient()->DidStartClosingHandshake(Handle());
  checkpoint.Call(1);

  Channel()->Close(WebSocketChannel::kCloseEventCodeNormalClosure,
                   String("close reason"));
  checkpoint.Call(2);

  HandleClient()->DidClose(Handle(), true,
                           WebSocketChannel::kCloseEventCodeNormalClosure,
                           String("close reason"));
  checkpoint.Call(3);

  Channel()->Disconnect();
}

TEST_F(DocumentWebSocketChannelTest, closeFromWebSocket) {
  Connect();
  Checkpoint checkpoint;
  {
    InSequence s;

    EXPECT_CALL(*Handle(), Close(WebSocketChannel::kCloseEventCodeNormalClosure,
                                 String("close reason")));
    EXPECT_CALL(checkpoint, Call(1));

    EXPECT_CALL(*ChannelClient(),
                DidClose(WebSocketChannelClient::kClosingHandshakeComplete,
                         WebSocketChannel::kCloseEventCodeNormalClosure,
                         String("close reason")));
    EXPECT_CALL(checkpoint, Call(2));
  }

  Channel()->Close(WebSocketChannel::kCloseEventCodeNormalClosure,
                   String("close reason"));
  checkpoint.Call(1);

  HandleClient()->DidClose(Handle(), true,
                           WebSocketChannel::kCloseEventCodeNormalClosure,
                           String("close reason"));
  checkpoint.Call(2);

  Channel()->Disconnect();
}

TEST_F(DocumentWebSocketChannelTest, failFromBrowser) {
  Connect();
  {
    InSequence s;

    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(
        *ChannelClient(),
        DidClose(WebSocketChannelClient::kClosingHandshakeIncomplete,
                 WebSocketChannel::kCloseEventCodeAbnormalClosure, String()));
  }

  HandleClient()->DidFail(Handle(), "fail message");
}

TEST_F(DocumentWebSocketChannelTest, failFromWebSocket) {
  Connect();
  {
    InSequence s;

    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(
        *ChannelClient(),
        DidClose(WebSocketChannelClient::kClosingHandshakeIncomplete,
                 WebSocketChannel::kCloseEventCodeAbnormalClosure, String()));
  }

  Channel()->Fail("fail message from WebSocket", kErrorMessageLevel,
                  SourceLocation::Create(String(), 0, 0, nullptr));
}

class DocumentWebSocketChannelHandshakeThrottleTest
    : public DocumentWebSocketChannelTest {
 public:
  DocumentWebSocketChannelHandshakeThrottleTest() {
    handshake_throttle_ = MockWebSocketHandshakeThrottle::Create();
  }

  // Expectations for the normal result of calling Channel()->Connect() with a
  // non-null throttle.
  void NormalHandshakeExpectations() {
    EXPECT_CALL(*Handle(), DoInitialize(_));
    EXPECT_CALL(*Handle(), Connect(_, _, _, _, _, _, _));
    EXPECT_CALL(*Handle(), FlowControl(_));
    EXPECT_CALL(*handshake_throttle_, ThrottleHandshake(_, _, _));
  }

  static KURL url() { return KURL(NullURL(), "ws://localhost/"); }
};

TEST_F(DocumentWebSocketChannelHandshakeThrottleTest, ThrottleArguments) {
  EXPECT_CALL(*Handle(), DoInitialize(_));
  EXPECT_CALL(*Handle(), Connect(_, _, _, _, _, _, _));
  EXPECT_CALL(*Handle(), FlowControl(_));
  EXPECT_CALL(*handshake_throttle_,
              ThrottleHandshake(WebURL(url()), _, WebCallbacks()));
  EXPECT_CALL(*handshake_throttle_, Destructor());
  Channel()->Connect(url(), "");
}

TEST_F(DocumentWebSocketChannelHandshakeThrottleTest, ThrottleSucceedsFirst) {
  Checkpoint checkpoint;
  NormalHandshakeExpectations();
  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*handshake_throttle_, Destructor());
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*ChannelClient(), DidConnect(String("a"), String("b")));
  }
  Channel()->Connect(url(), "");
  checkpoint.Call(1);
  WebCallbacks()->OnSuccess();
  checkpoint.Call(2);
  HandleClient()->DidConnect(Handle(), String("a"), String("b"));
}

TEST_F(DocumentWebSocketChannelHandshakeThrottleTest, HandshakeSucceedsFirst) {
  Checkpoint checkpoint;
  NormalHandshakeExpectations();
  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*handshake_throttle_, Destructor());
    EXPECT_CALL(*ChannelClient(), DidConnect(String("a"), String("b")));
  }
  Channel()->Connect(url(), "");
  checkpoint.Call(1);
  HandleClient()->DidConnect(Handle(), String("a"), String("b"));
  checkpoint.Call(2);
  WebCallbacks()->OnSuccess();
}

// This happens if JS code calls close() during the handshake.
TEST_F(DocumentWebSocketChannelHandshakeThrottleTest, FailDuringThrottle) {
  Checkpoint checkpoint;
  NormalHandshakeExpectations();
  {
    InSequence s;
    EXPECT_CALL(*handshake_throttle_, Destructor());
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(*ChannelClient(), DidClose(_, _, _));
    EXPECT_CALL(checkpoint, Call(1));
  }
  Channel()->Connect(url(), "");
  Channel()->Fail("close during handshake", kWarningMessageLevel,
                  SourceLocation::Create(String(), 0, 0, nullptr));
  checkpoint.Call(1);
}

// It makes no difference to the behaviour if the WebSocketHandle has actually
// connected.
TEST_F(DocumentWebSocketChannelHandshakeThrottleTest,
       FailDuringThrottleAfterConnect) {
  Checkpoint checkpoint;
  NormalHandshakeExpectations();
  {
    InSequence s;
    EXPECT_CALL(*handshake_throttle_, Destructor());
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(*ChannelClient(), DidClose(_, _, _));
    EXPECT_CALL(checkpoint, Call(1));
  }
  Channel()->Connect(url(), "");
  HandleClient()->DidConnect(Handle(), String("a"), String("b"));
  Channel()->Fail("close during handshake", kWarningMessageLevel,
                  SourceLocation::Create(String(), 0, 0, nullptr));
  checkpoint.Call(1);
}

// This happens if the JS context is destroyed during the handshake.
TEST_F(DocumentWebSocketChannelHandshakeThrottleTest, CloseDuringThrottle) {
  Checkpoint checkpoint;
  NormalHandshakeExpectations();
  {
    InSequence s;
    EXPECT_CALL(*handshake_throttle_, Destructor());
    EXPECT_CALL(*Handle(), Close(_, _));
    EXPECT_CALL(checkpoint, Call(1));
  }
  Channel()->Connect(url(), "");
  Channel()->Close(DocumentWebSocketChannel::kCloseEventCodeGoingAway, "");
  checkpoint.Call(1);
}

TEST_F(DocumentWebSocketChannelHandshakeThrottleTest,
       CloseDuringThrottleAfterConnect) {
  Checkpoint checkpoint;
  NormalHandshakeExpectations();
  {
    InSequence s;
    EXPECT_CALL(*handshake_throttle_, Destructor());
    EXPECT_CALL(*Handle(), Close(_, _));
    EXPECT_CALL(checkpoint, Call(1));
  }
  Channel()->Connect(url(), "");
  HandleClient()->DidConnect(Handle(), String("a"), String("b"));
  Channel()->Close(DocumentWebSocketChannel::kCloseEventCodeGoingAway, "");
  checkpoint.Call(1);
}

TEST_F(DocumentWebSocketChannelHandshakeThrottleTest,
       DisconnectDuringThrottle) {
  Checkpoint checkpoint;
  NormalHandshakeExpectations();
  {
    InSequence s;
    EXPECT_CALL(*handshake_throttle_, Destructor());
    EXPECT_CALL(checkpoint, Call(1));
  }
  Channel()->Connect(url(), "");
  Channel()->Disconnect();
  checkpoint.Call(1);
}

TEST_F(DocumentWebSocketChannelHandshakeThrottleTest,
       DisconnectDuringThrottleAfterConnect) {
  Checkpoint checkpoint;
  NormalHandshakeExpectations();
  {
    InSequence s;
    EXPECT_CALL(*handshake_throttle_, Destructor());
    EXPECT_CALL(checkpoint, Call(1));
  }
  Channel()->Connect(url(), "");
  HandleClient()->DidConnect(Handle(), String("a"), String("b"));
  Channel()->Disconnect();
  checkpoint.Call(1);
}

TEST_F(DocumentWebSocketChannelHandshakeThrottleTest,
       ThrottleReportsErrorBeforeConnect) {
  NormalHandshakeExpectations();
  {
    InSequence s;
    EXPECT_CALL(*handshake_throttle_, Destructor());
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(*ChannelClient(), DidClose(_, _, _));
  }
  Channel()->Connect(url(), "");
  WebCallbacks()->OnError("Connection blocked by throttle");
}

TEST_F(DocumentWebSocketChannelHandshakeThrottleTest,
       ThrottleReportsErrorAfterConnect) {
  NormalHandshakeExpectations();
  {
    InSequence s;
    EXPECT_CALL(*handshake_throttle_, Destructor());
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(*ChannelClient(), DidClose(_, _, _));
  }
  Channel()->Connect(url(), "");
  HandleClient()->DidConnect(Handle(), String("a"), String("b"));
  WebCallbacks()->OnError("Connection blocked by throttle");
}

TEST_F(DocumentWebSocketChannelHandshakeThrottleTest,
       ConnectFailBeforeThrottle) {
  NormalHandshakeExpectations();
  {
    InSequence s;
    EXPECT_CALL(*handshake_throttle_, Destructor());
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(*ChannelClient(), DidClose(_, _, _));
  }
  Channel()->Connect(url(), "");
  HandleClient()->DidFail(Handle(), "connect failed");
}

// TODO(ricea): Can this actually happen?
TEST_F(DocumentWebSocketChannelHandshakeThrottleTest,
       ConnectCloseBeforeThrottle) {
  NormalHandshakeExpectations();
  {
    InSequence s;
    EXPECT_CALL(*handshake_throttle_, Destructor());
    EXPECT_CALL(*ChannelClient(), DidClose(_, _, _));
  }
  Channel()->Connect(url(), "");
  HandleClient()->DidClose(Handle(), false,
                           WebSocketChannel::kCloseEventCodeProtocolError,
                           "connect error");
}

}  // namespace

}  // namespace blink
