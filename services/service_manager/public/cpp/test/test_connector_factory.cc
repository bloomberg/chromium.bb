// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/test/test_connector_factory.h"

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"

namespace service_manager {

namespace {

class TestConnectorImpl : public mojom::Connector {
 public:
  TestConnectorImpl(TestConnectorFactory* factory, Service* service)
      : factory_(factory), service_(service) {}

  ~TestConnectorImpl() override = default;

  void BindInterface(const Identity& target,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe,
                     BindInterfaceCallback callback) override {
    service_->OnBindInterface(
        BindSourceInfo(factory_->source_identity(), CapabilitySet()),
        interface_name, std::move(interface_pipe));
    std::move(callback).Run(mojom::ConnectResult::SUCCEEDED, Identity());
  }

  void StartService(const Identity& target,
                    StartServiceCallback callback) override {
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
  TestConnectorFactory* const factory_;
  Service* const service_;
  mojo::BindingSet<mojom::Connector> bindings_;

  DISALLOW_COPY_AND_ASSIGN(TestConnectorImpl);
};

}  // namespace

TestConnectorFactory::TestConnectorFactory(Service* service)
    : source_identity_("TestConnectorFactory"),
      impl_(base::MakeUnique<TestConnectorImpl>(this, service)) {}

TestConnectorFactory::~TestConnectorFactory() = default;

std::unique_ptr<Connector> TestConnectorFactory::CreateConnector() {
  mojom::ConnectorPtr proxy;
  impl_->Clone(mojo::MakeRequest(&proxy));
  return base::MakeUnique<Connector>(std::move(proxy));
}

}  // namespace service_manager
