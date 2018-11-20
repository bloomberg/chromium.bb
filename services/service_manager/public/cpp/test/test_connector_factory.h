// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_TEST_TEST_CONNECTOR_FACTORY_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_TEST_TEST_CONNECTOR_FACTORY_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/token.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/service_control.mojom.h"

namespace service_manager {

class Service;

// Creates Connector instances which route BindInterface requests directly to
// one or several given Service implementations. Useful for testing production
// code which is parameterized over a Connector, while bypassing all the
// Service Manager machinery. Typical usage should look something like:
//
//     TEST(MyTest, Foo) {
//       // Your implementation of service_manager::Service.
//       auto impl =  std::make_unique<MyServiceImpl>();
//       std::unique_ptr<TestConnectorFactory> connector_factory =
//          TestConnectorFactory::CreateForUniqueService(std::move(impl));
//       std::unique_ptr<service_manager::Connector> connector =
//           connector_factory->CreateConnector();
//       RunSomeClientCode(connector.get());
//     }
//
// Where |RunSomeClientCode()| would typically be some production code that
// expects a functioning Connector and uses it to connect to the service you're
// testing.
// Note that when using CreateForUniqueService(), Connectors created by this
// factory ignores the target service name in BindInterface calls: interface
// requests are always routed to a single target Service instance.
// If you need to set-up more than one service, use CreateForServices(), like:
//   TestConnectorFactory::NameToServiceMap services;
//   services.insert(std::make_pair("data_decoder",
//                                  std::make_unique<DataDecoderService>()));
//   services.insert(std::make_pair("file",
//                                  std::make_unique<file::FileService>()));
//   std::unique_ptr<TestConnectorFactory> connector_factory =
//       TestConnectorFactory::CreateForServices(std::move(services));
//   std::unique_ptr<service_manager::Connector> connector =
//       connector_factory->CreateConnector();
//   ...
class TestConnectorFactory : public mojom::ServiceControl {
 public:
  // Creates a simple TestConnectorFactory which can be used register unowned
  // Service instances and vend Connectors which can connect to them.
  TestConnectorFactory();
  ~TestConnectorFactory() override;

  // A mapping from service names to Service proxies for unowned Service
  // instances.
  using NameToServiceProxyMap = std::map<std::string, mojom::ServicePtr>;

  // Used to hold a mapping from service names to callbacks which can
  // instantiate an instance of those services.
  using ServiceHandler =
      base::RepeatingCallback<void(service_manager::mojom::ServiceRequest)>;
  using NameToServiceHandlerMap = std::map<std::string, ServiceHandler>;

  // Used to hold a mapping from service names to owned Service instances.
  using NameToServiceMap = std::map<std::string, std::unique_ptr<Service>>;

  // Constructs a new TestConnectorFactory which creates Connectors whose
  // requests are routed directly to |service|. If
  // |release_service_on_quit_request| is set to true, the Connector will
  // release the service instance when the service requests to be quit.
  static std::unique_ptr<TestConnectorFactory> CreateForUniqueService(
      std::unique_ptr<Service> service,
      bool release_service_on_quit_request = false);

  // Constructs a new TestConnectorFactory which creates Connectors whose
  // requests are routed directly to a service in |services| based on the name
  // they are associated with. If |release_service_on_quit_request|
  // is set to true, the Connector will release the service instance when the
  // service requests to be quit.
  static std::unique_ptr<TestConnectorFactory> CreateForServices(
      NameToServiceMap services,
      bool release_service_on_quit_request = false);

  // Creates a new connector which routes BindInterfaces requests directly to
  // the Service instance associated with this factory.
  std::unique_ptr<Connector> CreateConnector();

  // Registers a Service instance not owned by this TestConnectorFactory.
  // Returns a ServiceRequest which the instance must bind in order to receive
  // simulated events from this object.
  mojom::ServiceRequest RegisterInstance(const std::string& service_name);

  // Registers a callback to start an instance of a specific named service
  // whenever it's needed by this TestConnectorFactory. This may be used instead
  // of a one-time registration via |RegisterInstance()| in cases where a
  // service-under-test may be stopped and restarted multiple times during a
  // single test.
  void RegisterServiceHandler(const std::string& service_name,
                              const ServiceHandler& handler);

  const base::Token& test_instance_group() const {
    return test_instance_group_;
  }

  // Normally a TestConnectorFactory will assert if asked to route a request to
  // an unregistered service. If this is set to |true|, such requests will be
  // silently ignored instead.
  bool ignore_unknown_service_requests() const {
    return ignore_unknown_service_requests_;
  }
  void set_ignore_unknown_service_requests(bool ignore) {
    ignore_unknown_service_requests_ = ignore;
  }

  // Normally when a service instance registered via either |RegisterInstance()|
  // or |RegisterServiceHandler()| requests termination from the Service
  // Manager, TestConnectorFactory immediately severs the service instance's
  // connection, typically triggering the service's shutdown path.
  //
  // If this is set to |true| (defaults to |false|), quit requests are ignored
  // and each service instance will remain connected to the TestConnectorFactory
  // until either it or the TestConnoctorFactory is destroyed.
  void set_ignore_quit_requests(bool ignore) { ignore_quit_requests_ = ignore; }

 private:
  explicit TestConnectorFactory(std::unique_ptr<mojom::Connector> impl,
                                const base::Token& test_instance_group);

  void OnStartResponseHandler(
      const std::string& service_name,
      mojom::ConnectorRequest connector_request,
      mojom::ServiceControlAssociatedRequest control_request);

  // mojom::ServiceControl:
  void RequestQuit() override;

  NameToServiceMap names_to_services_;

  std::unique_ptr<mojom::Connector> impl_;
  base::Token test_instance_group_;

  // Mapping used only in the default-constructed case where Service instances
  // are unowned by the TestConnectorFactory. Maps service names to their
  // proxies.
  NameToServiceProxyMap service_proxies_;

  // Maps service names to handlers which can construct service instances.
  NameToServiceHandlerMap service_handlers_;

  // ServiceControl bindings which receive and process RequestQuit requests from
  // connected service instances. The associated service name is used as
  // context.
  mojo::AssociatedBindingSet<mojom::ServiceControl, std::string>
      service_control_bindings_;

  bool ignore_unknown_service_requests_ = false;
  bool ignore_quit_requests_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestConnectorFactory);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_TEST_TEST_CONNECTOR_FACTORY_H_
