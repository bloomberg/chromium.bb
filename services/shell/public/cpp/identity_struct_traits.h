// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_IDENTITY_STRUCT_TRAITS_H_
#define SERVICES_SHELL_PUBLIC_CPP_IDENTITY_STRUCT_TRAITS_H_

#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/interfaces/connector.mojom.h"

namespace mojo {

template <>
struct StructTraits<shell::mojom::IdentityDataView, shell::Identity> {
  static const std::string& name(const shell::Identity& identity) {
    return identity.name();
  }
  static const std::string& user_id(const shell::Identity& identity) {
    return identity.user_id();
  }
  static const std::string& instance(const shell::Identity& identity) {
    return identity.instance();
  }
  static bool Read(shell::mojom::IdentityDataView data, shell::Identity* out) {
    std::string name, user_id, instance;
    if (!data.ReadName(&name))
      return false;

    if (!data.ReadUserId(&user_id))
      return false;

    if (!data.ReadInstance(&instance))
      return false;

    *out = shell::Identity(name, user_id, instance);
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_SHELL_PUBLIC_CPP_IDENTITY_STRUCT_TRAITS_H_
