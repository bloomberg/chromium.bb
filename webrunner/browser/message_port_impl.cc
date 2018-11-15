// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/browser/message_port_impl.h"

#include <stdint.h>

#include <lib/fit/function.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/macros.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message_struct_traits.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom.h"
#include "webrunner/browser/vmo_util.h"
#include "webrunner/fidl/chromium/web/cpp/fidl.h"

namespace webrunner {
namespace {

// Converts a message posted to a JS MessagePort to a WebMessage.
// Returns an unset Optional<> if the message could not be converted.
base::Optional<chromium::web::WebMessage> FromMojoMessage(
    mojo::Message message) {
  chromium::web::WebMessage converted;

  blink::TransferableMessage transferable_message;
  if (!blink::mojom::TransferableMessage::DeserializeFromMessage(
          std::move(message), &transferable_message))
    return {};

  if (!transferable_message.ports.empty()) {
    if (transferable_message.ports.size() != 1u) {
      // TODO(crbug.com/893236): support >1 transferable when fidlc cycle
      // detection is fixed (FIDL-354).
      LOG(ERROR) << "FIXME: Request to transfer >1 MessagePort was ignored.";
      return {};
    }
    converted.incoming_transfer =
        std::make_unique<chromium::web::IncomingTransferable>();
    converted.incoming_transfer->set_message_port(MessagePortImpl::FromMojo(
        transferable_message.ports[0].ReleaseHandle()));
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

  converted.data.size = data_utf8.size();
  zx_status_t status = zx::vmo::create(data_utf8.size(), ZX_VMO_NON_RESIZABLE,
                                       &converted.data.vmo);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx_vmo_create";
    return {};
  }

  status = converted.data.vmo.write(data_utf8.data(), 0, data_utf8.length());
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx_vmo_write";
    return {};
  }

  return converted;
}

// Converts a FIDL chromium::web::WebMessage into a MessagePipe message, for
// sending over Mojo.
//
// Returns a null mojo::Message if |message| was invalid.
mojo::Message FromFidlMessage(chromium::web::WebMessage message) {
  base::string16 data_utf16;
  if (!ReadUTF8FromVMOAsUTF16(message.data, &data_utf16))
    return mojo::Message();

  // TODO(crbug.com/893236): support >1 transferable when fidlc cycle detection
  // is fixed (FIDL-354).
  blink::TransferableMessage transfer_message;
  if (message.outgoing_transfer) {
    transfer_message.ports.emplace_back(MessagePortImpl::FromFidl(
        std::move(message.outgoing_transfer->message_port())));
  }

  transfer_message.owned_encoded_message =
      blink::EncodeStringMessage(data_utf16);
  transfer_message.encoded_message = transfer_message.owned_encoded_message;
  return blink::mojom::TransferableMessage::SerializeAsMessage(
      &transfer_message);
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
    if (status != ZX_OK)
      ZX_DLOG(INFO, status) << "Disconnected";

    OnDisconnected();
  });
}

MessagePortImpl::~MessagePortImpl() {}

void MessagePortImpl::OnDisconnected() {
  // |connector_| and |binding_| are implicitly unbound.
  delete this;
}

void MessagePortImpl::PostMessage(chromium::web::WebMessage message,
                                  PostMessageCallback callback) {
  mojo::Message mojo_message = FromFidlMessage(std::move(message));
  if (mojo_message.IsNull()) {
    callback(false);
    delete this;
    return;
  }
  CHECK(connector_->Accept(&mojo_message));
}

void MessagePortImpl::ReceiveMessage(ReceiveMessageCallback callback) {
  pending_client_read_cb_ = std::move(callback);
  MaybeDeliverToClient();
}

void MessagePortImpl::MaybeDeliverToClient() {
  // Do nothing if the client hasn't requested a read, or if there's nothing to
  // read.
  if (!pending_client_read_cb_ || message_queue_.empty())
    return;

  std::move(pending_client_read_cb_)(std::move(message_queue_.front()));
  message_queue_.pop_front();
}

bool MessagePortImpl::Accept(mojo::Message* message) {
  base::Optional<chromium::web::WebMessage> message_converted =
      FromMojoMessage(std::move(*message));
  if (!message_converted) {
    DLOG(ERROR) << "Couldn't decode MessageChannel from Mojo pipe.";
    return false;
  }

  message_queue_.push_back(std::move(message_converted.value()));
  MaybeDeliverToClient();
  return true;
}

// static
mojo::ScopedMessagePipeHandle MessagePortImpl::FromFidl(
    fidl::InterfaceRequest<chromium::web::MessagePort> port) {
  mojo::ScopedMessagePipeHandle client_port;
  mojo::ScopedMessagePipeHandle content_port;
  mojo::CreateMessagePipe(0, &content_port, &client_port);

  MessagePortImpl* port_impl = new MessagePortImpl(std::move(client_port));
  port_impl->binding_.Bind(std::move(port));

  return content_port;
}

// static
fidl::InterfaceHandle<chromium::web::MessagePort> MessagePortImpl::FromMojo(
    mojo::ScopedMessagePipeHandle port) {
  MessagePortImpl* created_port = new MessagePortImpl(std::move(port));
  return created_port->binding_.NewBinding();
}

}  // namespace webrunner
