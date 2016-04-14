// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/cpp/lib/connection_impl.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/interface_binder.h"

namespace shell {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// ConnectionImpl, public:

ConnectionImpl::ConnectionImpl(
    const std::string& connection_name,
    const Identity& remote,
    uint32_t remote_id,
    shell::mojom::InterfaceProviderPtr remote_interfaces,
    shell::mojom::InterfaceProviderRequest local_interfaces,
    const CapabilityRequest& capability_request,
    State initial_state)
    : connection_name_(connection_name),
      remote_(remote),
      remote_id_(remote_id),
      state_(initial_state),
      local_registry_(std::move(local_interfaces), this),
      remote_interfaces_(std::move(remote_interfaces)),
      capability_request_(capability_request),
      allow_all_interfaces_(capability_request.interfaces.size() == 1 &&
                            capability_request.interfaces.count("*") == 1),
      weak_factory_(this) {}

ConnectionImpl::ConnectionImpl()
    : local_registry_(shell::mojom::InterfaceProviderRequest(), this),
      allow_all_interfaces_(true),
      weak_factory_(this) {}

ConnectionImpl::~ConnectionImpl() {}

shell::mojom::Connector::ConnectCallback ConnectionImpl::GetConnectCallback() {
  return base::Bind(&ConnectionImpl::OnConnectionCompleted,
                    weak_factory_.GetWeakPtr());
}

////////////////////////////////////////////////////////////////////////////////
// ConnectionImpl, Connection implementation:

bool ConnectionImpl::HasCapabilityClass(const std::string& class_name) const {
  return capability_request_.classes.count(class_name) > 0;
}

const std::string& ConnectionImpl::GetConnectionName() {
  return connection_name_;
}

const Identity& ConnectionImpl::GetRemoteIdentity() const {
  return remote_;
}

void ConnectionImpl::SetConnectionLostClosure(const mojo::Closure& handler) {
  remote_interfaces_.set_connection_error_handler(handler);
}

shell::mojom::ConnectResult ConnectionImpl::GetResult() const {
  return result_;
}

bool ConnectionImpl::IsPending() const {
  return state_ == State::PENDING;
}

uint32_t ConnectionImpl::GetRemoteInstanceID() const {
  return remote_id_;
}

void ConnectionImpl::AddConnectionCompletedClosure(
    const mojo::Closure& callback) {
  if (IsPending())
    connection_completed_callbacks_.push_back(callback);
  else
    callback.Run();
}

bool ConnectionImpl::AllowsInterface(const std::string& interface_name) const {
  return allow_all_interfaces_ ||
         capability_request_.interfaces.count(interface_name);
}

shell::mojom::InterfaceProvider* ConnectionImpl::GetRemoteInterfaces() {
  return remote_interfaces_.get();
}

InterfaceRegistry* ConnectionImpl::GetLocalRegistry() {
  return &local_registry_;
}

base::WeakPtr<Connection> ConnectionImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

////////////////////////////////////////////////////////////////////////////////
// ConnectionImpl, private:

void ConnectionImpl::OnConnectionCompleted(shell::mojom::ConnectResult result,
                                           const std::string& target_user_id,
                                           uint32_t target_application_id) {
  DCHECK(State::PENDING == state_);

  result_ = result;
  state_ = result_ == shell::mojom::ConnectResult::SUCCEEDED ?
      State::CONNECTED : State::DISCONNECTED;
  remote_id_ = target_application_id;
  remote_.set_user_id(target_user_id);
  std::vector<mojo::Closure> callbacks;
  callbacks.swap(connection_completed_callbacks_);
  for (auto callback : callbacks)
    callback.Run();
}

}  // namespace internal
}  // namespace shell
