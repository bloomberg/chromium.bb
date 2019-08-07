// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/legacy_message_port_bridge.h"

#include "base/fuchsia/fuchsia_logging.h"

namespace cr_fuchsia {

// static
base::Optional<fuchsia::web::WebMessage>
LegacyMessagePortBridge::ConvertFromLegacyWebMessage(
    chromium::web::WebMessage& message) {
  fuchsia::web::WebMessage converted;
  converted.set_data(std::move(message.data));

  if (message.outgoing_transfer) {
    if (!message.outgoing_transfer->is_message_port())
      return base::nullopt;

    fuchsia::web::OutgoingTransferable outgoing;
    fuchsia::web::MessagePortPtr fuchsia_port;
    outgoing.set_message_port(fuchsia_port.NewRequest());
    new LegacyMessagePortBridge(
        std::move(message.outgoing_transfer->message_port()),
        std::move(fuchsia_port));
    std::vector<fuchsia::web::OutgoingTransferable> transferables;
    transferables.push_back(std::move(outgoing));
    converted.set_outgoing_transfer(std::move(transferables));
  }

  return converted;
}

LegacyMessagePortBridge::LegacyMessagePortBridge(
    fidl::InterfaceRequest<chromium::web::MessagePort> request,
    fuchsia::web::MessagePortPtr handle)
    : binding_(this, std::move(request)), message_port_(std::move(handle)) {
  binding_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << " |binding_| disconnected.";
    delete this;
  });
  message_port_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << " |message_port_| disconnected.";
    delete this;
  });
}

LegacyMessagePortBridge::~LegacyMessagePortBridge() = default;

void LegacyMessagePortBridge::PostMessage(chromium::web::WebMessage message,
                                          PostMessageCallback callback) {
  base::Optional<fuchsia::web::WebMessage> converted =
      ConvertFromLegacyWebMessage(message);
  if (!converted) {
    callback(false);
    return;
  }

  message_port_->PostMessage(
      std::move(*converted),
      [callback = std::move(callback)](
          fuchsia::web::MessagePort_PostMessage_Result result) {
        callback(result.is_response());
      });
}

void LegacyMessagePortBridge::ReceiveMessage(ReceiveMessageCallback callback) {
  message_port_->ReceiveMessage([callback = std::move(callback)](
                                    fuchsia::web::WebMessage message) {
    chromium::web::WebMessage converted;
    DCHECK(message.has_data());
    converted.data = std::move(*message.mutable_data());

    if (message.has_incoming_transfer()) {
      DCHECK(message.incoming_transfer().size() == 1u);
      DCHECK(message.incoming_transfer().at(0).is_message_port());
      converted.incoming_transfer =
          std::make_unique<chromium::web::IncomingTransferable>();
      chromium::web::MessagePortPtr chromium_port;
      new LegacyMessagePortBridge(
          chromium_port.NewRequest(),
          message.mutable_incoming_transfer()->at(0).message_port().Bind());
      converted.incoming_transfer->set_message_port(std::move(chromium_port));
    }

    callback(std::move(converted));
  });
}

}  // namespace cr_fuchsia
