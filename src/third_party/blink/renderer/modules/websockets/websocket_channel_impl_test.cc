// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/websockets/websocket_channel_impl.h"

#include <stdint.h>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/websockets/websocket_connector.mojom-blink.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

using testing::_;
using testing::InSequence;
using testing::PrintToString;
using testing::AnyNumber;
using testing::SaveArg;

namespace blink {

typedef testing::StrictMock<testing::MockFunction<void(int)>> Checkpoint;

class MockWebSocketChannelClient
    : public GarbageCollected<MockWebSocketChannelClient>,
      public WebSocketChannelClient {
  USING_GARBAGE_COLLECTED_MIXIN(MockWebSocketChannelClient);

 public:
  static MockWebSocketChannelClient* Create() {
    return MakeGarbageCollected<
        testing::StrictMock<MockWebSocketChannelClient>>();
  }

  MockWebSocketChannelClient() = default;

  ~MockWebSocketChannelClient() override = default;

  MOCK_METHOD2(DidConnect, void(const String&, const String&));
  MOCK_METHOD1(DidReceiveTextMessage, void(const String&));
  void DidReceiveBinaryMessage(
      const Vector<base::span<const char>>& data) override {
    Vector<char> flatten;
    for (const auto& span : data) {
      flatten.Append(span.data(), static_cast<wtf_size_t>(span.size()));
    }
    DidReceiveBinaryMessageMock(flatten);
  }
  MOCK_METHOD1(DidReceiveBinaryMessageMock, void(const Vector<char>&));
  MOCK_METHOD0(DidError, void());
  MOCK_METHOD1(DidConsumeBufferedAmount, void(uint64_t));
  MOCK_METHOD0(DidStartClosingHandshake, void());
  MOCK_METHOD3(DidClose,
               void(ClosingHandshakeCompletionStatus, uint16_t, const String&));

  void Trace(Visitor* visitor) override {
    WebSocketChannelClient::Trace(visitor);
  }
};

class MockWebSocketHandshakeThrottle : public WebSocketHandshakeThrottle {
 public:
  MockWebSocketHandshakeThrottle() = default;
  ~MockWebSocketHandshakeThrottle() override { Destructor(); }

  MOCK_METHOD2(ThrottleHandshake,
               void(const WebURL&, WebSocketHandshakeThrottle::OnCompletion));

  // This method is used to allow us to require that the destructor is called at
  // a particular time.
  MOCK_METHOD0(Destructor, void());
};

class WebSocketChannelImplTest : public PageTestBase {
 public:
  using WebSocketMessageType = network::mojom::WebSocketMessageType;
  class TestWebSocket final : public network::mojom::blink::WebSocket {
   public:
    struct Frame {
      bool fin;
      WebSocketMessageType type;
      Vector<uint8_t> data;

      bool operator==(const Frame& that) const {
        return fin == that.fin && type == that.type && data == that.data;
      }
    };

    struct DataFrame final {
      DataFrame(WebSocketMessageType type, uint64_t data_length)
          : type(type), data_length(data_length) {}
      WebSocketMessageType type;
      uint64_t data_length;

      bool operator==(const DataFrame& that) const {
        return std::tie(type, data_length) ==
               std::tie(that.type, that.data_length);
      }
    };

    explicit TestWebSocket(
        mojo::PendingReceiver<network::mojom::blink::WebSocket>
            pending_receiver)
        : receiver_(this, std::move(pending_receiver)) {}

    void SendFrame(bool fin,
                   WebSocketMessageType type,
                   base::span<const uint8_t> data) override {
      Vector<uint8_t> data_to_pass;
      data_to_pass.AppendRange(data.begin(), data.end());
      frames_.push_back(Frame{fin, type, std::move(data_to_pass)});
    }
    void SendMessage(WebSocketMessageType type, uint64_t data_length) override {
      pending_send_data_frames_.push_back(DataFrame(type, data_length));
      return;
    }
    void StartReceiving() override {
      DCHECK(!is_start_receiving_called_);
      is_start_receiving_called_ = true;
    }
    void StartClosingHandshake(uint16_t code, const String& reason) override {
      DCHECK(!is_start_closing_handshake_called_);
      is_start_closing_handshake_called_ = true;
      closing_code_ = code;
      closing_reason_ = reason;
    }

    const Vector<Frame>& GetFrames() const { return frames_; }
    void ClearFrames() { frames_.clear(); }
    const Vector<DataFrame>& GetDataFrames() const {
      return pending_send_data_frames_;
    }
    void ClearDataFrames() { pending_send_data_frames_.clear(); }
    bool IsStartReceivingCalled() const { return is_start_receiving_called_; }
    bool IsStartClosingHandshakeCalled() const {
      return is_start_closing_handshake_called_;
    }
    uint16_t GetClosingCode() const { return closing_code_; }
    const String& GetClosingReason() const { return closing_reason_; }

   private:
    Vector<Frame> frames_;
    Vector<DataFrame> pending_send_data_frames_;
    bool is_start_receiving_called_ = false;
    bool is_start_closing_handshake_called_ = false;
    uint16_t closing_code_ = 0;
    String closing_reason_;

    mojo::Receiver<network::mojom::blink::WebSocket> receiver_;
  };
  using Frames = Vector<TestWebSocket::Frame>;
  using DataFrames = Vector<TestWebSocket::DataFrame>;

  class WebSocketConnector final : public mojom::blink::WebSocketConnector {
   public:
    struct ConnectArgs {
      ConnectArgs(
          const KURL& url,
          const Vector<String>& protocols,
          const net::SiteForCookies& site_for_cookies,
          const String& user_agent,
          mojo::PendingRemote<network::mojom::blink::WebSocketHandshakeClient>
              handshake_client)
          : url(url),
            protocols(protocols),
            site_for_cookies(site_for_cookies),
            user_agent(user_agent),
            handshake_client(std::move(handshake_client)) {}

      KURL url;
      Vector<String> protocols;
      net::SiteForCookies site_for_cookies;
      String user_agent;
      mojo::PendingRemote<network::mojom::blink::WebSocketHandshakeClient>
          handshake_client;
    };

