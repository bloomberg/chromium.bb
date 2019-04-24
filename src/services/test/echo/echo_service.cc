// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/test/echo/echo_service.h"
#include "base/bind.h"

namespace echo {

EchoService::EchoService(service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)) {
  registry_.AddInterface<mojom::Echo>(base::BindRepeating(
      &EchoService::BindEchoRequest, base::Unretained(this)));
}

EchoService::~EchoService() = default;

void EchoService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void EchoService::BindEchoRequest(mojom::EchoRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void EchoService::EchoString(const std::string& input,
                             EchoStringCallback callback) {
  std::move(callback).Run(input);
}

void EchoService::Quit() {
  service_binding_.RequestClose();
}

}  // namespace echo
