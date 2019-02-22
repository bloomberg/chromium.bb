// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/host_resolver_mojom_traits.h"

namespace mojo {

using network::mojom::ResolveHostParameters;

// static
ResolveHostParameters::DnsQueryType EnumTraits<
    ResolveHostParameters::DnsQueryType,
    net::HostResolver::DnsQueryType>::ToMojom(net::HostResolver::DnsQueryType
                                                  input) {
  switch (input) {
    case net::HostResolver::DnsQueryType::UNSPECIFIED:
      return ResolveHostParameters::DnsQueryType::UNSPECIFIED;
    case net::HostResolver::DnsQueryType::A:
      return ResolveHostParameters::DnsQueryType::A;
    case net::HostResolver::DnsQueryType::AAAA:
      return ResolveHostParameters::DnsQueryType::AAAA;
  }
}

// static
bool EnumTraits<ResolveHostParameters::DnsQueryType,
                net::HostResolver::DnsQueryType>::
    FromMojom(ResolveHostParameters::DnsQueryType input,
              net::HostResolver::DnsQueryType* output) {
  switch (input) {
    case ResolveHostParameters::DnsQueryType::UNSPECIFIED:
      *output = net::HostResolver::DnsQueryType::UNSPECIFIED;
      return true;
    case ResolveHostParameters::DnsQueryType::A:
      *output = net::HostResolver::DnsQueryType::A;
      return true;
    case ResolveHostParameters::DnsQueryType::AAAA:
      *output = net::HostResolver::DnsQueryType::AAAA;
      return true;
  }
}

// static
ResolveHostParameters::Source
EnumTraits<ResolveHostParameters::Source, net::HostResolverSource>::ToMojom(
    net::HostResolverSource input) {
  switch (input) {
    case net::HostResolverSource::ANY:
      return ResolveHostParameters::Source::ANY;
    case net::HostResolverSource::SYSTEM:
      return ResolveHostParameters::Source::SYSTEM;
    case net::HostResolverSource::DNS:
      return ResolveHostParameters::Source::DNS;
    case net::HostResolverSource::MULTICAST_DNS:
      return ResolveHostParameters::Source::MULTICAST_DNS;
  }
}

// static
bool EnumTraits<ResolveHostParameters::Source, net::HostResolverSource>::
    FromMojom(ResolveHostParameters::Source input,
              net::HostResolverSource* output) {
  switch (input) {
    case ResolveHostParameters::Source::ANY:
      *output = net::HostResolverSource::ANY;
      return true;
    case ResolveHostParameters::Source::SYSTEM:
      *output = net::HostResolverSource::SYSTEM;
      return true;
    case ResolveHostParameters::Source::DNS:
      *output = net::HostResolverSource::DNS;
      return true;
    case ResolveHostParameters::Source::MULTICAST_DNS:
      *output = net::HostResolverSource::MULTICAST_DNS;
      return true;
  }
}

}  // namespace mojo
