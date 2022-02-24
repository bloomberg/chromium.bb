// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/udp_readable_stream_wrapper.h"

#include "base/callback_helpers.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_udp_message.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_writable_stream_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {
namespace {

class FakeDirectUDPSocket : public blink::mojom::blink::DirectUDPSocket {
 public:
  void Send(base::span<const uint8_t> data, SendCallback callback) override {
    NOTIMPLEMENTED();
  }

  void ReceiveMore(uint32_t num_additional_datagrams) override {
    num_requested_datagrams += num_additional_datagrams;
  }

  void Close() override { NOTIMPLEMENTED(); }

  void ProvideRequestedDatagrams(UDPReadableStreamWrapper* stream) {
    while (num_requested_datagrams > 0) {
      stream->AcceptDatagram(
          datagram_.Span8(),
          net::IPEndPoint{net::IPAddress::IPv4Localhost(), 0U});
      num_requested_datagrams--;
    }
  }

  const String& GetTestingDatagram() const { return datagram_; }
  void SetTestingDatagram(String datagram) { datagram_ = std::move(datagram); }

 private:
  uint32_t num_requested_datagrams = 0;
  String datagram_{"abcde"};
};

class StreamCreator {
  STACK_ALLOCATED();

 public:
  explicit StreamCreator(const V8TestingScope& scope,
                         FakeDirectUDPSocket* fake_udp_socket)
      : scope_(scope), receiver_{fake_udp_socket} {}

  ~StreamCreator() { test::RunPendingTasks(); }

  UDPReadableStreamWrapper* Create() {
    auto* udp_socket =
        MakeGarbageCollected<UDPSocketMojoRemote>(scope_.GetExecutionContext());
    udp_socket->get().Bind(
        receiver_.BindNewPipeAndPassRemote(),
        scope_.GetExecutionContext()->GetTaskRunner(TaskType::kNetworking));

    auto* script_state = scope_.GetScriptState();
    auto* udp_readable_stream_wrapper =
        MakeGarbageCollected<UDPReadableStreamWrapper>(script_state, udp_socket,
                                                       base::DoNothing());
    return udp_readable_stream_wrapper;
  }

