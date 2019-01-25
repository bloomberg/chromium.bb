// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"
#include "services/service_manager/tests/service_manager/test_manifests.h"

namespace {

class PackagedService : public service_manager::Service {
 public:
  explicit PackagedService(service_manager::mojom::ServiceRequest request)
      : service_binding_(this, std::move(request)) {}
  ~PackagedService() override = default;

 private:
  service_manager::ServiceBinding service_binding_;

  DISALLOW_COPY_AND_ASSIGN(PackagedService);
};

class Embedder : public service_manager::Service,
                 public service_manager::mojom::ServiceFactory {
 public:
  explicit Embedder(service_manager::mojom::ServiceRequest request)
      : service_binding_(this, std::move(request)) {
    registry_.AddInterface<service_manager::mojom::ServiceFactory>(
        base::Bind(&Embedder::Create, base::Unretained(this)));
  }

  ~Embedder() override = default;

 private:
  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  void Create(service_manager::mojom::ServiceFactoryRequest request) {
    service_factory_bindings_.AddBinding(this, std::move(request));
  }

  // mojom::ServiceFactory:
  void CreateService(
      service_manager::mojom::ServiceRequest request,
      const std::string& name,
      service_manager::mojom::PIDReceiverPtr pid_receiver) override {
    if (name == service_manager::kTestRegularServiceName ||
        name == service_manager::kTestSharedServiceName ||
        name == service_manager::kTestSingletonServiceName) {
      packaged_service_ = std::make_unique<PackagedService>(std::move(request));
    } else {
      LOG(ERROR) << "Failed to create unknown service " << name;
    }
  }

  service_manager::ServiceBinding service_binding_;
  service_manager::BinderRegistry registry_;
  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;
  std::unique_ptr<service_manager::Service> packaged_service_;

  DISALLOW_COPY_AND_ASSIGN(Embedder);
};

}  // namespace

void ServiceMain(service_manager::mojom::ServiceRequest request) {
  base::MessageLoop message_loop;
  Embedder(std::move(request)).RunUntilTermination();
}
