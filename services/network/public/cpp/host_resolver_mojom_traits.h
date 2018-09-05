// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_HOST_RESOLVER_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_HOST_RESOLVER_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/dns/host_resolver.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

namespace mojo {

template <>
struct EnumTraits<network::mojom::ResolveHostParameters::DnsQueryType,
                  net::HostResolver::DnsQueryType> {
  static network::mojom::ResolveHostParameters::DnsQueryType ToMojom(
      net::HostResolver::DnsQueryType input);
  static bool FromMojom(
      network::mojom::ResolveHostParameters::DnsQueryType input,
      net::HostResolver::DnsQueryType* output);
};

template <>
struct EnumTraits<network::mojom::ResolveHostParameters::Source,
                  net::HostResolverSource> {
  static network::mojom::ResolveHostParameters::Source ToMojom(
      net::HostResolverSource input);
  static bool FromMojom(network::mojom::ResolveHostParameters::Source input,
                        net::HostResolverSource* output);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_HOST_RESOLVER_MOJOM_TRAITS_H_
