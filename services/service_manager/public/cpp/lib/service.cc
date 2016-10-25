// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service.h"

#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_info.h"

namespace service_manager {

Service::Service() {}
Service::~Service() {}

void Service::OnStart(const ServiceInfo& info) {}

bool Service::OnConnect(const ServiceInfo& remote_info,
                        InterfaceRegistry* registry) {
  return false;
}

bool Service::OnStop() { return true; }

Connector* Service::connector() {
  return context_->connector();
}

ServiceContext* Service::context() {
  return context_.get();
}

void Service::set_context(std::unique_ptr<ServiceContext> context) {
  context_ = std::move(context);
}

}  // namespace service_manager