 private:
  const V8TestingScope& scope_;
  mojo::Receiver<blink::mojom::blink::DirectUDPSocket> receiver_;
};

std::pair<UDPMessage*, bool> UnpackPromiseResult(const V8TestingScope& scope,
                                                 v8::Local<v8::Value> result) {
  // js call looks like this:
  // let { value, done } = await reader.read();
  // So we have to unpack the iterator first.
  EXPECT_TRUE(result->IsObject());
  v8::Local<v8::Value> udp_message_packed;
  bool done = false;
  EXPECT_TRUE(V8UnpackIteratorResult(scope.GetScriptState(),
                                     result.As<v8::Object>(), &done)
                  .ToLocal(&udp_message_packed));
  if (done) {
    return {nullptr, true};
  }
  auto* message = NativeValueTraits<UDPMessage>::NativeValue(
      scope.GetIsolate(), udp_message_packed, ASSERT_NO_EXCEPTION);

  return {message, false};
}

String UDPMessageDataToString(const UDPMessage* message) {
  DOMArrayPiece array_piece{message->data()};
  return String{static_cast<const uint8_t*>(array_piece.Bytes()),
                static_cast<wtf_size_t>(array_piece.ByteLength())};
}

TEST(UDPReadableStreamWrapperTest, Create) {
  V8TestingScope scope;
  FakeDirectUDPSocket fake_udp_socket;
  StreamCreator stream_creator{scope, &fake_udp_socket};
  auto* udp_readable_stream_wrapper = stream_creator.Create();
  EXPECT_TRUE(udp_readable_stream_wrapper->Readable());
}

TEST(UDPReadableStreamWrapperTest, ReadUdpMessage) {
  V8TestingScope scope;
  FakeDirectUDPSocket fake_udp_socket;
  StreamCreator stream_creator{scope, &fake_udp_socket};
  auto* script_state = scope.GetScriptState();

  auto* udp_readable_stream_wrapper = stream_creator.Create();
  // Ensure that udp_socket_->ReceiveMore(...) call from
  // UDPReadableStreamWrapper constructor lands before calling
  // fake_udp_socket.ProvideRequestedDiagrams().
  test::RunPendingTasks();

  fake_udp_socket.ProvideRequestedDatagrams(udp_readable_stream_wrapper);

  auto* reader =
      udp_readable_stream_wrapper->Readable()->GetDefaultReaderForTesting(
          script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseTester tester(script_state,
                             reader->read(script_state, ASSERT_NO_EXCEPTION));
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  auto [message, done] = UnpackPromiseResult(scope, tester.Value().V8Value());
  ASSERT_FALSE(done);
  ASSERT_TRUE(message->hasData());
  ASSERT_EQ(UDPMessageDataToString(message),
            fake_udp_socket.GetTestingDatagram());
}

TEST(UDPReadableStreamWrapperTest, ReadDelayedUdpMessage) {
  V8TestingScope scope;
  FakeDirectUDPSocket fake_udp_socket;
  StreamCreator stream_creator{scope, &fake_udp_socket};
  auto* script_state = scope.GetScriptState();

  auto* udp_readable_stream_wrapper = stream_creator.Create();
  // Ensure that udp_socket_->ReceiveMore(...) call from
  // UDPReadableStreamWrapper constructor lands before calling
  // fake_udp_socket.ProvideRequestedDiagrams().
  test::RunPendingTasks();

  auto* reader =
      udp_readable_stream_wrapper->Readable()->GetDefaultReaderForTesting(
          script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state,
                             reader->read(script_state, ASSERT_NO_EXCEPTION));

  fake_udp_socket.ProvideRequestedDatagrams(udp_readable_stream_wrapper);

  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  auto [message, done] = UnpackPromiseResult(scope, tester.Value().V8Value());
  ASSERT_FALSE(done);
  ASSERT_TRUE(message->hasData());
  ASSERT_EQ(UDPMessageDataToString(message),
            fake_udp_socket.GetTestingDatagram());
}

TEST(UDPReadableStreamWrapperTest, ReadEmptyUdpMessage) {
  V8TestingScope scope;
  FakeDirectUDPSocket fake_udp_socket;
  StreamCreator stream_creator{scope, &fake_udp_socket};
  auto* script_state = scope.GetScriptState();

  auto* udp_readable_stream_wrapper = stream_creator.Create();
  // Ensure that udp_socket_->ReceiveMore(...) call from
  // UDPReadableStreamWrapper constructor lands before calling
  // fake_udp_socket.ProvideRequestedDiagrams().
  test::RunPendingTasks();

  // Send empty datagrams.
  fake_udp_socket.SetTestingDatagram({});
  fake_udp_socket.ProvideRequestedDatagrams(udp_readable_stream_wrapper);

  auto* reader =
      udp_readable_stream_wrapper->Readable()->GetDefaultReaderForTesting(
          script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseTester tester(script_state,
                             reader->read(script_state, ASSERT_NO_EXCEPTION));
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  auto [message, done] = UnpackPromiseResult(scope, tester.Value().V8Value());
  ASSERT_FALSE(done);
  ASSERT_TRUE(message->hasData());

  ASSERT_EQ(UDPMessageDataToString(message).length(), 0U);
}

TEST(UDPReadableStreamWrapperTest, CancelStreamFromReader) {
  V8TestingScope scope;
  FakeDirectUDPSocket fake_udp_socket;
  StreamCreator stream_creator{scope, &fake_udp_socket};
  auto* script_state = scope.GetScriptState();

  auto* udp_readable_stream_wrapper = stream_creator.Create();
  // Ensure that udp_socket_->ReceiveMore(...) call from
  // UDPReadableStreamWrapper constructor lands before calling
  // fake_udp_socket.ProvideRequestedDiagrams().
  test::RunPendingTasks();

  auto* reader =
      udp_readable_stream_wrapper->Readable()->GetDefaultReaderForTesting(
          script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseTester cancel_tester(
      script_state, reader->cancel(script_state, ASSERT_NO_EXCEPTION));
  cancel_tester.WaitUntilSettled();
  EXPECT_TRUE(cancel_tester.IsFulfilled());

  ScriptPromiseTester read_tester(
      script_state, reader->read(script_state, ASSERT_NO_EXCEPTION));
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsFulfilled());

  auto [message, done] =
      UnpackPromiseResult(scope, read_tester.Value().V8Value());

  EXPECT_TRUE(done);
  EXPECT_FALSE(message);
}

TEST(UDPReadableStreamWrapperTest, CancelStreamFromWrapper) {
  V8TestingScope scope;
  FakeDirectUDPSocket fake_udp_socket;
  StreamCreator stream_creator{scope, &fake_udp_socket};
  auto* script_state = scope.GetScriptState();

  auto* udp_readable_stream_wrapper = stream_creator.Create();
  // Ensure that udp_socket_->ReceiveMore(...) call from
  // UDPReadableStreamWrapper constructor lands before calling
  // fake_udp_socket.ProvideRequestedDiagrams().
  test::RunPendingTasks();

  auto* reader =
      udp_readable_stream_wrapper->Readable()->GetDefaultReaderForTesting(
          script_state, ASSERT_NO_EXCEPTION);

  udp_readable_stream_wrapper->Close();

  ScriptPromiseTester read_tester(
      script_state, reader->read(script_state, ASSERT_NO_EXCEPTION));
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsFulfilled());

  auto [message, done] =
      UnpackPromiseResult(scope, read_tester.Value().V8Value());

  EXPECT_TRUE(done);
  EXPECT_FALSE(message);
}

}  // namespace

}  // namespace blink
