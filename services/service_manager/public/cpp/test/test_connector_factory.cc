// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/test/test_connector_factory.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace service_manager {

namespace {

class ProxiedServiceConnector : public mojom::Connector {
 public:
  ProxiedServiceConnector(
      TestConnectorFactory* factory,
      TestConnectorFactory::NameToServiceProxyMap* proxies,
      const base::Token& test_instance_group)
      : fake_guid_(base::Token::CreateRandom()),
        factory_(factory),
        proxies_(proxies),
        test_instance_group_(test_instance_group) {}

  ~ProxiedServiceConnector() override = default;

 private:
  mojom::Service* GetServiceProxy(const std::string& service_name) {
    auto proxy_it = proxies_->find(service_name);
    if (proxy_it != proxies_->end())
      return proxy_it->second.get();

    return nullptr;
  }

  // mojom::Connector:
  void BindInterface(const ServiceFilter& service_filter,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe,
                     mojom::BindInterfacePriority priority,
                     BindInterfaceCallback callback) override {
    auto* proxy = GetServiceProxy(service_filter.service_name());
    if (!proxy && factory_->ignore_unknown_service_requests()) {
      std::move(callback).Run(mojom::ConnectResult::ACCESS_DENIED,
                              base::nullopt);
      return;
    }

    CHECK(proxy)
        << "TestConnectorFactory received a BindInterface request for an "
        << "unregistered service '" << service_filter.service_name() << "'";
    proxy->OnBindInterface(
        BindSourceInfo(Identity("TestConnectorFactory", test_instance_group_,
                                base::Token{}, fake_guid_),
                       CapabilitySet()),
        interface_name, std::move(interface_pipe), base::DoNothing());
    std::move(callback).Run(mojom::ConnectResult::SUCCEEDED, base::nullopt);
  }

  void WarmService(const ServiceFilter& filter,
                   WarmServiceCallback callback) override {
    NOTREACHED();
  }

  void QueryService(const std::string& service_name,
                    QueryServiceCallback callback) override {
    NOTREACHED();
  }

  void RegisterServiceInstance(
      const Identity& identity,
      mojo::ScopedMessagePipeHandle service,
      mojom::PIDReceiverRequest pid_receiver_request,
      RegisterServiceInstanceCallback callback) override {
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

  const base::Token fake_guid_;
  TestConnectorFactory* const factory_;
  TestConnectorFactory::NameToServiceProxyMap* const proxies_;
  const base::Token test_instance_group_;
  mojo::BindingSet<mojom::Connector> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ProxiedServiceConnector);
};

}  // namespace

TestConnectorFactory::TestConnectorFactory() {
  test_instance_group_ = base::Token::CreateRandom();
  impl_ = std::make_unique<ProxiedServiceConnector>(this, &service_proxies_,
                                                    test_instance_group_);
}

TestConnectorFactory::~TestConnectorFactory() = default;

Connector* TestConnectorFactory::GetDefaultConnector() {
  if (!default_connector_)
    default_connector_ = CreateConnector();
  return default_connector_.get();
}

std::unique_ptr<Connector> TestConnectorFactory::CreateConnector() {
  mojom::ConnectorPtr proxy;
  impl_->Clone(mojo::MakeRequest(&proxy));
  return std::make_unique<Connector>(std::move(proxy));
}

mojom::ServiceRequest TestConnectorFactory::RegisterInstance(
    const std::string& service_name) {
  mojom::ServicePtr proxy;
  mojom::ServiceRequest request = mojo::MakeRequest(&proxy);
  proxy->OnStart(Identity(service_name, test_instance_group_, base::Token{},
                          base::Token::CreateRandom()),
                 base::BindOnce(&TestConnectorFactory::OnStartResponseHandler,
                                base::Unretained(this), service_name));
  service_proxies_[service_name] = std::move(proxy);
  return request;
}

void TestConnectorFactory::OnStartResponseHandler(
    const std::string& service_name,
    mojom::ConnectorRequest connector_request,
    mojom::ServiceControlAssociatedRequest control_request) {
  impl_->Clone(std::move(connector_request));
  service_control_bindings_.AddBinding(this, std::move(control_request),
                                       service_name);
}

void TestConnectorFactory::RequestQuit() {
  if (ignore_quit_requests_)
    return;

  service_proxies_.erase(service_control_bindings_.dispatch_context());
  service_control_bindings_.RemoveBinding(
      service_control_bindings_.dispatch_binding());
}

}  // namespace service_manager
