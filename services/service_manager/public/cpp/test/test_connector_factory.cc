// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/test/test_connector_factory.h"

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"

namespace service_manager {

namespace {

class TestConnectorImpl : public mojom::Connector {
 public:
  TestConnectorImpl(TestConnectorFactory* factory,
                    std::unique_ptr<Service> service)
      : service_context_(std::move(service), mojo::MakeRequest(&service_ptr_)),
        factory_(factory) {
    service_ptr_->OnStart(factory_->source_identity(),
                          base::BindOnce(&TestConnectorImpl::OnStartCallback,
                                         base::Unretained(this)));
  }

  ~TestConnectorImpl() override = default;

  void BindInterface(const Identity& target,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe,
                     BindInterfaceCallback callback) override {
    service_ptr_->OnBindInterface(
        BindSourceInfo(factory_->source_identity(), CapabilitySet()),
        interface_name, std::move(interface_pipe),
        base::Bind(&base::DoNothing));
    std::move(callback).Run(mojom::ConnectResult::SUCCEEDED, Identity());
  }

  void StartService(const Identity& target,
                    StartServiceCallback callback) override {
    NOTREACHED();
  }

  void QueryService(const Identity& target,
                    QueryServiceCallback callback) override {
    NOTREACHED();
  }

  void StartServiceWithProcess(
      const Identity& identity,
      mojo::ScopedMessagePipeHandle service,
      mojom::PIDReceiverRequest pid_receiver_request,
      StartServiceWithProcessCallback callback) override {
    NOTREACHED();
  }

  void Clone(mojom::ConnectorRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  void FilterInterfaces(const std::string& spec,
                        const Identity& source,
                        mojom::InterfaceProviderRequest source_request,
                        mojom::InterfaceProviderPtr target) override {
    NOTREACHED();
  }

 private:
  void OnStartCallback(
      service_manager::mojom::ConnectorRequest request,
      service_manager::mojom::ServiceControlAssociatedRequest control_request) {
  }

  mojom::ServicePtr service_ptr_;
  service_manager::ServiceContext service_context_;
  TestConnectorFactory* const factory_;
  mojo::BindingSet<mojom::Connector> bindings_;

  DISALLOW_COPY_AND_ASSIGN(TestConnectorImpl);
};

}  // namespace

TestConnectorFactory::TestConnectorFactory(std::unique_ptr<Service> service)
    : source_identity_("TestConnectorFactory"),
      impl_(std::make_unique<TestConnectorImpl>(this, std::move(service))) {}

TestConnectorFactory::~TestConnectorFactory() = default;

std::unique_ptr<Connector> TestConnectorFactory::CreateConnector() {
  mojom::ConnectorPtr proxy;
  impl_->Clone(mojo::MakeRequest(&proxy));
  return base::MakeUnique<Connector>(std::move(proxy));
}

}  // namespace service_manager
