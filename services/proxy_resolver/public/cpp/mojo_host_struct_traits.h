// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_PUBLIC_CPP_MOJO_HOST_STRUCT_TRAITS_H_
#define SERVICES_PROXY_RESOLVER_PUBLIC_CPP_MOJO_HOST_STRUCT_TRAITS_H_

#include "base/strings/string_piece.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/dns/host_resolver.h"
#include "services/network/public/mojom/address_family.mojom.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"

namespace mojo {

template <>
struct StructTraits<proxy_resolver::mojom::HostResolverRequestInfoDataView,
                    std::unique_ptr<net::HostResolver::RequestInfo>> {
  static base::StringPiece host(
      const std::unique_ptr<net::HostResolver::RequestInfo>& obj) {
    return obj->hostname();
  }

  static uint16_t port(
      const std::unique_ptr<net::HostResolver::RequestInfo>& obj) {
    return obj->port();
  }

  static net::AddressFamily address_family(
      const std::unique_ptr<net::HostResolver::RequestInfo>& obj) {
    return obj->address_family();
  }

  static bool is_my_ip_address(
      const std::unique_ptr<net::HostResolver::RequestInfo>& obj) {
    return obj->is_my_ip_address();
  }

  static bool Read(proxy_resolver::mojom::HostResolverRequestInfoDataView obj,
                   std::unique_ptr<net::HostResolver::RequestInfo>* output);
};

}  // namespace mojo

#endif  // SERVICES_PROXY_RESOLVER_PUBLIC_CPP_MOJO_HOST_STRUCT_TRAITS_H_
