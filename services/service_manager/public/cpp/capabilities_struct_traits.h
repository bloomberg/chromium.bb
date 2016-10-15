// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CAPABILITIES_STRUCT_TRAITS_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CAPABILITIES_STRUCT_TRAITS_H_

#include "services/service_manager/public/cpp/capabilities.h"
#include "services/service_manager/public/interfaces/capabilities.mojom.h"

namespace mojo {

template <>
struct StructTraits<service_manager::mojom::CapabilitySpec::DataView,
                    service_manager::CapabilitySpec> {
  static const std::map<service_manager::Class, service_manager::Interfaces>&
  provided(const service_manager::CapabilitySpec& spec) {
    return spec.provided;
  }
  static const std::map<service_manager::Name, service_manager::Classes>&
  required(const service_manager::CapabilitySpec& spec) {
    return spec.required;
  }
  static bool Read(service_manager::mojom::CapabilitySpecDataView data,
                   service_manager::CapabilitySpec* out) {
    return data.ReadProvided(&out->provided) &&
           data.ReadRequired(&out->required);
  }
};

template <>
struct StructTraits<service_manager::mojom::Interfaces::DataView,
                    service_manager::Interfaces> {
  static std::vector<std::string> interfaces(
      const service_manager::Interfaces& spec) {
    std::vector<std::string> vec;
    for (const auto& interface_name : spec)
      vec.push_back(interface_name);
    return vec;
  }
  static bool Read(service_manager::mojom::InterfacesDataView data,
                   service_manager::Interfaces* out) {
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
struct StructTraits<service_manager::mojom::Classes::DataView,
                    service_manager::Classes> {
  static std::vector<std::string> classes(
      const service_manager::Classes& spec) {
    std::vector<std::string> vec;
    for (const auto& class_name : spec)
      vec.push_back(class_name);
    return vec;
  }
  static bool Read(service_manager::mojom::ClassesDataView data,
                   service_manager::Classes* out) {
    ArrayDataView<StringDataView> classes_data_view;
    data.GetClassesDataView(&classes_data_view);
    for (size_t i = 0; i < classes_data_view.size(); ++i) {
      std::string class_name;
      if (!classes_data_view.Read(i, &class_name))
        return false;
      out->insert(std::move(class_name));
    }
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CAPABILITIES_STRUCT_TRAITS_H_
