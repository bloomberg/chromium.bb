// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/lib/connector_impl.h"

#include "base/memory/ptr_util.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/lib/connection_impl.h"

namespace service_manager {

namespace {
void EmptyBindCallback(mojom::ConnectResult, const std::string&) {}
}

ConnectorImpl::ConnectorImpl(mojom::ConnectorPtrInfo unbound_state)
    : unbound_state_(std::move(unbound_state)) {
  thread_checker_.DetachFromThread();
}

ConnectorImpl::ConnectorImpl(mojom::ConnectorPtr connector)
    : connector_(std::move(connector)) {
  connector_.set_connection_error_handler(
      base::Bind(&ConnectorImpl::OnConnectionError, base::Unretained(this)));
}

ConnectorImpl::~ConnectorImpl() {}

void ConnectorImpl::OnConnectionError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  connector_.reset();
}

void ConnectorImpl::StartService(
    const Identity& identity,
    mojom::ServicePtr service,
    mojom::PIDReceiverRequest pid_receiver_request) {
  if (!BindConnectorIfNecessary())
    return;

  DCHECK(service.is_bound() && pid_receiver_request.is_pending());
  connector_->StartService(identity,
                           service.PassInterface().PassHandle(),
                           std::move(pid_receiver_request));
}

std::unique_ptr<Connection> ConnectorImpl::Connect(const std::string& name) {
  return Connect(Identity(name, mojom::kInheritUserID));
}

std::unique_ptr<Connection> ConnectorImpl::Connect(const Identity& target) {
  if (!BindConnectorIfNecessary())
    return nullptr;

  DCHECK(thread_checker_.CalledOnValidThread());

  mojom::InterfaceProviderPtr remote_interfaces;
  mojom::InterfaceProviderRequest remote_request(&remote_interfaces);
  std::unique_ptr<internal::ConnectionImpl> connection(
      new internal::ConnectionImpl(target, Connection::State::PENDING));
  std::unique_ptr<InterfaceProvider> remote_interface_provider(
      new InterfaceProvider);
  remote_interface_provider->Bind(std::move(remote_interfaces));
  connection->SetRemoteInterfaces(std::move(remote_interface_provider));

  connector_->Connect(target, std::move(remote_request),
                      connection->GetConnectCallback());
  return std::move(connection);
}

void ConnectorImpl::BindInterface(
    const Identity& target,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (!BindConnectorIfNecessary())
    return;

  connector_->BindInterface(target, interface_name, std::move(interface_pipe),
                            base::Bind(&EmptyBindCallback));
}

std::unique_ptr<Connector> ConnectorImpl::Clone() {
  if (!BindConnectorIfNecessary())
    return nullptr;

  mojom::ConnectorPtr connector;
  mojom::ConnectorRequest request(&connector);
  connector_->Clone(std::move(request));
  return base::MakeUnique<ConnectorImpl>(connector.PassInterface());
}

void ConnectorImpl::BindConnectorRequest(mojom::ConnectorRequest request) {
  if (!BindConnectorIfNecessary())
    return;
  connector_->Clone(std::move(request));
}

bool ConnectorImpl::BindConnectorIfNecessary() {
  // Bind this object to the current thread the first time it is used to
  // connect.
  if (!connector_.is_bound()) {
    if (!unbound_state_.is_valid()) {
      // It's possible to get here when the link to the service manager has been
      // severed
      // (and so the connector pipe has been closed) but the app has chosen not
      // to quit.
      return false;
    }

    // Bind the ThreadChecker to this thread.
    DCHECK(thread_checker_.CalledOnValidThread());

    connector_.Bind(std::move(unbound_state_));
    connector_.set_connection_error_handler(
        base::Bind(&ConnectorImpl::OnConnectionError, base::Unretained(this)));
  }

  return true;
}

std::unique_ptr<Connector> Connector::Create(mojom::ConnectorRequest* request) {
  mojom::ConnectorPtr proxy;
  *request = mojo::MakeRequest(&proxy);
  return base::MakeUnique<ConnectorImpl>(proxy.PassInterface());
}

}  // namespace service_manager
