// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/lib/connector_impl.h"

#include "base/memory/ptr_util.h"
#include "services/service_manager/public/cpp/identity.h"

namespace service_manager {

ConnectorImpl::ConnectorImpl(mojom::ConnectorPtrInfo unbound_state)
    : unbound_state_(std::move(unbound_state)), weak_factory_(this) {
  thread_checker_.DetachFromThread();
}

ConnectorImpl::ConnectorImpl(mojom::ConnectorPtr connector)
    : connector_(std::move(connector)), weak_factory_(this) {
  connector_.set_connection_error_handler(
      base::Bind(&ConnectorImpl::OnConnectionError, base::Unretained(this)));
}

ConnectorImpl::~ConnectorImpl() {}

void ConnectorImpl::OnConnectionError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  connector_.reset();
}

void ConnectorImpl::StartService(const Identity& identity) {
  if (BindConnectorIfNecessary())
    connector_->StartService(identity,
                             base::Bind(&ConnectorImpl::StartServiceCallback,
                                        weak_factory_.GetWeakPtr()));
}

void ConnectorImpl::StartService(const std::string& name) {
  StartService(Identity(name, mojom::kInheritUserID));
}

void ConnectorImpl::StartService(
    const Identity& identity,
    mojom::ServicePtr service,
    mojom::PIDReceiverRequest pid_receiver_request) {
  if (!BindConnectorIfNecessary())
    return;

  DCHECK(service.is_bound() && pid_receiver_request.is_pending());
  connector_->StartServiceWithProcess(
      identity, service.PassInterface().PassHandle(),
      std::move(pid_receiver_request),
      base::Bind(&ConnectorImpl::StartServiceCallback,
                 weak_factory_.GetWeakPtr()));
}

void ConnectorImpl::BindInterface(
    const Identity& target,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (!BindConnectorIfNecessary())
    return;

  auto service_overrides_iter = local_binder_overrides_.find(target.name());
  if (service_overrides_iter != local_binder_overrides_.end()) {
    auto override_iter = service_overrides_iter->second.find(interface_name);
    if (override_iter != service_overrides_iter->second.end()) {
      override_iter->second.Run(std::move(interface_pipe));
      return;
    }
  }

  connector_->BindInterface(target, interface_name, std::move(interface_pipe),
                            base::Bind(&ConnectorImpl::StartServiceCallback,
                                       weak_factory_.GetWeakPtr()));
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

base::WeakPtr<Connector> ConnectorImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ConnectorImpl::OverrideBinderForTesting(const std::string& service_name,
                                             const std::string& interface_name,
                                             const TestApi::Binder& binder) {
  local_binder_overrides_[service_name][interface_name] = binder;
}

void ConnectorImpl::ClearBinderOverrides() {
  local_binder_overrides_.clear();
}

void ConnectorImpl::SetStartServiceCallback(
    const Connector::StartServiceCallback& callback) {
  start_service_callback_ = callback;
}

void ConnectorImpl::ResetStartServiceCallback() {
  start_service_callback_.Reset();
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

void ConnectorImpl::StartServiceCallback(mojom::ConnectResult result,
                                         const Identity& user_id) {
  if (!start_service_callback_.is_null())
    start_service_callback_.Run(result, user_id);
}

std::unique_ptr<Connector> Connector::Create(mojom::ConnectorRequest* request) {
  mojom::ConnectorPtr proxy;
  *request = mojo::MakeRequest(&proxy);
  return base::MakeUnique<ConnectorImpl>(proxy.PassInterface());
}

}  // namespace service_manager
