// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/message_port_impl.h"

#include <stdint.h>

#include <lib/fit/function.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/macros.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message_struct_traits.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom.h"

namespace {

// Converts a message posted to a JS MessagePort to a WebMessage.
// Returns an unset Optional<> if the message could not be converted.
base::Optional<fuchsia::web::WebMessage> FromMojoMessage(
    mojo::Message message) {
  fuchsia::web::WebMessage converted;

  blink::TransferableMessage transferable_message;
  if (!blink::mojom::TransferableMessage::DeserializeFromMessage(
          std::move(message), &transferable_message))
    return {};

  if (!transferable_message.ports.empty()) {
    std::vector<fuchsia::web::IncomingTransferable> transferables;
    for (const blink::MessagePortChannel& port : transferable_message.ports) {
      fuchsia::web::IncomingTransferable incoming;
      incoming.set_message_port(
          MessagePortImpl::FromMojo(port.ReleaseHandle()));
      transferables.emplace_back(std::move(incoming));
    }
    converted.set_incoming_transfer(std::move(transferables));
  }

  base::string16 data_utf16;
  if (!blink::DecodeStringMessage(transferable_message.encoded_message,
                                  &data_utf16)) {
    return {};
  }

  std::string data_utf8;
  if (!base::UTF16ToUTF8(data_utf16.data(), data_utf16.size(), &data_utf8))
    return {};
  base::STLClearObject(&data_utf16);

  fuchsia::mem::Buffer data = cr_fuchsia::MemBufferFromString(data_utf8);
  if (!data.vmo) {
    return {};
  }

  converted.set_data(std::move(data));
  return converted;
}

}  // namespace

MessagePortImpl::MessagePortImpl(mojo::ScopedMessagePipeHandle mojo_port)
    : binding_(this) {
  connector_ = std::make_unique<mojo::Connector>(
      std::move(mojo_port), mojo::Connector::SINGLE_THREADED_SEND,
      base::ThreadTaskRunnerHandle::Get());
  connector_->set_incoming_receiver(this);
  connector_->set_connection_error_handler(
      base::BindOnce(&MessagePortImpl::OnDisconnected, base::Unretained(this)));
  binding_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << " MessagePort disconnected.";
    OnDisconnected();
  });
}

MessagePortImpl::~MessagePortImpl() = default;

void MessagePortImpl::OnDisconnected() {
  // |connector_| and |binding_| are implicitly unbound.
  delete this;
}

void MessagePortImpl::PostMessage(fuchsia::web::WebMessage message,
                                  PostMessageCallback callback) {
  fuchsia::web::MessagePort_PostMessage_Result result;
  if (!message.has_data()) {
    result.set_err(fuchsia::web::FrameError::NO_DATA_IN_MESSAGE);
    callback(std::move(result));
    return;
  }

  base::string16 data_utf16;
  if (!cr_fuchsia::ReadUTF8FromVMOAsUTF16(message.data(), &data_utf16)) {
    result.set_err(fuchsia::web::FrameError::BUFFER_NOT_UTF8);
    callback(std::move(result));
    return;
  }

  blink::TransferableMessage transfer_message;
  if (message.has_outgoing_transfer()) {
    for (fuchsia::web::OutgoingTransferable& outgoing :
         *message.mutable_outgoing_transfer()) {
      transfer_message.ports.emplace_back(
          MessagePortImpl::FromFidl(std::move(outgoing.message_port())));
    }
  }

  transfer_message.owned_encoded_message =
      blink::EncodeStringMessage(data_utf16);
  transfer_message.encoded_message = transfer_message.owned_encoded_message;
  mojo::Message mojo_message =
      blink::mojom::TransferableMessage::SerializeAsMessage(&transfer_message);

  CHECK(connector_->Accept(&mojo_message));
  result.set_response(fuchsia::web::MessagePort_PostMessage_Response());
  callback(std::move(result));
}

void MessagePortImpl::ReceiveMessage(
    fuchsia::web::MessagePort::ReceiveMessageCallback callback) {
  pending_client_read_cb_ = std::move(callback);
  MaybeDeliverToClient();
}

void MessagePortImpl::MaybeDeliverToClient() {
  // Do nothing if the client hasn't requested a read, or if there's nothing
  // to read.
  if (!pending_client_read_cb_ || message_queue_.empty()) {
    return;
  }

  base::ResetAndReturn (&pending_client_read_cb_)(
      std::move(message_queue_.front()));
  message_queue_.pop_front();
}

bool MessagePortImpl::Accept(mojo::Message* message) {
  base::Optional<fuchsia::web::WebMessage> message_converted =
      FromMojoMessage(std::move(*message));
  if (!message_converted) {
    DLOG(ERROR) << "Couldn't decode MessageChannel from Mojo pipe.";
    return false;
  }
  message_queue_.emplace_back(std::move(message_converted.value()));
  MaybeDeliverToClient();
  return true;
}

// static
mojo::ScopedMessagePipeHandle MessagePortImpl::FromFidl(
    fidl::InterfaceRequest<fuchsia::web::MessagePort> port) {
  mojo::ScopedMessagePipeHandle client_port;
  mojo::ScopedMessagePipeHandle content_port;
  mojo::CreateMessagePipe(0, &content_port, &client_port);

  MessagePortImpl* port_impl = new MessagePortImpl(std::move(client_port));
  port_impl->binding_.Bind(std::move(port));

  return content_port;
}

// static
fidl::InterfaceHandle<fuchsia::web::MessagePort> MessagePortImpl::FromMojo(
    mojo::ScopedMessagePipeHandle port) {
  MessagePortImpl* created_port = new MessagePortImpl(std::move(port));
  return created_port->binding_.NewBinding();
}