    void Connect(
        const KURL& url,
        const Vector<String>& requested_protocols,
        const net::SiteForCookies& site_for_cookies,
        const String& user_agent,
        mojo::PendingRemote<network::mojom::blink::WebSocketHandshakeClient>
            handshake_client) override {
      connect_args_.push_back(ConnectArgs(url, requested_protocols,
                                          site_for_cookies, user_agent,
                                          std::move(handshake_client)));
    }

    const Vector<ConnectArgs>& GetConnectArgs() const { return connect_args_; }
    Vector<ConnectArgs> TakeConnectArgs() { return std::move(connect_args_); }

    void Bind(
        mojo::PendingReceiver<mojom::blink::WebSocketConnector> receiver) {
      receiver_set_.Add(this, std::move(receiver));
    }

   private:
    mojo::ReceiverSet<mojom::blink::WebSocketConnector> receiver_set_;
    Vector<ConnectArgs> connect_args_;
  };

  explicit WebSocketChannelImplTest(
      std::unique_ptr<MockWebSocketHandshakeThrottle> handshake_throttle =
          nullptr)
      : channel_client_(MockWebSocketChannelClient::Create()),
        handshake_throttle_(std::move(handshake_throttle)),
        raw_handshake_throttle_(handshake_throttle_.get()),
        sum_of_consumed_buffered_amount_(0),
        weak_ptr_factory_(this) {
    ON_CALL(*ChannelClient(), DidConsumeBufferedAmount(_))
        .WillByDefault(
            Invoke(this, &WebSocketChannelImplTest::DidConsumeBufferedAmount));
  }

  ~WebSocketChannelImplTest() override { Channel()->Disconnect(); }

  void BindWebSocketConnector(mojo::ScopedMessagePipeHandle handle) {
    connector_.Bind(mojo::PendingReceiver<mojom::blink::WebSocketConnector>(
        std::move(handle)));
  }

  MojoResult CreateDataPipe(uint32_t capacity,
                            mojo::ScopedDataPipeProducerHandle* writable,
                            mojo::ScopedDataPipeConsumerHandle* readable) {
    const MojoCreateDataPipeOptions data_pipe_options{
        sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1,
        capacity};
    return mojo::CreateDataPipe(&data_pipe_options, writable, readable);
  }

  std::unique_ptr<TestWebSocket> EstablishConnection(
      network::mojom::blink::WebSocketHandshakeClient* handshake_client,
      const String& selected_protocol,
      const String& extensions,
      mojo::ScopedDataPipeConsumerHandle readable,
      mojo::ScopedDataPipeProducerHandle writable,
      mojo::Remote<network::mojom::blink::WebSocketClient>* client) {
    mojo::PendingRemote<network::mojom::blink::WebSocketClient> client_remote;
    mojo::PendingRemote<network::mojom::blink::WebSocket> websocket_to_pass;
    auto websocket = std::make_unique<TestWebSocket>(
        websocket_to_pass.InitWithNewPipeAndPassReceiver());

    auto response = network::mojom::blink::WebSocketHandshakeResponse::New();
    response->http_version = network::mojom::blink::HttpVersion::New();
    response->status_text = "";
    response->headers_text = "";
    response->selected_protocol = selected_protocol;
    response->extensions = extensions;
    handshake_client->OnConnectionEstablished(
        std::move(websocket_to_pass),
        client_remote.InitWithNewPipeAndPassReceiver(), std::move(response),
        std::move(readable), std::move(writable));
    client->Bind(std::move(client_remote));
    return websocket;
  }

  void SetUp() override {
    local_frame_client_ = MakeGarbageCollected<EmptyLocalFrameClient>();
    local_frame_client_->GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::WebSocketConnector::Name_,
        base::BindRepeating(&WebSocketChannelImplTest::BindWebSocketConnector,
                            weak_ptr_factory_.GetWeakPtr()));

    PageTestBase::SetupPageWithClients(nullptr /* page_clients */,
                                       local_frame_client_.Get());
    const KURL page_url("http://example.com/");
    NavigateTo(page_url);
    channel_ = WebSocketChannelImpl::CreateForTesting(
        GetFrame().DomWindow(), channel_client_.Get(),
        SourceLocation::Capture(), std::move(handshake_throttle_));
  }

  void TearDown() override {
    local_frame_client_->GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::WebSocketConnector::Name_, {});
  }

  MockWebSocketChannelClient* ChannelClient() { return channel_client_.Get(); }

  WebSocketChannelImpl* Channel() { return channel_.Get(); }

  void DidConsumeBufferedAmount(uint64_t a) {
    sum_of_consumed_buffered_amount_ += a;
  }

  static Vector<uint8_t> AsVector(const char* data, size_t size) {
    Vector<uint8_t> v;
    v.Append(reinterpret_cast<const uint8_t*>(data), size);
    return v;
  }
  static Vector<uint8_t> AsVector(const char* data) {
    return AsVector(data, strlen(data));
  }

  // Returns nullptr if something bad happens.
  std::unique_ptr<TestWebSocket> Connect(
      uint32_t capacity,
      mojo::ScopedDataPipeProducerHandle* writable,
      mojo::ScopedDataPipeConsumerHandle* readable,
      mojo::Remote<network::mojom::blink::WebSocketClient>* client) {
    if (!Channel()->Connect(KURL("ws://localhost/"), "")) {
      ADD_FAILURE() << "WebSocketChannelImpl::Connect returns false.";
      return nullptr;
    }
    test::RunPendingTasks();
    auto connect_args = connector_.TakeConnectArgs();

    if (connect_args.size() != 1) {
      ADD_FAILURE() << "|connect_args.size()| is " << connect_args.size();
      return nullptr;
    }
    mojo::Remote<network::mojom::blink::WebSocketHandshakeClient>
        handshake_client(std::move(connect_args[0].handshake_client));

    mojo::ScopedDataPipeConsumerHandle remote_readable;
    if (CreateDataPipe(capacity, writable, &remote_readable) !=
        MOJO_RESULT_OK) {
      ADD_FAILURE() << "Failed to create a datapipe.";
      return nullptr;
    }

    mojo::ScopedDataPipeProducerHandle remote_writable;
    if (CreateDataPipe(capacity, &remote_writable, readable) !=
        MOJO_RESULT_OK) {
      ADD_FAILURE() << "Failed to create a datapipe.";
      return nullptr;
    }
    auto websocket = EstablishConnection(handshake_client.get(), "", "",
                                         std::move(remote_readable),
                                         std::move(remote_writable), client);
    test::RunPendingTasks();
    return websocket;
  }

  WebSocketConnector connector_;
  Persistent<EmptyLocalFrameClient> local_frame_client_;
  Persistent<MockWebSocketChannelClient> channel_client_;
  std::unique_ptr<MockWebSocketHandshakeThrottle> handshake_throttle_;
  MockWebSocketHandshakeThrottle* const raw_handshake_throttle_;
  Persistent<WebSocketChannelImpl> channel_;
  uint64_t sum_of_consumed_buffered_amount_;

  base::WeakPtrFactory<WebSocketChannelImplTest> weak_ptr_factory_;
};

