// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/named_message_port_connector.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/runners/cast/cast_platform_bindings_ids.h"

namespace {

const char kBindingsJsPath[] = FILE_PATH_LITERAL(
    "chromecast/bindings/resources/named_message_port_connector.js");
const char kControlPortConnectMessage[] = "cast.master.connect";

}  // namespace

NamedMessagePortConnector::NamedMessagePortConnector(fuchsia::web::Frame* frame)
    : frame_(frame) {
  DCHECK(frame_);

  // Inject the script providing the connection API into the Frame.
  base::FilePath assets_path;
  bool path_service_result =
      base::PathService::Get(base::DIR_ASSETS, &assets_path);
  DCHECK(path_service_result);
  fuchsia::mem::Buffer bindings_script_ = cr_fuchsia::MemBufferFromFile(
      base::File(assets_path.AppendASCII(kBindingsJsPath),
                 base::File::FLAG_OPEN | base::File::FLAG_READ));

  std::vector<std::string> origins = {"*"};
  frame_->AddBeforeLoadJavaScript(
      static_cast<uint64_t>(
          CastPlatformBindingsId::NAMED_MESSAGE_PORT_CONNECTOR),
      std::move(origins),
      cr_fuchsia::CloneBuffer(bindings_script_, "cast-bindings-js"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        CHECK(result.is_response()) << "Couldn't inject bindings.";
      });
}

NamedMessagePortConnector::~NamedMessagePortConnector() {
  frame_->RemoveBeforeLoadJavaScript(static_cast<uint64_t>(
      CastPlatformBindingsId::NAMED_MESSAGE_PORT_CONNECTOR));

  // All handlers must be unregistered prior to deletion.
  DCHECK(port_connected_handlers_.empty());
}

void NamedMessagePortConnector::RegisterDefaultHandler(
    DefaultPortConnectedCallback handler) {
  default_handler_ = std::move(handler);
}

void NamedMessagePortConnector::Register(const std::string& port_name,
                                         PortConnectedCallback handler) {
  DCHECK(handler);
  DCHECK(!port_name.empty());
  DCHECK(port_connected_handlers_.find(port_name) ==
         port_connected_handlers_.end());

  port_connected_handlers_[port_name] = handler;
}

void NamedMessagePortConnector::Unregister(const std::string& port_name) {
  size_t removed = port_connected_handlers_.erase(port_name);
  DCHECK_EQ(removed, 1U);
}

void NamedMessagePortConnector::OnPageLoad() {
  control_port_.Unbind();

  fuchsia::web::WebMessage message;
  message.set_data(cr_fuchsia::MemBufferFromString(kControlPortConnectMessage,
                                                   "cast-connect-message"));
  std::vector<fuchsia::web::OutgoingTransferable> outgoing_vector(1);
  outgoing_vector[0].set_message_port(control_port_.NewRequest());
  message.set_outgoing_transfer(std::move(outgoing_vector));

  frame_->PostMessage("*", std::move(message),
                      [](fuchsia::web::Frame_PostMessage_Result result) {
                        DCHECK(result.is_response());
                      });

  ReceiveNextConnectRequest();
}

void NamedMessagePortConnector::ReceiveNextConnectRequest() {
  DCHECK(control_port_);

  control_port_->ReceiveMessage(
      fit::bind_member(this, &NamedMessagePortConnector::OnConnectRequest));
}

void NamedMessagePortConnector::OnConnectRequest(
    fuchsia::web::WebMessage message) {
  std::string port_name;
  if (!message.has_data() ||
      !cr_fuchsia::StringFromMemBuffer(message.data(), &port_name)) {
    LOG(ERROR) << "Couldn't read from message VMO.";
    control_port_.Unbind();
    return;
  }

  if (message.incoming_transfer().size() != 1) {
    LOG(ERROR) << "Expected one Transferable, got "
               << message.incoming_transfer().size() << " instead.";
    control_port_.Unbind();
    return;
  }

  fuchsia::web::IncomingTransferable& transferable =
      (*message.mutable_incoming_transfer())[0];
  if (!transferable.is_message_port()) {
    LOG(ERROR) << "Transferable is not a MessagePort.";
    control_port_.Unbind();
    return;
  }

  if (default_handler_ && port_connected_handlers_.find(port_name) ==
                              port_connected_handlers_.end()) {
    default_handler_.Run(port_name, std::move(transferable.message_port()));
  } else {
    // TODO(crbug.com/953958): Deprecated, remove this once all APIs are
    // migrated.

    if (port_connected_handlers_.find(port_name) ==
        port_connected_handlers_.end()) {
      LOG(ERROR) << "No registration for port: " << port_name;
      control_port_.Unbind();
      return;
    }

    port_connected_handlers_[port_name].Run(
        std::move(transferable.message_port()));
  }
  ReceiveNextConnectRequest();
}
