// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/tests/lifecycle/app_client.h"

#include "base/macros.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace service_manager {
namespace test {

AppClient::AppClient() {}

AppClient::~AppClient() {}

bool AppClient::OnConnect(const ServiceInfo& remote_info,
                          InterfaceRegistry* registry) {
  registry->AddInterface<mojom::LifecycleControl>(this);
  return true;
}

bool AppClient::OnServiceManagerConnectionLost() {
  base::MessageLoop::current()->QuitWhenIdle();
  return true;
}

void AppClient::Create(const Identity& remote_identity,
                       mojom::LifecycleControlRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void AppClient::Ping(const PingCallback& callback) {
  callback.Run();
}

void AppClient::GracefulQuit() {
  context()->RequestQuit();
}

void AppClient::Crash() {
  // Rather than actually crash, which causes a bunch of console spray and
  // maybe UI clutter on some platforms, just exit without shutting anything
  // down properly.
  exit(1);
}

void AppClient::CloseServiceManagerConnection() {
  context()->DisconnectFromServiceManager();
  bindings_.set_connection_error_handler(
      base::Bind(&AppClient::BindingLost, base::Unretained(this)));
}

void AppClient::BindingLost() {
  if (bindings_.empty())
    OnServiceManagerConnectionLost();
}

}  // namespace test
}  // namespace service_manager
