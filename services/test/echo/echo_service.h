// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ECHO_ECHO_SERVICE_H_
#define SERVICES_ECHO_ECHO_SERVICE_H_

#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/test/echo/public/mojom/echo.mojom.h"

namespace echo {

class EchoService : public service_manager::Service, public mojom::Echo {
 public:
  explicit EchoService(service_manager::mojom::ServiceRequest request);
  ~EchoService() override;

 private:
  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // mojom::Echo:
  void EchoString(const std::string& input,
                  EchoStringCallback callback) override;
  void Quit() override;

  void BindEchoRequest(mojom::EchoRequest request);

  service_manager::ServiceBinding service_binding_;
  service_manager::BinderRegistry registry_;
  mojo::BindingSet<mojom::Echo> bindings_;

  DISALLOW_COPY_AND_ASSIGN(EchoService);
};

}  // namespace echo

#endif  // SERVICES_ECHO_ECHO_SERVICE_H_