class CallTrackingClosure {
 public:
  CallTrackingClosure() = default;

  base::OnceClosure Closure() {
    // This use of base::Unretained is safe because nothing can call the
    // callback once the test has finished.
    return WTF::Bind(&CallTrackingClosure::Called, base::Unretained(this));
  }

  bool WasCalled() const { return was_called_; }

 private:
  void Called() { was_called_ = true; }

  bool was_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(CallTrackingClosure);
};

std::ostream& operator<<(
    std::ostream& o,
    const WebSocketChannelImplTest::TestWebSocket::Frame& f) {
  return o << "fin = " << f.fin << ", type = " << f.type << ", data = (...)";
}

TEST_F(WebSocketChannelImplTest, ConnectSuccess) {
  Checkpoint checkpoint;

  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*ChannelClient(), DidConnect(String("a"), String("b")));
  }

  // Make sure that SiteForCookies() is set to the given value.
  EXPECT_TRUE(net::SiteForCookies::FromUrl(GURL("http://example.com/"))
                  .IsEquivalent(GetDocument().SiteForCookies()));

  ASSERT_TRUE(Channel()->Connect(KURL("ws://localhost/"), "x"));
  EXPECT_TRUE(connector_.GetConnectArgs().IsEmpty());

  test::RunPendingTasks();
  auto connect_args = connector_.TakeConnectArgs();

  ASSERT_EQ(1u, connect_args.size());
  EXPECT_EQ(connect_args[0].url, KURL("ws://localhost/"));
  EXPECT_TRUE(connect_args[0].site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(GURL("http://example.com/"))));

  EXPECT_EQ(connect_args[0].protocols, Vector<String>({"x"}));

  mojo::Remote<network::mojom::blink::WebSocketHandshakeClient>
      handshake_client(std::move(connect_args[0].handshake_client));

  mojo::ScopedDataPipeProducerHandle incoming_writable;
  mojo::ScopedDataPipeConsumerHandle incoming_readable;
  ASSERT_EQ(CreateDataPipe(32, &incoming_writable, &incoming_readable),
            MOJO_RESULT_OK);

  mojo::ScopedDataPipeProducerHandle outgoing_writable;
  mojo::ScopedDataPipeConsumerHandle outgoing_readable;
  ASSERT_EQ(CreateDataPipe(32, &outgoing_writable, &outgoing_readable),
            MOJO_RESULT_OK);

  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = EstablishConnection(handshake_client.get(), "a", "b",
                                       std::move(incoming_readable),
                                       std::move(outgoing_writable), &client);

  checkpoint.Call(1);
  test::RunPendingTasks();

  EXPECT_TRUE(websocket->IsStartReceivingCalled());
}

TEST_F(WebSocketChannelImplTest, MojoConnectionErrorDuringHandshake) {
  Checkpoint checkpoint;

  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(
        *ChannelClient(),
        DidClose(WebSocketChannelClient::kClosingHandshakeIncomplete,
                 WebSocketChannel::kCloseEventCodeAbnormalClosure, String()));
  }

  ASSERT_TRUE(Channel()->Connect(KURL("ws://localhost/"), "x"));
  EXPECT_TRUE(connector_.GetConnectArgs().IsEmpty());

  test::RunPendingTasks();
  auto connect_args = connector_.TakeConnectArgs();

  ASSERT_EQ(1u, connect_args.size());

  checkpoint.Call(1);
  // This destroys the PendingReceiver, which will be detected as a mojo
  // connection error.
  connect_args.clear();
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, SendText) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  Channel()->Send("foo", base::OnceClosure());
  Channel()->Send("bar", base::OnceClosure());
  Channel()->Send("baz", base::OnceClosure());

  test::RunPendingTasks();

  EXPECT_TRUE(websocket->GetFrames().IsEmpty());

  client->AddSendFlowControlQuota(16);

  EXPECT_TRUE(websocket->GetFrames().IsEmpty());
  test::RunPendingTasks();

  EXPECT_EQ(websocket->GetFrames(),
            (Frames{{true, WebSocketMessageType::TEXT, AsVector("foo")},
                    {true, WebSocketMessageType::TEXT, AsVector("bar")},
                    {true, WebSocketMessageType::TEXT, AsVector("baz")}}));

  EXPECT_EQ(9u, sum_of_consumed_buffered_amount_);
}

