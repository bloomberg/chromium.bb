// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_TEST_TEST_TRANSPORT_LISTENER_H_
#define IPCZ_SRC_TEST_TEST_TRANSPORT_LISTENER_H_

#include <functional>
#include <string_view>

#include "ipcz/driver_transport.h"
#include "util/ref_counted.h"

namespace ipcz::test {

// Helper for tests to conveniently listen for incoming messages on a driver
// transport.
class TestTransportListener : public DriverTransport::Listener {
 public:
  using GenericMessageHandler =
      std::function<IpczResult(const DriverTransport::Message&)>;
  using ErrorHandler = std::function<void()>;

  TestTransportListener(IpczHandle node, IpczDriverHandle handle);
  explicit TestTransportListener(Ref<DriverTransport> transport);
  ~TestTransportListener() override;

  // Deactivates the transport and stops listening for messages and/or errors.
  void StopListening();

  // NOTE: The message handlers below are mutually exclusive, and all of them
  // activate the transport if it's not already active.

  // Invokes `handler` every time a message is received on the transport.
  void OnRawMessage(GenericMessageHandler handler);

  // Invokes `handler` every time a message is received on the transport.
  // Messages are expected to have no driver handles attached, and message data
  // is exposed to `handler` as string data.
  void OnStringMessage(std::function<void(std::string_view)> handler);

  // Invokes `handler` when the transport encounters an error, such as
  // disconnection or validation failure. Activates the transport if not already
  // active.
  void OnError(ErrorHandler handler);

  template <typename MessageType>
  using MessageHandler = std::function<IpczResult(MessageType&)>;

  // Listens only for a specific type of message and passes its deserialized
  // representation to `handler`. Any other message is failure.
  template <typename MessageType>
  void OnMessage(MessageHandler<MessageType> handler) {
    OnRawMessage([this, handler](const DriverTransport::Message& message) {
      MessageType m;
      EXPECT_TRUE(m.Deserialize(message, *transport_));
      return handler(m);
    });
  }

  // Listens for a specific type of message, deserializing and discarding them
  // when they arrive.
  template <typename MessageType>
  void DiscardMessages() {
    OnMessage<MessageType>([](MessageType& message) { return IPCZ_RESULT_OK; });
  }

 private:
  void ActivateTransportIfNecessary();

  // DriverTransport::Listener:
  IpczResult OnTransportMessage(
      const DriverTransport::Message& message) override;
  void OnTransportError() override;

  const Ref<DriverTransport> transport_;
  bool activated_ = false;
  GenericMessageHandler message_handler_;
  ErrorHandler error_handler_;
};

}  // namespace ipcz::test

#endif  // IPCZ_SRC_TEST_TEST_TRANSPORT_LISTENER_H_
