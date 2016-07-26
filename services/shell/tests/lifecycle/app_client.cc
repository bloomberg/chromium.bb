// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/tests/lifecycle/app_client.h"

#include "services/shell/public/cpp/service_context.h"

namespace shell {
namespace test {

AppClient::AppClient() {}
AppClient::AppClient(shell::mojom::ServiceRequest request)
    : context_(new ServiceContext(this, std::move(request))) {}
AppClient::~AppClient() {}

bool AppClient::OnConnect(Connection* connection) {
  connection->AddInterface<LifecycleControl>(this);
  return true;
}

void AppClient::Create(const Identity& remote_identity,
                       LifecycleControlRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void AppClient::Ping(const PingCallback& callback) {
  callback.Run();
}

void AppClient::GracefulQuit() {
  base::MessageLoop::current()->QuitWhenIdle();
}

void AppClient::Crash() {
  // Rather than actually crash, which causes a bunch of console spray and
  // maybe UI clutter on some platforms, just exit without shutting anything
  // down properly.
  exit(1);
}

void AppClient::CloseShellConnection() {
  DCHECK(runner_);
  runner_->DestroyServiceContext();
  // Quit the app once the caller goes away.
  bindings_.set_connection_error_handler(
      base::Bind(&AppClient::BindingLost, base::Unretained(this)));
}

void AppClient::BindingLost() {
  if (bindings_.empty())
    GracefulQuit();
}

}  // namespace test
}  // namespace shell


