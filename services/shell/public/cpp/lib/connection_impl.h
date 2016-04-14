// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_LIB_CONNECTION_IMPL_H_
#define SERVICES_SHELL_PUBLIC_CPP_LIB_CONNECTION_IMPL_H_

#include <stdint.h>

#include <set>
#include <string>

#include "mojo/public/cpp/bindings/binding.h"
#include "services/shell/public/cpp/capabilities.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "services/shell/public/interfaces/interface_provider.mojom.h"

namespace shell {
namespace internal {

// A ConnectionImpl represents each half of a connection between two
// applications, allowing customization of which interfaces are published to the
// other.
class ConnectionImpl : public Connection {
 public:
  ConnectionImpl();
  // |allowed_interfaces| are the set of interfaces that the shell has allowed
  // an application to expose to another application. If this set contains only
  // the string value "*" all interfaces may be exposed.
  ConnectionImpl(const std::string& connection_name,
                 const Identity& remote,
                 uint32_t remote_id,
                 shell::mojom::InterfaceProviderPtr remote_interfaces,
                 shell::mojom::InterfaceProviderRequest local_interfaces,
                 const CapabilityRequest& capability_request,
                 State initial_state);
  ~ConnectionImpl() override;

  shell::mojom::Connector::ConnectCallback GetConnectCallback();

 private:
  // Connection:
  bool HasCapabilityClass(const std::string& class_name) const override;
  const std::string& GetConnectionName() override;
  const Identity& GetRemoteIdentity() const override;
  void SetConnectionLostClosure(const mojo::Closure& handler) override;
  shell::mojom::ConnectResult GetResult() const override;
  bool IsPending() const override;
  uint32_t GetRemoteInstanceID() const override;
  void AddConnectionCompletedClosure(const mojo::Closure& callback) override;
  bool AllowsInterface(const std::string& interface_name) const override;
  shell::mojom::InterfaceProvider* GetRemoteInterfaces() override;
  InterfaceRegistry* GetLocalRegistry() override;
  base::WeakPtr<Connection> GetWeakPtr() override;

  void OnConnectionCompleted(shell::mojom::ConnectResult result,
                             const std::string& target_user_id,
                             uint32_t target_application_id);

  const std::string connection_name_;
  Identity remote_;
  uint32_t remote_id_ = shell::mojom::kInvalidInstanceID;

  State state_;
  shell::mojom::ConnectResult result_ = shell::mojom::ConnectResult::SUCCEEDED;
  std::vector<mojo::Closure> connection_completed_callbacks_;

  InterfaceRegistry local_registry_;
  shell::mojom::InterfaceProviderPtr remote_interfaces_;

  const CapabilityRequest capability_request_;
  const bool allow_all_interfaces_;

  base::WeakPtrFactory<ConnectionImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionImpl);
};

}  // namespace internal
}  // namespace shell

#endif  // SERVICES_SHELL_PUBLIC_CPP_LIB_CONNECTION_IMPL_H_
