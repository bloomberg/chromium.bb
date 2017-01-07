// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_LIB_CONNECTOR_IMPL_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_LIB_CONNECTOR_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"

namespace service_manager {

class ConnectorImpl : public Connector {
 public:
  explicit ConnectorImpl(mojom::ConnectorPtrInfo unbound_state);
  explicit ConnectorImpl(mojom::ConnectorPtr connector);
  ~ConnectorImpl() override;

 private:
  void OnConnectionError();

  // Connector:
  void StartService(const Identity& identity,
                    mojom::ServicePtr service,
                    mojom::PIDReceiverRequest pid_receiver_request) override;
  std::unique_ptr<Connection> Connect(const std::string& name) override;
  std::unique_ptr<Connection> Connect(const Identity& target) override;
  void BindInterface(const Identity& target,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe) override;
  std::unique_ptr<Connector> Clone() override;
  void BindConnectorRequest(mojom::ConnectorRequest request) override;

  bool BindConnectorIfNecessary();

  mojom::ConnectorPtrInfo unbound_state_;
  mojom::ConnectorPtr connector_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ConnectorImpl);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_LIB_CONNECTOR_IMPL_H_
