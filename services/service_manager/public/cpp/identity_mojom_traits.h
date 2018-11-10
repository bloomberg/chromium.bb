// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_IDENTITY_STRUCT_TRAITS_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_IDENTITY_STRUCT_TRAITS_H_

#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/mojom/connector.mojom.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(SERVICE_MANAGER_MOJOM)
    StructTraits<service_manager::mojom::IdentityDataView,
                 service_manager::Identity> {
  static const std::string& name(const service_manager::Identity& identity) {
    return identity.name();
  }
  static const base::Optional<base::Token>& instance_group(
      const service_manager::Identity& identity) {
    return identity.instance_group();
  }
  static const base::Optional<base::Token>& instance_id(
      const service_manager::Identity& identity) {
    return identity.instance_id();
  }
  static const base::Optional<base::Token>& globally_unique_id(
      const service_manager::Identity& identity) {
    return identity.globally_unique_id();
  }

  static bool Read(service_manager::mojom::IdentityDataView data,
                   service_manager::Identity* out) {
    std::string name;
    if (!data.ReadName(&name))
      return false;

    base::Optional<base::Token> instance_group;
    if (!data.ReadInstanceGroup(&instance_group))
      return false;

    base::Optional<base::Token> instance_id;
    if (!data.ReadInstanceId(&instance_id))
      return false;

    base::Optional<base::Token> globally_unique_id;
    if (!data.ReadGloballyUniqueId(&globally_unique_id))
      return false;

    *out = service_manager::Identity(name, instance_group, instance_id,
                                     globally_unique_id);
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_IDENTITY_STRUCT_TRAITS_H_
