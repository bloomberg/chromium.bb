// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_context.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"

namespace service_manager {

ServiceContext::ServiceContext(
    std::unique_ptr<service_manager::Service> service,
    mojom::ServiceRequest request,
    std::unique_ptr<Connector> connector,
    mojom::ConnectorRequest connector_request)
    : pending_connector_request_(std::move(connector_request)),
      service_(std::move(service)),
      binding_(this, std::move(request)),
      connector_(std::move(connector)),
      weak_factory_(this) {
  DCHECK(binding_.is_bound());
  binding_.set_connection_error_handler(
      base::Bind(&ServiceContext::OnConnectionError, base::Unretained(this)));
  if (!connector_) {
    connector_ = Connector::Create(&pending_connector_request_);
  } else {
    DCHECK(pending_connector_request_.is_pending());
  }
  service_->SetContext(this);
}

ServiceContext::~ServiceContext() {}

void ServiceContext::SetQuitClosure(const base::Closure& closure) {
  quit_closure_ = closure;
}

void ServiceContext::RequestQuit() {
  DCHECK(service_control_.is_bound());
  service_control_->RequestQuit();
}

base::RepeatingClosure ServiceContext::CreateQuitClosure() {
  return base::BindRepeating(&ServiceContext::RequestQuit,
                             weak_factory_.GetWeakPtr());
}

void ServiceContext::DisconnectFromServiceManager() {
  if (binding_.is_bound())
    binding_.Close();
  connector_.reset();
}

void ServiceContext::QuitNow() {
  if (binding_.is_bound())
    binding_.Close();
  if (quit_closure_) {
    // CAUTION: May delete |this|.
    std::move(quit_closure_).Run();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ServiceContext, mojom::Service implementation:

void ServiceContext::OnStart(const Identity& identity,
                             OnStartCallback callback) {
  identity_ = identity;
  std::move(callback).Run(std::move(pending_connector_request_),
                          mojo::MakeRequest(&service_control_));
  service_->OnStart();
}

void ServiceContext::OnBindInterface(
    const BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe,
    OnBindInterfaceCallback callback) {
  // Acknowledge the request regardless of whether it's accepted.
  std::move(callback).Run();
  service_->OnBindInterface(source_info, interface_name,
                            std::move(interface_pipe));
}

////////////////////////////////////////////////////////////////////////////////
// ServiceContext, private:

void ServiceContext::OnConnectionError() {
  if (service_->OnServiceManagerConnectionLost()) {
    // CAUTION: May delete |this|.
    QuitNow();
  }
}

}  // namespace service_manager
