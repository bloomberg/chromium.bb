// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_CAPABILITIES_STRUCT_TRAITS_H_
#define SERVICES_SHELL_PUBLIC_CPP_CAPABILITIES_STRUCT_TRAITS_H_

#include "services/shell/public/cpp/capabilities.h"
#include "services/shell/public/interfaces/capabilities.mojom.h"

namespace mojo {

template <>
struct StructTraits<shell::mojom::CapabilitySpec::DataView,
                    shell::CapabilitySpec> {
  static const std::map<shell::Class, shell::Interfaces>& provided(
      const shell::CapabilitySpec& spec) {
    return spec.provided;
  }
  static const std::map<shell::Name, shell::Classes>& required(
      const shell::CapabilitySpec& spec) {
    return spec.required;
  }
  static bool Read(shell::mojom::CapabilitySpecDataView data,
                   shell::CapabilitySpec* out) {
    return data.ReadProvided(&out->provided) &&
           data.ReadRequired(&out->required);
  }
};

template <>
struct StructTraits<shell::mojom::Interfaces::DataView,
                    shell::Interfaces> {
  static std::vector<std::string> interfaces(const shell::Interfaces& spec) {
    std::vector<std::string> vec;
    for (const auto& interface_name : spec)
      vec.push_back(interface_name);
    return vec;
  }
  static bool Read(shell::mojom::InterfacesDataView data,
                   shell::Interfaces* out) {
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
struct StructTraits<shell::mojom::Classes::DataView,
                    shell::Classes> {
  static std::vector<std::string> classes(const shell::Classes& spec) {
    std::vector<std::string> vec;
    for (const auto& class_name : spec)
      vec.push_back(class_name);
    return vec;
  }
  static bool Read(shell::mojom::ClassesDataView data, shell::Classes* out) {
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

#endif  // SERVICES_SHELL_PUBLIC_CPP_CAPABILITIES_STRUCT_TRAITS_H_
