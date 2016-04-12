// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_LIB_CONNECTOR_IMPL_H_
#define SERVICES_SHELL_PUBLIC_CPP_LIB_CONNECTOR_IMPL_H_

#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/interfaces/connector.mojom.h"

namespace mojo {

class ConnectorImpl : public Connector {
 public:
  explicit ConnectorImpl(shell::mojom::ConnectorPtrInfo unbound_state);
  explicit ConnectorImpl(shell::mojom::ConnectorPtr connector);
  ~ConnectorImpl() override;

 private:
  // Connector:
  scoped_ptr<Connection> Connect(const std::string& name) override;
  scoped_ptr<Connection> Connect(ConnectParams* params) override;
  scoped_ptr<Connector> Clone() override;

  shell::mojom::ConnectorPtrInfo unbound_state_;
  shell::mojom::ConnectorPtr connector_;

  scoped_ptr<base::ThreadChecker> thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ConnectorImpl);
};

}  // namespace mojo

#endif  // SERVICES_SHELL_PUBLIC_CPP_LIB_CONNECTOR_IMPL_H_
