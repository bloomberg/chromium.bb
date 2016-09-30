// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_CAPABILITIES_STRUCT_TRAITS_H_
#define SERVICES_SHELL_PUBLIC_CPP_CAPABILITIES_STRUCT_TRAITS_H_

#include "services/shell/public/cpp/capabilities.h"
#include "services/shell/public/interfaces/capabilities.mojom.h"

namespace mojo {

template <>
struct StructTraits<shell::mojom::CapabilityRequest::DataView,
                    shell::CapabilityRequest> {
  static const shell::Classes& classes(
      const shell::CapabilityRequest& request) {
    return request.classes;
  }
  static const shell::Interfaces& interfaces(
      const shell::CapabilityRequest& request) {
    return request.interfaces;
  }
  static bool Read(shell::mojom::CapabilityRequestDataView data,
                   shell::CapabilityRequest* out) {
    ArrayDataView<StringDataView> classes_data_view;
    data.GetClassesDataView(&classes_data_view);
    for (size_t i = 0; i < classes_data_view.size(); ++i) {
      std::string clazz;
      if (!classes_data_view.Read(i, &clazz))
        return false;
      out->classes.insert(std::move(clazz));
    }

    ArrayDataView<StringDataView> interfaces_data_view;
    data.GetInterfacesDataView(&interfaces_data_view);
    for (size_t i = 0; i < interfaces_data_view.size(); ++i) {
      std::string interface_name;
      if (!interfaces_data_view.Read(i, &interface_name))
        return false;
      out->interfaces.insert(std::move(interface_name));
    }
    return true;
  }
};

template <>
struct StructTraits<shell::mojom::CapabilitySpec::DataView,
                    shell::CapabilitySpec> {
  static const std::map<shell::Class, shell::Interfaces>& provided(
      const shell::CapabilitySpec& spec) {
    return spec.provided;
  }
  static const std::map<shell::Name, shell::CapabilityRequest>& required(
      const shell::CapabilitySpec& spec) {
    return spec.required;
  }
  static bool Read(shell::mojom::CapabilitySpecDataView data,
                   shell::CapabilitySpec* out) {
    MapDataView<StringDataView, ArrayDataView<StringDataView>>
        provided_data_view;
    data.GetProvidedDataView(&provided_data_view);

    for (size_t i = 0; i < provided_data_view.keys().size(); ++i) {
      std::string clazz;
      if (!provided_data_view.keys().Read(i, &clazz))
        return false;

      std::vector<std::string> interfaces_vec;
      if (!provided_data_view.values().Read(i, &interfaces_vec))
        return false;
      std::set<std::string> interfaces;
      for (const auto& interface_name : interfaces_vec)
        interfaces.insert(interface_name);

      out->provided[clazz] = std::move(interfaces);
    }

    return data.ReadRequired(&out->required);
  }
};

}  // namespace mojo

#endif  // SERVICES_SHELL_PUBLIC_CPP_CAPABILITIES_STRUCT_TRAITS_H_
