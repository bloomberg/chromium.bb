// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/bindings_manager_web_runtime.h"

#include "base/bind.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromecast/cast_core/runtime/browser/message_port_handler.h"
#include "components/cast/message_port/blink_message_port_adapter.h"
#include "components/cast/message_port/platform_message_port.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromecast {

BindingsManagerWebRuntime::BindingsManagerWebRuntime(
    grpc::CompletionQueue* grpc_cq,
    cast::v2::CoreApplicationService::Stub* core_app_stub)
    : message_port_service_(grpc_cq, core_app_stub) {}

BindingsManagerWebRuntime::~BindingsManagerWebRuntime() = default;

void BindingsManagerWebRuntime::AddBinding(base::StringPiece binding_script) {
  int id = next_script_id_++;
  bindings_[base::NumberToString(id)] = std::string(binding_script);
}

void BindingsManagerWebRuntime::HandleMessage(
    const cast::web::Message& message,
    cast::web::MessagePortStatus* response) {
  message_port_service_.HandleMessage(message, response);
}

mojo::PendingRemote<mojom::ApiBindings>
BindingsManagerWebRuntime::CreateRemote() {
  DCHECK(!receiver_.is_bound());

  mojo::PendingRemote<mojom::ApiBindings> pending_remote =
      receiver_.BindNewPipeAndPassRemote();
  receiver_.set_disconnect_handler(
      base::BindOnce(&BindingsManagerWebRuntime::OnMojoClientDisconnected,
                     base::Unretained(this)));

  return pending_remote;
}

void BindingsManagerWebRuntime::OnMojoClientDisconnected() {
  receiver_.reset();
}

void BindingsManagerWebRuntime::AddBinding(base::StringPiece binding_name,
                                           base::StringPiece binding_script) {
  bindings_[std::string(binding_name)] = std::string(binding_script);
}

void BindingsManagerWebRuntime::GetAll(GetAllCallback callback) {
  std::vector<chromecast::mojom::ApiBindingPtr> bindings_vector;
  for (auto& bindings_name_and_script : bindings_) {
    auto api_binding =
        chromecast::mojom::ApiBinding::New(bindings_name_and_script.second);
    bindings_vector.emplace_back(std::move(api_binding));
  }
  std::move(callback).Run(std::move(bindings_vector));
}

void BindingsManagerWebRuntime::Connect(const std::string& port_name,
                                        blink::MessagePortDescriptor port) {
  message_port_service_.ConnectToPort(
      port_name,
      cast_api_bindings::BlinkMessagePortAdapter::ToClientPlatformMessagePort(
          blink::WebMessagePort::Create(std::move(port))));
}

}  // namespace chromecast
