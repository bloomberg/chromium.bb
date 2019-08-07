// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_MESSAGE_PORT_IMPL_H_
#define FUCHSIA_ENGINE_BROWSER_MESSAGE_PORT_IMPL_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <memory>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/system/message_pipe.h"

// Defines the implementation of a MessagePort which routes messages from
// FIDL clients to web content, or vice versa. Every MessagePortImpl has a FIDL
// port and a Mojo port.
//
// MessagePortImpl instances are self-managed; they destroy themselves when
// the connection is terminated from either the Mojo or FIDL side.
class MessagePortImpl : public fuchsia::web::MessagePort,
                        public mojo::MessageReceiver {
 public:
  // Creates a connected MessagePort from a FIDL MessagePort request and
  // returns a handle to its peer Mojo pipe.
  static mojo::ScopedMessagePipeHandle FromFidl(
      fidl::InterfaceRequest<fuchsia::web::MessagePort> port);

  // Creates a connected MessagePort from a transferred Mojo MessagePort and
  // returns a handle to its FIDL interface peer.
  static fidl::InterfaceHandle<fuchsia::web::MessagePort> FromMojo(
      mojo::ScopedMessagePipeHandle port);

 private:
  explicit MessagePortImpl(mojo::ScopedMessagePipeHandle mojo_port);

  // Non-public to ensure that only this object may destroy itself.
  ~MessagePortImpl() override;

  // fuchsia::web::MessagePort implementation.
  void PostMessage(fuchsia::web::WebMessage message,
                   PostMessageCallback callback) override;
  void ReceiveMessage(ReceiveMessageCallback callback) override;

  // Called when the connection to Blink or FIDL is terminated.
  void OnDisconnected();

  // Sends the next message enqueued in |message_queue_| to the client,
  // if the client has requested a message.
  void MaybeDeliverToClient();

  // mojo::MessageReceiver implementation.
  bool Accept(mojo::Message* message) override;

  fidl::Binding<fuchsia::web::MessagePort> binding_;
  base::circular_deque<fuchsia::web::WebMessage> message_queue_;
  ReceiveMessageCallback pending_client_read_cb_;
  std::unique_ptr<mojo::Connector> connector_;

  DISALLOW_COPY_AND_ASSIGN(MessagePortImpl);
};

#endif  // FUCHSIA_ENGINE_BROWSER_MESSAGE_PORT_IMPL_H_