TEST_F(WebSocketChannelImplTest, SendTextContinuation) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  client->AddSendFlowControlQuota(16);

  Channel()->Send("0123456789abcdefg", base::OnceClosure());
  Channel()->Send("hijk", base::OnceClosure());
  Channel()->Send("lmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",
                  base::OnceClosure());
  test::RunPendingTasks();

  EXPECT_EQ(websocket->GetFrames(), (Frames{{false, WebSocketMessageType::TEXT,
                                             AsVector("0123456789abcdef")}}));

  websocket->ClearFrames();
  client->AddSendFlowControlQuota(16);
  test::RunPendingTasks();

  EXPECT_EQ(
      websocket->GetFrames(),
      (Frames{{true, WebSocketMessageType::CONTINUATION, AsVector("g")},
              {true, WebSocketMessageType::TEXT, AsVector("hijk")},
              {false, WebSocketMessageType::TEXT, AsVector("lmnopqrstuv")}}));

  websocket->ClearFrames();
  client->AddSendFlowControlQuota(16);
  test::RunPendingTasks();

  EXPECT_EQ(websocket->GetFrames(),
            (Frames{{false, WebSocketMessageType::CONTINUATION,
                     AsVector("wxyzABCDEFGHIJKL")}}));

  websocket->ClearFrames();
  client->AddSendFlowControlQuota(16);
  test::RunPendingTasks();

  EXPECT_EQ(websocket->GetFrames(),
            (Frames{{true, WebSocketMessageType::CONTINUATION,
                     AsVector("MNOPQRSTUVWXYZ")}}));

  EXPECT_EQ(62u, sum_of_consumed_buffered_amount_);
}

TEST_F(WebSocketChannelImplTest, SendBinaryInVector) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  client->AddSendFlowControlQuota(16);

  DOMArrayBuffer* foo_buffer = DOMArrayBuffer::Create("foo", 3);
  Channel()->Send(*foo_buffer, 0, 3, base::OnceClosure());
  test::RunPendingTasks();

  EXPECT_EQ(websocket->GetFrames(),
            (Frames{{true, WebSocketMessageType::BINARY, AsVector("foo")}}));

  EXPECT_EQ(3u, sum_of_consumed_buffered_amount_);
}

TEST_F(WebSocketChannelImplTest, SendBinaryInArrayBufferPartial) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  client->AddSendFlowControlQuota(16);

  DOMArrayBuffer* foobar_buffer = DOMArrayBuffer::Create("foobar", 6);
  DOMArrayBuffer* qbazux_buffer = DOMArrayBuffer::Create("qbazux", 6);
  Channel()->Send(*foobar_buffer, 0, 3, base::OnceClosure());
  Channel()->Send(*foobar_buffer, 3, 3, base::OnceClosure());
  Channel()->Send(*qbazux_buffer, 1, 3, base::OnceClosure());
  Channel()->Send(*qbazux_buffer, 2, 1, base::OnceClosure());

  test::RunPendingTasks();

  EXPECT_EQ(websocket->GetFrames(),
            (Frames{
                {true, WebSocketMessageType::BINARY, AsVector("foo")},
                {true, WebSocketMessageType::BINARY, AsVector("bar")},
                {true, WebSocketMessageType::BINARY, AsVector("baz")},
                {true, WebSocketMessageType::BINARY, AsVector("a")},
            }));

  EXPECT_EQ(10u, sum_of_consumed_buffered_amount_);
}

TEST_F(WebSocketChannelImplTest, SendBinaryInArrayBufferWithNullBytes) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  {
    DOMArrayBuffer* b = DOMArrayBuffer::Create("\0ar", 3);
    Channel()->Send(*b, 0, 3, base::OnceClosure());
  }
  {
    DOMArrayBuffer* b = DOMArrayBuffer::Create("b\0z", 3);
    Channel()->Send(*b, 0, 3, base::OnceClosure());
  }
  {
    DOMArrayBuffer* b = DOMArrayBuffer::Create("qu\0", 3);
    Channel()->Send(*b, 0, 3, base::OnceClosure());
  }
  {
    DOMArrayBuffer* b = DOMArrayBuffer::Create("\0\0\0", 3);
    Channel()->Send(*b, 0, 3, base::OnceClosure());
  }

  client->AddSendFlowControlQuota(16);
  test::RunPendingTasks();

  EXPECT_EQ(websocket->GetFrames(),
            (Frames{
                {true, WebSocketMessageType::BINARY, AsVector("\0ar", 3)},
                {true, WebSocketMessageType::BINARY, AsVector("b\0z", 3)},
                {true, WebSocketMessageType::BINARY, AsVector("qu\0", 3)},
                {true, WebSocketMessageType::BINARY, AsVector("\0\0\0", 3)},
            }));

  EXPECT_EQ(12u, sum_of_consumed_buffered_amount_);
}

TEST_F(WebSocketChannelImplTest, SendBinaryInArrayBufferNonLatin1UTF8) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  DOMArrayBuffer* b = DOMArrayBuffer::Create("\xe7\x8b\x90", 3);
  Channel()->Send(*b, 0, 3, base::OnceClosure());

  client->AddSendFlowControlQuota(16);
  test::RunPendingTasks();

  EXPECT_EQ(
      websocket->GetFrames(),
      (Frames{{true, WebSocketMessageType::BINARY, AsVector("\xe7\x8b\x90")}}));

  EXPECT_EQ(3u, sum_of_consumed_buffered_amount_);
}

TEST_F(WebSocketChannelImplTest, SendBinaryInArrayBufferNonUTF8) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  DOMArrayBuffer* b = DOMArrayBuffer::Create("\x80\xff\xe7", 3);
  Channel()->Send(*b, 0, 3, base::OnceClosure());

  client->AddSendFlowControlQuota(16);
  test::RunPendingTasks();

  EXPECT_EQ(
      websocket->GetFrames(),
      (Frames{{true, WebSocketMessageType::BINARY, AsVector("\x80\xff\xe7")}}));

  EXPECT_EQ(3ul, sum_of_consumed_buffered_amount_);
}

TEST_F(WebSocketChannelImplTest,
       SendBinaryInArrayBufferNonLatin1UTF8Continuation) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  DOMArrayBuffer* b = DOMArrayBuffer::Create(
      "\xe7\x8b\x90\xe7\x8b\x90\xe7\x8b\x90\xe7\x8b\x90\xe7\x8b\x90\xe7\x8b"
      "\x90",
      18);
  Channel()->Send(*b, 0, 18, base::OnceClosure());

  client->AddSendFlowControlQuota(16);
  test::RunPendingTasks();

  EXPECT_EQ(websocket->GetFrames(),
            (Frames{{false, WebSocketMessageType::BINARY,
                     AsVector("\xe7\x8b\x90\xe7\x8b\x90\xe7\x8b\x90\xe7\x8b\x90"
                              "\xe7\x8b\x90\xe7")}}));

  websocket->ClearFrames();
  client->AddSendFlowControlQuota(16);
  test::RunPendingTasks();

  EXPECT_EQ(websocket->GetFrames(),
            (Frames{{true, WebSocketMessageType::CONTINUATION,
                     AsVector("\x8b\x90")}}));

  EXPECT_EQ(18u, sum_of_consumed_buffered_amount_);
}

