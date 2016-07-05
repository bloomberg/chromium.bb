// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/cpp/service.h"

namespace shell {

Service::Service() {}
Service::~Service() {}

void Service::OnStart(Connector* connector, const Identity& identity,
                      uint32_t id) {
}

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

}  // namespace shell
