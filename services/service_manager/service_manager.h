// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SERVICE_MANAGER_H_
#define SERVICES_SERVICE_MANAGER_SERVICE_MANAGER_H_

#include <memory>
#include <set>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/process/process.h"
#include "base/token.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/service_manager/catalog.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"
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
  // |metadata_receiver| may be null, in which case the Service Manager assumes
  // the new service is running in the calling process.
  //
  // Returns |true| if registration succeeded, or |false| otherwise.
  bool RegisterService(
      const Identity& identity,
      mojo::PendingRemote<mojom::Service> service,
      mojo::PendingReceiver<mojom::ProcessMetadata> metadata_receiver);

  // Determine information about |service_name| from its manifests. Returns
  // false if the identity does not have a catalog entry.
  bool QueryCatalog(const std::string& service_name,
                    const base::Token& instance_group,
                    std::string* sandbox_type);

  // Attempts to locate a ServiceInstance as a target for a connection request
  // from |source_instance| by matching against |partial_target_filter|. If a
  // suitable instance exists it is returned, otherwise the Service Manager
  // attempts to create a new suitable instance.
  //
  // Returns null if a matching instance did not exist and could not be created,
  // otherwise returns a valid ServiceInstance which matches
  // |partial_target_filter| from |source_instance|'s perspective.
  ServiceInstance* FindOrCreateMatchingTargetInstance(
      const ServiceInstance& source_instance,
      const ServiceFilter& partial_target_filter);

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

  void NotifyServiceCreated(const ServiceInstance& instance);
  void NotifyServiceStarted(const Identity& identity, base::ProcessId pid);
  void NotifyServiceFailedToStart(const Identity& identity);

  void NotifyServicePIDReceived(const Identity& identity, base::ProcessId pid);

  ServiceInstance* CreateServiceInstance(const Identity& identity,
                                         const Manifest& manifest);

  // Called from the instance implementing mojom::ServiceManager.
  void AddListener(mojom::ServiceManagerListenerPtr listener);

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

  mojo::InterfacePtrSet<mojom::ServiceManagerListener> listeners_;
  base::Callback<void(const Identity&)> instance_quit_callback_;
  std::unique_ptr<ServiceProcessLauncherFactory>
      service_process_launcher_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceManager);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SERVICE_MANAGER_H_
