// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SERVICE_MANAGER_H_
#define SERVICES_SERVICE_MANAGER_SERVICE_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/process/process.h"
#include "base/token.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/service_manager/catalog.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/service_manager/service_instance_registry.h"
#include "services/service_manager/service_process_launcher_factory.h"

namespace service_manager {

class ServiceInstance;

class ServiceManager : public Service {
 public:
  // Constructs a new ServiceManager instance which exclusively uses |manifests|
  // as its source of truth regarding what services exist and how they should
  // be configured.
  //
  // |service_process_launcher_factory| is an instance of an object capable of
  // vending implementations of ServiceProcessLauncher, e.g. for out-of-process
  // execution.
  explicit ServiceManager(std::unique_ptr<ServiceProcessLauncherFactory>
                              service_process_launcher_factory,
                          const std::vector<Manifest>& manifests);
  ~ServiceManager() override;

  // Provide a callback to be notified whenever an instance is destroyed.
  // Typically the creator of the Service Manager will use this to determine
  // when some set of services it created are destroyed, so it can shut down.
  void SetInstanceQuitCallback(base::Callback<void(const Identity&)> callback);

  // Directly requests that the Service Manager start a new instance for
  // |service_name| if one is not already running.
  //
  // TODO(https://crbug.com/904240): Remove this method.
  void StartService(const std::string& service_name);

  // Creates a service instance for |identity|. This is intended for use by the
  // Service Manager's embedder to register instances directly, without
  // requiring a Connector.
  //
  // |pid_receiver_request| may be null, in which case the service manager
  // assumes the new service is running in this process.
  void RegisterService(const Identity& identity,
                       mojom::ServicePtr service,
                       mojom::PIDReceiverRequest pid_receiver_request);

  // Determine information about |service_name| from its manifests. Returns
  // false if the identity does not have a catalog entry.
  bool QueryCatalog(const std::string& service_name,
                    const base::Token& instance_group,
                    std::string* sandbox_type);

  // Completes a connection between a source and target application as defined
  // by |params|. If no existing instance of the target service is running, one
  // will be loaded.
  void Connect(const ServiceFilter& partial_target_filter,
               const Identity& source_identity,
               const base::Optional<std::string>& interface_name,
               mojo::ScopedMessagePipeHandle receiving_pipe,
               mojo::PendingRemote<mojom::Service> service_remote,
               mojo::PendingReceiver<mojom::PIDReceiver> pid_receiver,
               mojom::BindInterfacePriority priority,
               mojom::Connector::BindInterfaceCallback callback);

  // Variant of the above when no Service remote or PIDReceiver is provided by
  // the caller.
  void Connect(const ServiceFilter& partial_target_filter,
               const Identity& source_identity,
               const std::string& interface_name,
               mojo::ScopedMessagePipeHandle receiving_pipe,
               mojom::BindInterfacePriority priority,
               mojom::Connector::BindInterfaceCallback callback);

  // Variant of the above where no interface name, receiving pipe, Service
  // remote, or PIDReceiver is provided by the caller.
  void Connect(const ServiceFilter& partial_target_filter,
               const Identity& source_identity,
               mojom::Connector::BindInterfaceCallback callback);

  // Variant of the above where no interface name or receiving pipe is provided
  // by the caller, but a Service remote and PIDReceiver are provided.
  void Connect(const ServiceFilter& partial_target_filter,
               const Identity& source_identity,
               mojo::PendingRemote<mojom::Service> service_remote,
               mojo::PendingReceiver<mojom::PIDReceiver> pid_receiver,
               mojom::Connector::BindInterfaceCallback callback);

 private:
  friend class ServiceInstance;

  // Erases |instance| from the instance registry. Following this call it is
  // impossible for any call to GetExistingInstance() to return |instance| even
  // though the instance may continue to exist and send requests to the Service
  // Manager.
  void MakeInstanceUnreachable(ServiceInstance* instance);

  // Called when |instance| no longer has any connections to the remote service
  // instance, or when some other fatal error is encountered in managing the
  // instance. Deletes |instance|.
  void DestroyInstance(ServiceInstance* instance);

  // Called by a ServiceInstance as it's being destroyed.
  void OnInstanceStopped(const Identity& identity);

  // Returns a running instance identified by |identity|.
  ServiceInstance* GetExistingInstance(const Identity& identity) const;

  void NotifyServiceCreated(ServiceInstance* instance);
  void NotifyServiceStarted(const Identity& identity, base::ProcessId pid);
  void NotifyServiceFailedToStart(const Identity& identity);

  void NotifyServicePIDReceived(const Identity& identity, base::ProcessId pid);

  ServiceInstance* CreateServiceInstance(const Identity& identity,
                                         const Manifest& manifest);

  // Called from the instance implementing mojom::ServiceManager.
  void AddListener(mojom::ServiceManagerListenerPtr listener);

  void CreateServiceWithFactory(const ServiceFilter& service_factory_filter,
                                const std::string& name,
                                mojom::ServiceRequest request,
                                mojom::PIDReceiverPtr pid_receiver);

  // Returns a running ServiceFactory for |filter|. If there is not one running,
  // one is started.
  mojom::ServiceFactory* GetServiceFactory(const ServiceFilter& filter);
  void OnServiceFactoryLost(const ServiceFilter& which);

  // Service:
  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle receiving_pipe) override;

  ServiceBinding service_binding_{this};

  // Ownership of all ServiceInstances.
  using InstanceMap =
      std::set<std::unique_ptr<ServiceInstance>, base::UniquePtrComparator>;
  InstanceMap instances_;

  Catalog catalog_;

  // Maps service identities to reachable instances, allowing for lookup of
  // running instances by ServiceFilter.
  ServiceInstanceRegistry instance_registry_;

  // Always points to the ServiceManager's own Instance. Note that this
  // ServiceInstance still has an entry in |instances_|.
  ServiceInstance* service_manager_instance_;

  std::map<ServiceFilter, mojom::ServiceFactoryPtr> service_factories_;
  mojo::InterfacePtrSet<mojom::ServiceManagerListener> listeners_;
  base::Callback<void(const Identity&)> instance_quit_callback_;
  std::unique_ptr<ServiceProcessLauncherFactory>
      service_process_launcher_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceManager);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SERVICE_MANAGER_H_
