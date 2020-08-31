// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_ADDRESS_LIST_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_ADDRESS_LIST_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/address_list.h"
#include "services/network/public/mojom/address_list.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::AddressListDataView, net::AddressList> {
  static const std::vector<net::IPEndPoint>& addresses(
      const net::AddressList& obj) {
    return obj.endpoints();
  }

  static const std::string& canonical_name(const net::AddressList& obj) {
    return obj.canonical_name();
  }

  static bool Read(network::mojom::AddressListDataView data,
                   net::AddressList* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_ADDRESS_LIST_MOJOM_TRAITS_H_
