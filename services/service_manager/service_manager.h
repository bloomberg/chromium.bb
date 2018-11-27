// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SERVICE_MANAGER_H_
#define SERVICES_SERVICE_MANAGER_SERVICE_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/catalog/catalog.h"
#include "services/catalog/service_options.h"
#include "services/service_manager/connect_params.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/interface_provider_spec.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/service_manager/runner/host/service_process_launcher_factory.h"
#include "services/service_manager/service_overrides.h"

namespace catalog {
class ManifestProvider;
}

namespace service_manager {

class ServiceContext;

// Creates an identity for the singular Service Manager instance which is always
// present in the system.
const Identity& GetServiceManagerInstanceIdentity();

class ServiceManager {
 public:
  // |service_process_launcher_factory| is an instance of an object capable of
  // vending implementations of ServiceProcessLauncher, e.g. for out-of-process
  // execution.
  //
  // |catalog_contents|, if not null, will be used to prepoulate the service
  // manager catalog with a fixed data set. |manifest_provider|, if not null,
  // will be consulted for dynamic manifest resolution if a manifest is not
  // found within the catalog; if used, |manifest_provider| is not owned and
  // must outlive this ServiceManager.
  ServiceManager(std::unique_ptr<ServiceProcessLauncherFactory>
                     service_process_launcher_factory,
                 std::unique_ptr<base::Value> catalog_contents,
                 catalog::ManifestProvider* manifest_provider);
  ~ServiceManager();

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
  void Connect(std::unique_ptr<ConnectParams> params);

 private:
  class Instance;
  class IdentityToInstanceMap;
  class ServiceImpl;

  // Used in CreateInstance to specify how an instance should be shared between
  // various identities.
  enum class InstanceType {
    // All fields of the instance's identity are relevant, so instances are
    // generally isolated to their own instance group, and there may be multiple
    // instances of the service within each instance group.
    kRegular,

    // There may be multiple instances of the service qualified by instance ID,
    // but all such instances are shared across instance group boundaries. The
    // instance group is therefore effectively ignored when resolving an
    // Identity to a running instance.
    kSharedAcrossInstanceGroups,

    // A single instance of the service exists globally. For all connections to
    // the service, both instance group and instance ID are ignored when
    // resolving the Identity.
    kSingleton,
  };

  void InitCatalog(mojom::ServicePtr catalog);

  // Called when |instance| encounters an error. Deletes |instance|.
  void OnInstanceError(Instance* instance);

  // Called when |instance| becomes unreachable to new connections because it
  // no longer has any pipes to the ServiceManager.
  void OnInstanceUnreachable(Instance* instance);

  // Called by an Instance as it's being destroyed.
  void OnInstanceStopped(const Identity& identity);

  // Returns a running instance identified by |identity|.
  Instance* GetExistingInstance(const Identity& identity) const;

  // Erases any identities mapping to |instance|. Following this call it is
  // impossible for any call to GetExistingInstance() to return |instance|.
  void EraseInstanceIdentity(Instance* instance);

  void NotifyServiceCreated(Instance* instance);
  void NotifyServiceStarted(const Identity& identity, base::ProcessId pid);
  void NotifyServiceFailedToStart(const Identity& identity);

  void NotifyServicePIDReceived(const Identity& identity, base::ProcessId pid);

  Instance* CreateInstance(const Identity& identity,
                           InstanceType instance_type,
                           const InterfaceProviderSpecMap& specs,
                           const catalog::ServiceOptions& options);

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

  base::WeakPtr<ServiceManager> GetWeakPtr();

  // Ownership of all Instances.
  using InstanceMap = std::map<Instance*, std::unique_ptr<Instance>>;
  InstanceMap instances_;

  catalog::Catalog catalog_;

  // Maps service identities to reachable instances. Note that the Instance
  // values stored in that map are NOT owned by this map.
  std::unique_ptr<IdentityToInstanceMap> identity_to_instance_;

  // Always points to the ServiceManager's own Instance. Note that this
  // Instance still has an entry in |instances_|.
  Instance* service_manager_instance_;

  std::map<ServiceFilter, mojom::ServiceFactoryPtr> service_factories_;
  mojo::InterfacePtrSet<mojom::ServiceManagerListener> listeners_;
  base::Callback<void(const Identity&)> instance_quit_callback_;
  std::unique_ptr<ServiceProcessLauncherFactory>
      service_process_launcher_factory_;
  std::unique_ptr<ServiceContext> service_context_;
  base::WeakPtrFactory<ServiceManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceManager);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SERVICE_MANAGER_H_
