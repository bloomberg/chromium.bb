// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service.h"

#include "base/logging.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace service_manager {

Service::Service() = default;

Service::~Service() = default;

// static
void Service::RunUntilTermination(std::unique_ptr<Service> service) {
  auto* raw_service = service.get();
  raw_service->set_termination_closure(base::BindOnce(
      [](std::unique_ptr<Service> service) {}, std::move(service)));
}

void Service::OnStart() {}

void Service::OnBindInterface(const BindSourceInfo& source,
                              const std::string& interface_name,
                              mojo::ScopedMessagePipeHandle interface_pipe) {}

void Service::OnDisconnected() {
  Terminate();
}

bool Service::OnServiceManagerConnectionLost() {
  return true;
}

void Service::Terminate() {
  if (termination_closure_)
    std::move(termination_closure_).Run();
}

ServiceContext* Service::context() const {
  DCHECK(service_context_)
      << "Service::context() may only be called after the Service constructor.";
  return service_context_;
}

void Service::SetContext(ServiceContext* context) {
  service_context_ = context;
}

}  // namespace service_manager
