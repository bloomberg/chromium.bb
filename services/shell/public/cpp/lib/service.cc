// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/cpp/service.h"

#include "services/shell/public/cpp/service_context.h"

namespace shell {

Service::Service() {}
Service::~Service() {}

void Service::OnStart(const Identity& identity) {}

bool Service::OnConnect(Connection* connection) {
  return false;
}

bool Service::OnStop() { return true; }

InterfaceProvider* Service::GetInterfaceProviderForConnection() {
  return nullptr;
}

InterfaceRegistry* Service::GetInterfaceRegistryForConnection() {
  return nullptr;
}

Connector* Service::connector() {
  return context_->connector();
}

ServiceContext* Service::context() {
  return context_.get();
}

void Service::set_context(std::unique_ptr<ServiceContext> context) {
  context_ = std::move(context);
}

}  // namespace shell
