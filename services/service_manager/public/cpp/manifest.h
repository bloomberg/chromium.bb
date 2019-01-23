// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_MANIFEST_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_MANIFEST_H_

#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace service_manager {

namespace internal {

template <typename InterfaceType>
const char* GetInterfaceName() {
  return InterfaceType::Name_;
}

template <typename... InterfaceTypes>
std::set<std::string> GetInterfaceNames() {
  return std::set<std::string>({GetInterfaceName<InterfaceTypes>()...});
}

}  // namespace internal

// Represents metadata about a service that the Service Manager needs in order
// to start and control instances of that given service. This data is provided
// to the Service Manager at initialization time for every known service in the
// system.
//
// A service will typically define a dedicated manifest target in their public
// C++ client library with a single GetManifest() function that returns a
// const ref to a function-local static Manifest. Then any Service Manager
// embedder can reference the manifest in order to include support for that
// service within its runtime environment.
//
// Instead of constructing a Manifest manually, prefer to use a ManifestBuilder
// defined in manifest_builder.h for more readable and maintainable manifest
// definitions.
struct COMPONENT_EXPORT(SERVICE_MANAGER_CPP) Manifest {
  // Represents the display name of this service (in e.g. a task manager).
  //
  // TODO(https://crbug.com/915806): Extend this to support resource IDs in
  // addition to raw strings.
  struct COMPONENT_EXPORT(SERVICE_MANAGER_CPP) DisplayName {
    DisplayName() = default;
    explicit DisplayName(const std::string& raw_string)
        : raw_string(raw_string) {}

    std::string raw_string;
  };

  enum class InstanceSharingPolicy {
    // Instances of the service are never shared across groups or instance IDs.
    // Every tuple of group and instance ID corresponds to a unique instance.
    kNoSharing,

    // Only one instance of the service can exist at a time, and any combination
    // of group and instance ID refers to the same single instance. i.e. group
    // and instance ID are effectively ignored when locating the instance of
    // this service on behalf of clients.
    kSingleton,

    // At most one instance of this service will exist with a given instance ID.
    // i.e., instance group is effectively ignored when locating an instance of
    // the service on behalf of a client.
    kSharedAcrossGroups,
  };

  // Miscellanous options which control how the service is launched and how it
  // can interact with other service instances in the system.
  struct COMPONENT_EXPORT(SERVICE_MANAGER_CPP) Options {
    Options();
    Options(const Options&);
    Options(Options&&);
    ~Options();

    Options& operator=(Options&&);
    Options& operator=(const Options&);

    // Indicates how instances of this service may be shared across clients.
    InstanceSharingPolicy instance_sharing_policy =
        InstanceSharingPolicy::kNoSharing;

    // If |true|, this service is allowed to connect to other service instances
    // in instance groups other than its own. This is considered a privileged
    // capability, as instance grouping provides natural boundaries for service
    // instance isolation.
    bool can_connect_to_instances_in_any_group = false;

    // If |true|, this service is allowed to connect to other services instances
    // with a specific instance ID. This is considered a privileged capability
    // since it allows this service to instigate the creation of an arbitrary
    // number of service instances.
    bool can_connect_to_instances_with_any_id = false;

    // If |true|, this service is allowed to directly register new service
    // instances with the Service Manager. This is considered a privileged
    // capability since it grants this service a significant degree of control
    // over the entire system's behavior. For example, the service could
    // completely replace other system services and therefore intercept requests
    // intended for those services.
    bool can_register_other_service_instances = false;

    // The type of sandboxing required by instances of this service.
    //
    // TODO(https://crbug.com/915806): Make this field a SandboxType enum.
    std::string sandbox_type{"utility"};
  };

  // Represents a file required by instances of the service despite being
  // inaccessible to the service directly, due to e.g. sandboxing constraints.
  //
  // Currently ignored on platforms other than Android and Linux.
  struct PreloadedFileInfo {
    // A key which can be used by the service implementation to locate the
    // file's open descriptor via |base::FileDescriptorStore|.
    std::string key;

    // The path to the file. On Linux this is relative to the main Service
    // Manager embedder's executable (i.e. relative to base::DIR_EXE.) On
    // Android it's an APK asset path.
    base::FilePath path;
  };

  // A helper for Manifest writers to create a set of interfaces to be used in
  // in exposed capabilities.
  template <typename... InterfaceTypes>
  struct InterfaceList {};

  // Represents a capability exposed by a service. Every exposed capability
  // consists of a name (implicitly scoped to the service) and a list of
  // interfaces the service is willing to bind on behalf of clients who have
  // been granted the capability.
  //
  // See RequiredCapability for more details on how exposed capabilities are
  // used by the system.
  struct COMPONENT_EXPORT(SERVICE_MANAGER_CPP) ExposedCapability {
    ExposedCapability();
    ExposedCapability(const ExposedCapability&);
    ExposedCapability(ExposedCapability&&);

