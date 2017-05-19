// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_PROVIDER_SPEC_STRUCT_TRAITS_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_PROVIDER_SPEC_STRUCT_TRAITS_H_

#include "services/service_manager/public/cpp/interface_provider_spec.h"
#include "services/service_manager/public/interfaces/interface_provider_spec.mojom.h"
#include "services/service_manager/public/interfaces/interfaces_export.h"

namespace mojo {

template <>
struct SERVICE_MANAGER_MOJOM_EXPORT
    StructTraits<service_manager::mojom::InterfaceProviderSpec::DataView,
                 service_manager::InterfaceProviderSpec> {
  static const std::map<service_manager::Capability,
                        service_manager::InterfaceSet>&
      provides(const service_manager::InterfaceProviderSpec& spec) {
    return spec.provides;
  }
  static const std::map<service_manager::Name, service_manager::CapabilitySet>&
      requires(const service_manager::InterfaceProviderSpec& spec) {
    return spec.requires;
  }
  static bool Read(service_manager::mojom::InterfaceProviderSpecDataView data,
                   service_manager::InterfaceProviderSpec* out) {
    return data.ReadProvides(&out->provides) &&
           data.ReadRequires(&out->requires);
  }
};

template <>
struct SERVICE_MANAGER_MOJOM_EXPORT
    StructTraits<service_manager::mojom::InterfaceSet::DataView,
                 service_manager::InterfaceSet> {
  static std::vector<std::string> interfaces(
      const service_manager::InterfaceSet& spec) {
    std::vector<std::string> vec;
    for (const auto& interface_name : spec)
      vec.push_back(interface_name);
    return vec;
  }
  static bool Read(service_manager::mojom::InterfaceSetDataView data,
                   service_manager::InterfaceSet* out) {
    ArrayDataView<StringDataView> interfaces_data_view;
    data.GetInterfacesDataView(&interfaces_data_view);
    for (size_t i = 0; i < interfaces_data_view.size(); ++i) {
      std::string interface_name;
      if (!interfaces_data_view.Read(i, &interface_name))
        return false;
      out->insert(std::move(interface_name));
    }
    return true;
  }
};

template <>
struct SERVICE_MANAGER_MOJOM_EXPORT
    StructTraits<service_manager::mojom::CapabilitySet::DataView,
                 service_manager::CapabilitySet> {
  static std::vector<std::string> capabilities(
      const service_manager::CapabilitySet& spec) {
    std::vector<std::string> vec;
    for (const auto& capability : spec)
      vec.push_back(capability);
    return vec;
  }
  static bool Read(service_manager::mojom::CapabilitySetDataView data,
                   service_manager::CapabilitySet* out) {
    ArrayDataView<StringDataView> capabilities_data_view;
    data.GetCapabilitiesDataView(&capabilities_data_view);
    for (size_t i = 0; i < capabilities_data_view.size(); ++i) {
      std::string capability;
      if (!capabilities_data_view.Read(i, &capability))
        return false;
      out->insert(std::move(capability));
    }
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_PROVIDER_SPEC_STRUCT_TRAITS_H_
