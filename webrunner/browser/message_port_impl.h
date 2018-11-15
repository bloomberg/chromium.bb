// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_BROWSER_MESSAGE_PORT_IMPL_H_
#define WEBRUNNER_BROWSER_MESSAGE_PORT_IMPL_H_

#include <lib/fidl/cpp/binding.h>
#include <deque>
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "webrunner/fidl/chromium/web/cpp/fidl.h"

namespace webrunner {

// Defines the implementation of a MessagePort which routes messages from
// FIDL clients to web content, or vice versa. Every MessagePortImpl has a FIDL
// port and a Mojo port.
//
// MessagePortImpl instances are self-managed; they destroy themselves when
// the connection is terminated from either the Mojo or FIDL side.
class MessagePortImpl : public chromium::web::MessagePort,
                        public mojo::MessageReceiver {
 public:
  // Creates a connected MessagePort from a FIDL MessagePort request and
  // returns a handle to its peer Mojo pipe.
  static mojo::ScopedMessagePipeHandle FromFidl(
      fidl::InterfaceRequest<chromium::web::MessagePort> client_fidl_port);

  // Creates a connected MessagePort from a transferred Mojo MessagePort and
  // returns a handle to its FIDL interface peer.
  static fidl::InterfaceHandle<chromium::web::MessagePort> FromMojo(
      mojo::ScopedMessagePipeHandle port);

 protected:
  friend class base::DeleteHelper<MessagePortImpl>;

  explicit MessagePortImpl(mojo::ScopedMessagePipeHandle mojo_port);

  // Non-public to ensure that only this object may destroy itself.
  ~MessagePortImpl() override;

  fidl::Binding<chromium::web::MessagePort> binding_;

 private:
  // chromium::web::MessagePortImpl implementation.
  void PostMessage(chromium::web::WebMessage message,
                   PostMessageCallback callback) override;
  void ReceiveMessage(ReceiveMessageCallback callback) override;

  // Called when the connection to Blink or FIDL is terminated.
  void OnDisconnected();

  // Sends the next message enqueued in |message_queue_| to the client,
  // if the client has requested a message.
  void MaybeDeliverToClient();

  // mojo::MessageReceiver implementation.
  bool Accept(mojo::Message* message) override;

  std::deque<chromium::web::WebMessage> message_queue_;
  ReceiveMessageCallback pending_client_read_cb_;
  std::unique_ptr<mojo::Connector> connector_;

  DISALLOW_COPY_AND_ASSIGN(MessagePortImpl);
};

}  // namespace webrunner

#endif  // WEBRUNNER_BROWSER_MESSAGE_PORT_IMPL_H_