    template <typename... InterfaceTypes>
    ExposedCapability(const std::string& capability_name,
                      InterfaceList<InterfaceTypes...> interfaces)
        : capability_name(capability_name),
          interface_names(internal::GetInterfaceNames<InterfaceTypes...>()) {}

    // Prefer the above constructor. This exists to support genenerated code.
    ExposedCapability(const std::string& capability_name,
                      std::set<const char*> interface_names);

    ~ExposedCapability();

    ExposedCapability& operator=(const ExposedCapability&);
    ExposedCapability& operator=(ExposedCapability&&);

    // The name of this capability.
    std::string capability_name;

    // The list of interfaces accessible to clients granted this capability.
    std::set<std::string> interface_names;
  };

  // Represents a capability required by a service. Every required capability
  // is a simple pairwise combination of service name and capability name, where
  // the capability name corresponds to a capability exposed by the named
  // service.
  //
  // A service which requires a specific capability is implicitly granted that
  // capability by the Service Manager. If a service requests an interface from
  // another service but has not been granted any capability which includes that
  // interface, the Service Manager will block the request without ever routing
  // it to an instance of the target service.
  struct RequiredCapability {
    // The name of the service which exposes this required capability.
    std::string service_name;

    // The name of the capability to require. This must match the name of a
    // capability exposed by |service_name|'s own Manifest.
    std::string capability_name;
  };

  // DEPRECATED: This will be removed soon. Don't add new uses of interface
  // filters. Instead prefer to define explicit broker interfaces and expose
  // them through a top-level ExposedCapability.
  //
  // Services may define capabilities to be scoped within a named interface
  // filter. These capabilities do not apply to normal interface binding
  // requests (i.e. requests made by clients through |Connector.BindInterface|).
  // Instead, the exposing service may use |Connector.FilterInterfaces| to
  // set up an InterfaceProvider pipe proxied through the Service Manager. The
  // Service Manager will filter interface requests on that pipe according to
  // the given filter name and remote service name. The remote service must in
  // turn require one or more capabilities from the named filter in order to
  // access any interfaces via the proxied InterfaceProvider, which the exposing
  // service must pass to the remote service somehow.
  //
  // If this all sounds very confusing, that's because it is very confusing.
  // Hence the "DEPRECATED" bit.
  struct COMPONENT_EXPORT(SERVICE_MANAGER_CPP)
      ExposedInterfaceFilterCapability {
    ExposedInterfaceFilterCapability();
    ExposedInterfaceFilterCapability(ExposedInterfaceFilterCapability&&);
    ExposedInterfaceFilterCapability(const ExposedInterfaceFilterCapability&);

    template <typename... InterfaceTypes>
    ExposedInterfaceFilterCapability(
        const std::string& filter_name,
        const std::string& capability_name,
        InterfaceList<InterfaceTypes...> interfaces)
        : filter_name(filter_name),
          capability_name(capability_name),
          interface_names(internal::GetInterfaceNames<InterfaceTypes...>()) {}

    // Prefer the above constructor. This exists to support genenerated code.
    ExposedInterfaceFilterCapability(const std::string& filter_name,
                                     const std::string& capability_name,
                                     std::set<const char*> interface_names);

    ~ExposedInterfaceFilterCapability();

    ExposedInterfaceFilterCapability& operator=(
        const ExposedInterfaceFilterCapability&);
    ExposedInterfaceFilterCapability& operator=(
        ExposedInterfaceFilterCapability&&);

    std::string filter_name;
    std::string capability_name;
    std::set<std::string> interface_names;
  };

  // DEPRECATED: This will be removed soon. Don't add new uses of interface
  // filters.
  //
  // This is like RequiredCapability, except that it only grants the requiring
  // service access to a set of interfaces on a specific InterfaceProvider,
  // filtered by the exposing service according to an
  // ExposedInterfaceFilterCapability in that service's manifest. See notes on
  // ExposedInterfaceFilterCapability.
  struct RequiredInterfaceFilterCapability {
    std::string service_name;
    std::string filter_name;
    std::string capability_name;
  };

  Manifest();
  Manifest(const Manifest&);
  Manifest(Manifest&&);

  ~Manifest();

  Manifest& operator=(const Manifest&);
  Manifest& operator=(Manifest&&);

  // Amends this Manifest with a subset of |other|. Namely, exposed and required
  // capabilities, exposed and required interface filter capabilities, packaged
  // services, and preloaded files are all added from |other| if present.
  Manifest& Amend(Manifest other);

  std::string service_name;
  DisplayName display_name;
  Options options;
  std::vector<ExposedCapability> exposed_capabilities;
  std::vector<RequiredCapability> required_capabilities;
  std::vector<ExposedInterfaceFilterCapability>
      exposed_interface_filter_capabilities;
  std::vector<RequiredInterfaceFilterCapability>
      required_interface_filter_capabilities;
  std::vector<Manifest> packaged_services;
  std::vector<PreloadedFileInfo> preloaded_files;

  // The list of interfaces that this service are allowed to connect to
  // unconditionally on any service.
  std::set<std::string> interfaces_bindable_on_any_service;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_MANIFEST_H_