TEST_F(WebSocketChannelImplTest, SendTextSync) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  client->AddSendFlowControlQuota(5);
  test::RunPendingTasks();
  CallTrackingClosure closure;
  EXPECT_EQ(WebSocketChannel::SendResult::SENT_SYNCHRONOUSLY,
            Channel()->Send("hello", closure.Closure()));
  EXPECT_FALSE(closure.WasCalled());
}

TEST_F(WebSocketChannelImplTest, SendTextAsyncDueToQuota) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  client->AddSendFlowControlQuota(4);
  test::RunPendingTasks();

  CallTrackingClosure closure;
  EXPECT_EQ(WebSocketChannel::SendResult::CALLBACK_WILL_BE_CALLED,
            Channel()->Send("hello", closure.Closure()));
  EXPECT_FALSE(closure.WasCalled());

  client->AddSendFlowControlQuota(1);
  test::RunPendingTasks();

  EXPECT_TRUE(closure.WasCalled());
}

TEST_F(WebSocketChannelImplTest, SendTextAsyncDueToQueueing) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  client->AddSendFlowControlQuota(8);
  test::RunPendingTasks();

  // Ideally we'd use a Blob to block the queue in this test, but setting up a
  // working blob environment in a unit-test is complicated, so just block
  // behind a larger string instead.
  Channel()->Send("0123456789", base::OnceClosure());
  CallTrackingClosure closure;
  EXPECT_EQ(WebSocketChannel::SendResult::CALLBACK_WILL_BE_CALLED,
            Channel()->Send("hello", closure.Closure()));
  EXPECT_FALSE(closure.WasCalled());

  client->AddSendFlowControlQuota(7);
  test::RunPendingTasks();

  EXPECT_TRUE(closure.WasCalled());
}

TEST_F(WebSocketChannelImplTest, SendBinaryInArrayBufferSync) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  client->AddSendFlowControlQuota(5);
  test::RunPendingTasks();

  CallTrackingClosure closure;
  const auto* b = DOMArrayBuffer::Create("hello", 5);
  EXPECT_EQ(WebSocketChannel::SendResult::SENT_SYNCHRONOUSLY,
            Channel()->Send(*b, 0, 5, closure.Closure()));
  EXPECT_FALSE(closure.WasCalled());
}

TEST_F(WebSocketChannelImplTest, SendBinaryInArrayBufferAsyncDueToQuota) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  client->AddSendFlowControlQuota(4);
  test::RunPendingTasks();

  CallTrackingClosure closure;
  const auto* b = DOMArrayBuffer::Create("hello", 5);
  EXPECT_EQ(WebSocketChannel::SendResult::CALLBACK_WILL_BE_CALLED,
            Channel()->Send(*b, 0, 5, closure.Closure()));
  EXPECT_FALSE(closure.WasCalled());

  client->AddSendFlowControlQuota(1);
  test::RunPendingTasks();

  EXPECT_TRUE(closure.WasCalled());
}

TEST_F(WebSocketChannelImplTest, SendBinaryInArrayBufferAsyncDueToQueueing) {
  EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  EXPECT_CALL(*ChannelClient(), DidConsumeBufferedAmount(_)).Times(AnyNumber());

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  client->AddSendFlowControlQuota(8);
  test::RunPendingTasks();

  Channel()->Send("0123456789", base::OnceClosure());
  CallTrackingClosure closure;
  const auto* b = DOMArrayBuffer::Create("hello", 5);
  EXPECT_EQ(WebSocketChannel::SendResult::CALLBACK_WILL_BE_CALLED,
            Channel()->Send(*b, 0, 5, closure.Closure()));

  EXPECT_FALSE(closure.WasCalled());

  client->AddSendFlowControlQuota(8);
  test::RunPendingTasks();

  EXPECT_TRUE(closure.WasCalled());
}

// FIXME: Add tests for WebSocketChannel::send(scoped_refptr<BlobDataHandle>)

