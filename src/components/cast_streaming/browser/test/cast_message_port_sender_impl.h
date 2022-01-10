// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_TEST_CAST_MESSAGE_PORT_SENDER_IMPL_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_TEST_CAST_MESSAGE_PORT_SENDER_IMPL_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/cast/message_port/message_port.h"
#include "third_party/openscreen/src/cast/common/public/message_port.h"

namespace cast_streaming {

// Adapter for a cast MessagePort that provides an Open Screen MessagePort
// implementation for a Cast Streaming Sender.
class CastMessagePortSenderImpl final
    : public openscreen::cast::MessagePort,
      public cast_api_bindings::MessagePort::Receiver {
 public:
  explicit CastMessagePortSenderImpl(
      std::unique_ptr<cast_api_bindings::MessagePort> message_port,
      base::OnceClosure on_close);
  ~CastMessagePortSenderImpl() override;

  CastMessagePortSenderImpl(const CastMessagePortSenderImpl&) = delete;
  CastMessagePortSenderImpl& operator=(const CastMessagePortSenderImpl&) =
      delete;

  // openscreen::cast::MessagePort implementation.
  void SetClient(Client* client, std::string client_sender_id) override;
  void ResetClient() override;
  void PostMessage(const std::string& sender_id,
                   const std::string& message_namespace,
                   const std::string& message) override;

 private:
  // Resets |message_port_| if it is open and signals an error to |client_| if
  // |client_| is set.
  void MaybeClose();

  // cast_api_bindings::MessagePort::Receiver implementation.
  bool OnMessage(base::StringPiece message,
                 std::vector<std::unique_ptr<cast_api_bindings::MessagePort>>
                     ports) override;
  void OnPipeError() override;

  raw_ptr<Client> client_ = nullptr;
  std::unique_ptr<cast_api_bindings::MessagePort> message_port_;
  base::OnceClosure on_close_;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_TEST_CAST_MESSAGE_PORT_SENDER_IMPL_H_
