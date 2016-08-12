// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_LIB_CONNECTION_IMPL_H_
#define SERVICES_SHELL_PUBLIC_CPP_LIB_CONNECTION_IMPL_H_

#include <stdint.h>

#include <set>
#include <string>

#include "base/callback.h"
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
  ConnectionImpl(const Identity& remote, State initial_state);
  ~ConnectionImpl() override;

  // Sets the remote provider, transferring ownership to the ConnectionImpl.
  void SetRemoteInterfaces(
      std::unique_ptr<InterfaceProvider> remote_interfaces);

  // Sets the remote provider, without transferring ownership.
  void set_remote_interfaces(InterfaceProvider* remote_interfaces) {
    remote_interfaces_ = remote_interfaces;
  }

  shell::mojom::Connector::ConnectCallback GetConnectCallback();

 private:
  // Connection:
  const Identity& GetRemoteIdentity() const override;
  void SetConnectionLostClosure(const base::Closure& handler) override;
  shell::mojom::ConnectResult GetResult() const override;
  bool IsPending() const override;
  void AddConnectionCompletedClosure(const base::Closure& callback) override;
  InterfaceProvider* GetRemoteInterfaces() override;
  base::WeakPtr<Connection> GetWeakPtr() override;

  void OnConnectionCompleted(shell::mojom::ConnectResult result,
                             const std::string& target_user_id);

  Identity remote_;

  State state_;
  shell::mojom::ConnectResult result_ = shell::mojom::ConnectResult::SUCCEEDED;
  std::vector<base::Closure> connection_completed_callbacks_;

  InterfaceProvider* remote_interfaces_ = nullptr;

  std::unique_ptr<InterfaceProvider> remote_interfaces_owner_;

  base::WeakPtrFactory<ConnectionImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionImpl);
};

}  // namespace internal
}  // namespace shell

#endif  // SERVICES_SHELL_PUBLIC_CPP_LIB_CONNECTION_IMPL_H_