TEST_F(WebSocketChannelImplTest, ReceiveText) {
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("FOO")));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("BAR")));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  uint32_t num_bytes = 6;
  ASSERT_EQ(MOJO_RESULT_OK, writable->WriteData("FOOBAR", &num_bytes,
                                                MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(num_bytes, 6u);

  client->OnDataFrame(true, WebSocketMessageType::TEXT, 3);
  client->OnDataFrame(true, WebSocketMessageType::TEXT, 3);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ReceiveTextContinuation) {
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("BAZ")));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  uint32_t num_bytes = 3;
  ASSERT_EQ(MOJO_RESULT_OK,
            writable->WriteData("BAZ", &num_bytes, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(num_bytes, 3u);

  client->OnDataFrame(false, WebSocketMessageType::TEXT, 1);
  client->OnDataFrame(false, WebSocketMessageType::CONTINUATION, 1);
  client->OnDataFrame(true, WebSocketMessageType::CONTINUATION, 1);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ReceiveTextNonLatin1) {
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    UChar non_latin1_string[] = {0x72d0, 0x0914, 0x0000};
    EXPECT_CALL(*ChannelClient(),
                DidReceiveTextMessage(String(non_latin1_string)));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  uint32_t num_bytes = 6;
  ASSERT_EQ(MOJO_RESULT_OK,
            writable->WriteData("\xe7\x8b\x90\xe0\xa4\x94", &num_bytes,
                                MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(num_bytes, 6u);

  client->OnDataFrame(true, WebSocketMessageType::TEXT, 6);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ReceiveTextNonLatin1Continuation) {
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    UChar non_latin1_string[] = {0x72d0, 0x0914, 0x0000};
    EXPECT_CALL(*ChannelClient(),
                DidReceiveTextMessage(String(non_latin1_string)));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  uint32_t num_bytes = 6;
  ASSERT_EQ(MOJO_RESULT_OK,
            writable->WriteData("\xe7\x8b\x90\xe0\xa4\x94", &num_bytes,
                                MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(num_bytes, 6u);

  client->OnDataFrame(false, WebSocketMessageType::TEXT, 2);
  client->OnDataFrame(false, WebSocketMessageType::CONTINUATION, 2);
  client->OnDataFrame(false, WebSocketMessageType::CONTINUATION, 1);
  client->OnDataFrame(true, WebSocketMessageType::CONTINUATION, 1);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ReceiveBinary) {
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(*ChannelClient(),
                DidReceiveBinaryMessageMock((Vector<char>{'F', 'O', 'O'})));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  uint32_t num_bytes = 3;
  ASSERT_EQ(MOJO_RESULT_OK,
            writable->WriteData("FOO", &num_bytes, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(num_bytes, 3u);

  client->OnDataFrame(true, WebSocketMessageType::BINARY, 3);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ReceiveBinaryContinuation) {
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(*ChannelClient(),
                DidReceiveBinaryMessageMock((Vector<char>{'B', 'A', 'Z'})));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  uint32_t num_bytes = 3;
  ASSERT_EQ(MOJO_RESULT_OK,
            writable->WriteData("BAZ", &num_bytes, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(num_bytes, 3u);

  client->OnDataFrame(false, WebSocketMessageType::BINARY, 1);
  client->OnDataFrame(false, WebSocketMessageType::CONTINUATION, 1);
  client->OnDataFrame(true, WebSocketMessageType::CONTINUATION, 1);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ReceiveBinaryWithNullBytes) {
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(*ChannelClient(),
                DidReceiveBinaryMessageMock((Vector<char>{'\0', 'A', '3'})));
    EXPECT_CALL(*ChannelClient(),
                DidReceiveBinaryMessageMock((Vector<char>{'B', '\0', 'Z'})));
    EXPECT_CALL(*ChannelClient(),
                DidReceiveBinaryMessageMock((Vector<char>{'Q', 'U', '\0'})));
    EXPECT_CALL(*ChannelClient(),
                DidReceiveBinaryMessageMock((Vector<char>{'\0', '\0', '\0'})));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  uint32_t num_bytes = 12;
  ASSERT_EQ(MOJO_RESULT_OK,
            writable->WriteData("\0A3B\0ZQU\0\0\0\0", &num_bytes,
                                MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(num_bytes, 12u);

  client->OnDataFrame(true, WebSocketMessageType::BINARY, 3);
  client->OnDataFrame(true, WebSocketMessageType::BINARY, 3);
  client->OnDataFrame(true, WebSocketMessageType::BINARY, 3);
  client->OnDataFrame(true, WebSocketMessageType::BINARY, 3);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ReceiveBinaryNonLatin1UTF8) {
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(*ChannelClient(),
                DidReceiveBinaryMessageMock((Vector<char>{
                    '\xe7', '\x8b', '\x90', '\xe0', '\xa4', '\x94'})));
  }
  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  uint32_t num_bytes = 6;
  ASSERT_EQ(MOJO_RESULT_OK,
            writable->WriteData("\xe7\x8b\x90\xe0\xa4\x94", &num_bytes,
                                MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(num_bytes, 6u);

  client->OnDataFrame(true, WebSocketMessageType::BINARY, 6);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ReceiveBinaryNonLatin1UTF8Continuation) {
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(*ChannelClient(),
                DidReceiveBinaryMessageMock((Vector<char>{
                    '\xe7', '\x8b', '\x90', '\xe0', '\xa4', '\x94'})));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  uint32_t num_bytes = 6;
  ASSERT_EQ(MOJO_RESULT_OK,
            writable->WriteData("\xe7\x8b\x90\xe0\xa4\x94", &num_bytes,
                                MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(num_bytes, 6u);

  client->OnDataFrame(false, WebSocketMessageType::BINARY, 2);
  client->OnDataFrame(false, WebSocketMessageType::CONTINUATION, 2);
  client->OnDataFrame(false, WebSocketMessageType::CONTINUATION, 1);
  client->OnDataFrame(true, WebSocketMessageType::CONTINUATION, 1);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ReceiveBinaryNonUTF8) {
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(*ChannelClient(),
                DidReceiveBinaryMessageMock((Vector<char>{'\x80', '\xff'})));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  uint32_t num_bytes = 2;
  ASSERT_EQ(MOJO_RESULT_OK, writable->WriteData("\x80\xff", &num_bytes,
                                                MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(num_bytes, 2u);

  client->OnDataFrame(true, WebSocketMessageType::BINARY, 2);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ReceiveWithExplicitBackpressure) {
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("abc")));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  uint32_t num_bytes = 3;
  ASSERT_EQ(MOJO_RESULT_OK,
            writable->WriteData("abc", &num_bytes, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(num_bytes, 3u);

  Channel()->ApplyBackpressure();

  client->OnDataFrame(true, WebSocketMessageType::TEXT, 3);
  test::RunPendingTasks();

  checkpoint.Call(1);
  Channel()->RemoveBackpressure();
}

TEST_F(WebSocketChannelImplTest,
       ReceiveMultipleMessagesWithSmallDataPipeWrites) {
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("abc")));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("")));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("")));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("de")));
    EXPECT_CALL(checkpoint, Call(4));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("")));
    EXPECT_CALL(checkpoint, Call(5));
    EXPECT_CALL(checkpoint, Call(6));
    EXPECT_CALL(*ChannelClient(), DidReceiveTextMessage(String("fghijkl")));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  client->OnDataFrame(true, WebSocketMessageType::TEXT, 3);
  client->OnDataFrame(true, WebSocketMessageType::TEXT, 0);
  client->OnDataFrame(true, WebSocketMessageType::TEXT, 0);
  client->OnDataFrame(true, WebSocketMessageType::TEXT, 2);
  test::RunPendingTasks();

  checkpoint.Call(1);
  uint32_t num_bytes = 2;
  ASSERT_EQ(MOJO_RESULT_OK,
            writable->WriteData("ab", &num_bytes, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(num_bytes, 2u);
  test::RunPendingTasks();

  checkpoint.Call(2);
  num_bytes = 2;
  ASSERT_EQ(MOJO_RESULT_OK,
            writable->WriteData("cd", &num_bytes, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(num_bytes, 2u);
  test::RunPendingTasks();

  checkpoint.Call(3);
  num_bytes = 4;
  ASSERT_EQ(MOJO_RESULT_OK,
            writable->WriteData("efgh", &num_bytes, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(num_bytes, 4u);
  test::RunPendingTasks();

  checkpoint.Call(4);
  client->OnDataFrame(true, WebSocketMessageType::TEXT, 0);
  client->OnDataFrame(false, WebSocketMessageType::TEXT, 1);
  test::RunPendingTasks();

  checkpoint.Call(5);
  client->OnDataFrame(false, WebSocketMessageType::CONTINUATION, 1);
  client->OnDataFrame(true, WebSocketMessageType::CONTINUATION, 5);
  test::RunPendingTasks();

  checkpoint.Call(6);
  num_bytes = 4;
  ASSERT_EQ(MOJO_RESULT_OK,
            writable->WriteData("ijkl", &num_bytes, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(num_bytes, 4u);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ConnectionCloseInitiatedByServer) {
  Checkpoint checkpoint;
  {
    InSequence s;

    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(*ChannelClient(), DidStartClosingHandshake());
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(checkpoint, Call(2));

    EXPECT_CALL(*ChannelClient(),
                DidClose(WebSocketChannelClient::kClosingHandshakeComplete,
                         WebSocketChannel::kCloseEventCodeNormalClosure,
                         String("close reason")));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  client->OnClosingHandshake();
  test::RunPendingTasks();

  EXPECT_FALSE(websocket->IsStartClosingHandshakeCalled());

  checkpoint.Call(1);
  Channel()->Close(WebSocketChannel::kCloseEventCodeNormalClosure,
                   "close reason");
  test::RunPendingTasks();

  EXPECT_TRUE(websocket->IsStartClosingHandshakeCalled());
  EXPECT_EQ(websocket->GetClosingCode(),
            WebSocketChannel::kCloseEventCodeNormalClosure);
  EXPECT_EQ(websocket->GetClosingReason(), "close reason");

  checkpoint.Call(2);
  client->OnDropChannel(true, WebSocketChannel::kCloseEventCodeNormalClosure,
                        "close reason");
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, ConnectionCloseInitiatedByClient) {
  Checkpoint checkpoint;
  {
    InSequence s;

    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*ChannelClient(),
                DidClose(WebSocketChannelClient::kClosingHandshakeComplete,
                         WebSocketChannel::kCloseEventCodeNormalClosure,
                         String("close reason")));
    EXPECT_CALL(checkpoint, Call(2));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  EXPECT_FALSE(websocket->IsStartClosingHandshakeCalled());
  Channel()->Close(WebSocketChannel::kCloseEventCodeNormalClosure,
                   "close reason");
  test::RunPendingTasks();
  EXPECT_TRUE(websocket->IsStartClosingHandshakeCalled());
  EXPECT_EQ(websocket->GetClosingCode(),
            WebSocketChannel::kCloseEventCodeNormalClosure);
  EXPECT_EQ(websocket->GetClosingReason(), "close reason");

  checkpoint.Call(1);
  client->OnDropChannel(true, WebSocketChannel::kCloseEventCodeNormalClosure,
                        "close reason");
  test::RunPendingTasks();
  checkpoint.Call(2);
}

TEST_F(WebSocketChannelImplTest, MojoConnectionError) {
  Checkpoint checkpoint;
  {
    InSequence s;

    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(
        *ChannelClient(),
        DidClose(WebSocketChannelClient::kClosingHandshakeIncomplete,
                 WebSocketChannel::kCloseEventCodeAbnormalClosure, String()));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  // Send a frame so that the WebSocketChannelImpl try to read the data pipe.
  client->OnDataFrame(true, WebSocketMessageType::TEXT, 1024);

  // We shouldn't detect a connection error on data pipes and mojom::WebSocket.
  writable.reset();
  websocket = nullptr;
  test::RunPendingTasks();

  // We should detect a connection error on the client.
  checkpoint.Call(1);
  client.reset();
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplTest, FailFromClient) {
  {
    InSequence s;

    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(
        *ChannelClient(),
        DidClose(WebSocketChannelClient::kClosingHandshakeIncomplete,
                 WebSocketChannel::kCloseEventCodeAbnormalClosure, String()));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  Channel()->Fail("fail message from WebSocket",
                  mojom::ConsoleMessageLevel::kError,
                  std::make_unique<SourceLocation>(String(), 0, 0, nullptr));
}

class WebSocketChannelImplHandshakeThrottleTest
    : public WebSocketChannelImplTest {
 public:
  WebSocketChannelImplHandshakeThrottleTest()
      : WebSocketChannelImplTest(
            std::make_unique<
                testing::StrictMock<MockWebSocketHandshakeThrottle>>()) {}

  static KURL url() { return KURL("ws://localhost/"); }
};

TEST_F(WebSocketChannelImplHandshakeThrottleTest, ThrottleSucceedsFirst) {
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*raw_handshake_throttle_, ThrottleHandshake(_, _));
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*raw_handshake_throttle_, Destructor());
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  }

  ASSERT_TRUE(Channel()->Connect(url(), ""));
  test::RunPendingTasks();

  auto connect_args = connector_.TakeConnectArgs();

  ASSERT_EQ(1u, connect_args.size());

  mojo::Remote<network::mojom::blink::WebSocketHandshakeClient>
      handshake_client(std::move(connect_args[0].handshake_client));
  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  ASSERT_EQ(CreateDataPipe(32, &writable, &readable), MOJO_RESULT_OK);

  mojo::ScopedDataPipeProducerHandle outgoing_writable;
  mojo::ScopedDataPipeConsumerHandle outgoing_readable;
  ASSERT_EQ(CreateDataPipe(32, &outgoing_writable, &outgoing_readable),
            MOJO_RESULT_OK);

  mojo::Remote<network::mojom::blink::WebSocketClient> client;

  checkpoint.Call(1);
  test::RunPendingTasks();

  Channel()->OnCompletion(base::nullopt);
  checkpoint.Call(2);

  auto websocket =
      EstablishConnection(handshake_client.get(), "", "", std::move(readable),
                          std::move(outgoing_writable), &client);
  test::RunPendingTasks();
}

TEST_F(WebSocketChannelImplHandshakeThrottleTest, HandshakeSucceedsFirst) {
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(*raw_handshake_throttle_, ThrottleHandshake(_, _));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(*raw_handshake_throttle_, Destructor());
    EXPECT_CALL(*ChannelClient(), DidConnect(_, _));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;

  checkpoint.Call(1);
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  checkpoint.Call(2);
  Channel()->OnCompletion(base::nullopt);
}

// This happens if JS code calls close() during the handshake.
TEST_F(WebSocketChannelImplHandshakeThrottleTest, FailDuringThrottle) {
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*raw_handshake_throttle_, ThrottleHandshake(_, _));
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(*ChannelClient(), DidClose(_, _, _));
    EXPECT_CALL(*raw_handshake_throttle_, Destructor());
    EXPECT_CALL(checkpoint, Call(1));
  }

  Channel()->Connect(url(), "");
  Channel()->Fail("close during handshake",
                  mojom::ConsoleMessageLevel::kWarning,
                  std::make_unique<SourceLocation>(String(), 0, 0, nullptr));
  checkpoint.Call(1);
}

// It makes no difference to the behaviour if the WebSocketHandle has actually
// connected.
TEST_F(WebSocketChannelImplHandshakeThrottleTest,
       FailDuringThrottleAfterConnect) {
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*raw_handshake_throttle_, ThrottleHandshake(_, _));
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(*ChannelClient(), DidClose(_, _, _));
    EXPECT_CALL(*raw_handshake_throttle_, Destructor());
    EXPECT_CALL(checkpoint, Call(1));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  Channel()->Fail("close during handshake",
                  mojom::ConsoleMessageLevel::kWarning,
                  std::make_unique<SourceLocation>(String(), 0, 0, nullptr));
  checkpoint.Call(1);
}

TEST_F(WebSocketChannelImplHandshakeThrottleTest, DisconnectDuringThrottle) {
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*raw_handshake_throttle_, ThrottleHandshake(_, _));
    EXPECT_CALL(*raw_handshake_throttle_, Destructor());
    EXPECT_CALL(checkpoint, Call(1));
  }

  Channel()->Connect(url(), "");
  test::RunPendingTasks();

  Channel()->Disconnect();
  checkpoint.Call(1);

  auto connect_args = connector_.TakeConnectArgs();
  ASSERT_EQ(1u, connect_args.size());

  mojo::Remote<network::mojom::blink::WebSocketHandshakeClient>
      handshake_client(std::move(connect_args[0].handshake_client));

  CallTrackingClosure closure;
  handshake_client.set_disconnect_handler(closure.Closure());
  EXPECT_FALSE(closure.WasCalled());

  test::RunPendingTasks();

  EXPECT_TRUE(closure.WasCalled());
}

TEST_F(WebSocketChannelImplHandshakeThrottleTest,
       DisconnectDuringThrottleAfterConnect) {
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(*raw_handshake_throttle_, ThrottleHandshake(_, _));
    EXPECT_CALL(*raw_handshake_throttle_, Destructor());
    EXPECT_CALL(checkpoint, Call(1));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  Channel()->Disconnect();
  checkpoint.Call(1);

  CallTrackingClosure closure;
  client.set_disconnect_handler(closure.Closure());
  EXPECT_FALSE(closure.WasCalled());

  test::RunPendingTasks();

  EXPECT_TRUE(closure.WasCalled());
}

TEST_F(WebSocketChannelImplHandshakeThrottleTest,
       ThrottleReportsErrorBeforeConnect) {
  {
    InSequence s;
    EXPECT_CALL(*raw_handshake_throttle_, ThrottleHandshake(_, _));
    EXPECT_CALL(*raw_handshake_throttle_, Destructor());
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(*ChannelClient(), DidClose(_, _, _));
  }

  Channel()->Connect(url(), "");
  Channel()->OnCompletion("Connection blocked by throttle");
}

TEST_F(WebSocketChannelImplHandshakeThrottleTest,
       ThrottleReportsErrorAfterConnect) {
  {
    InSequence s;
    EXPECT_CALL(*raw_handshake_throttle_, ThrottleHandshake(_, _));
    EXPECT_CALL(*raw_handshake_throttle_, Destructor());
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(*ChannelClient(), DidClose(_, _, _));
  }

  mojo::ScopedDataPipeProducerHandle writable;
  mojo::ScopedDataPipeConsumerHandle readable;
  mojo::Remote<network::mojom::blink::WebSocketClient> client;
  auto websocket = Connect(4 * 1024, &writable, &readable, &client);
  ASSERT_TRUE(websocket);

  Channel()->OnCompletion("Connection blocked by throttle");
}

TEST_F(WebSocketChannelImplHandshakeThrottleTest, ConnectFailBeforeThrottle) {
  {
    InSequence s;
    EXPECT_CALL(*raw_handshake_throttle_, ThrottleHandshake(_, _));
    EXPECT_CALL(*ChannelClient(), DidError());
    EXPECT_CALL(*ChannelClient(), DidClose(_, _, _));
    EXPECT_CALL(*raw_handshake_throttle_, Destructor());
  }

  ASSERT_TRUE(Channel()->Connect(url(), ""));
  test::RunPendingTasks();

  auto connect_args = connector_.TakeConnectArgs();

  ASSERT_EQ(1u, connect_args.size());

  connect_args.clear();
  test::RunPendingTasks();
}

}  // namespace blink
