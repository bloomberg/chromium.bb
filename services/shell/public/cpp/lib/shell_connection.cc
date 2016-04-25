// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/cpp/shell_connection.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/shell/public/cpp/capabilities.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/lib/connection_impl.h"
#include "services/shell/public/cpp/lib/connector_impl.h"
#include "services/shell/public/cpp/shell_client.h"

namespace shell {

////////////////////////////////////////////////////////////////////////////////
// ShellConnection, public:

ShellConnection::ShellConnection(shell::ShellClient* client,
                                 mojom::ShellClientRequest request)
    : client_(client), binding_(this) {
  mojom::ConnectorPtr connector;
  pending_connector_request_ = GetProxy(&connector);
  connector_.reset(new ConnectorImpl(std::move(connector)));

  DCHECK(request.is_pending());
  binding_.Bind(std::move(request));
}

ShellConnection::~ShellConnection() {}

void ShellConnection::set_initialize_handler(const base::Closure& callback) {
  initialize_handler_ = callback;
}

void ShellConnection::SetAppTestConnectorForTesting(
    mojom::ConnectorPtr connector) {
  pending_connector_request_ = nullptr;
  connector_.reset(new ConnectorImpl(std::move(connector)));
}

////////////////////////////////////////////////////////////////////////////////
// ShellConnection, mojom::ShellClient implementation:

void ShellConnection::Initialize(mojom::IdentityPtr identity,
                                 uint32_t id,
                                 const InitializeCallback& callback) {
  identity_ = identity.To<Identity>();
  if (!initialize_handler_.is_null())
    initialize_handler_.Run();

  callback.Run(std::move(pending_connector_request_));

  DCHECK(binding_.is_bound());
  binding_.set_connection_error_handler([this] { OnConnectionError(); });

  client_->Initialize(connector_.get(), identity_, id);
}

void ShellConnection::AcceptConnection(
    mojom::IdentityPtr source,
    uint32_t source_id,
    mojom::InterfaceProviderRequest local_interfaces,
    mojom::InterfaceProviderPtr remote_interfaces,
    mojom::CapabilityRequestPtr allowed_capabilities,
    const mojo::String& name) {
  std::unique_ptr<Connection> registry(new internal::ConnectionImpl(
      name, source.To<Identity>(), source_id, std::move(remote_interfaces),
      std::move(local_interfaces), allowed_capabilities.To<CapabilityRequest>(),
      Connection::State::CONNECTED));
  if (!client_->AcceptConnection(registry.get()))
    return;

  // TODO(beng): it appears we never prune this list. We should, when the
  //             connection's remote service provider pipe breaks.
  incoming_connections_.push_back(std::move(registry));
}

////////////////////////////////////////////////////////////////////////////////
// ShellConnection, private:

void ShellConnection::OnConnectionError() {
  // Note that the ShellClient doesn't technically have to quit now, it may live
  // on to service existing connections. All existing Connectors however are
  // invalid.
  if (client_->ShellConnectionLost() && !connection_lost_closure_.is_null())
    connection_lost_closure_.Run();
  // We don't reset the connector as clients may have taken a raw pointer to it.
  // Connect() will return nullptr if they try to connect to anything.
}

}  // namespace shell
