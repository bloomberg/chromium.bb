// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_HOST_RESOLVER_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_HOST_RESOLVER_MOJOM_TRAITS_H_

#include <string>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "mojo/public/cpp/bindings/array_traits.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/address_family.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/public/dns_config_overrides.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/dns/public/mdns_listener_update_type.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/dns/public/secure_dns_policy.h"
#include "services/network/public/mojom/host_resolver.mojom-forward.h"
#include "services/network/public/mojom/host_resolver.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {

// This is made visible for use by network::HostResolver. Not intended to be
// used elsewhere.
absl::optional<net::SecureDnsMode> FromOptionalSecureDnsMode(
    network::mojom::OptionalSecureDnsMode mode);

template <>
class StructTraits<network::mojom::DnsOverHttpsServerDataView,
                   net::DnsOverHttpsServerConfig> {
 public:
  static const std::string& server_template(
      const net::DnsOverHttpsServerConfig& server_config) {
    DCHECK(!server_config.server_template().empty());
    return server_config.server_template();
  }
  static bool Read(network::mojom::DnsOverHttpsServerDataView data,
                   net::DnsOverHttpsServerConfig* out_config);
};

template <>
struct StructTraits<network::mojom::DnsConfigOverridesDataView,
                    net::DnsConfigOverrides> {
  static const absl::optional<std::vector<net::IPEndPoint>>& nameservers(
      const net::DnsConfigOverrides& overrides) {
    return overrides.nameservers;
  }

  static const absl::optional<std::vector<std::string>>& search(
      const net::DnsConfigOverrides& overrides) {
    return overrides.search;
  }

  static network::mojom::DnsConfigOverrides_Tristate append_to_multi_label_name(
      const net::DnsConfigOverrides& overrides);

  static int ndots(const net::DnsConfigOverrides& overrides) {
    return overrides.ndots.value_or(-1);
  }

  static const absl::optional<base::TimeDelta>& fallback_period(
      const net::DnsConfigOverrides& overrides) {
    return overrides.fallback_period;
  }

  static int attempts(const net::DnsConfigOverrides& overrides) {
    return overrides.attempts.value_or(-1);
  }

  static network::mojom::DnsConfigOverrides_Tristate rotate(
      const net::DnsConfigOverrides& overrides);
  static network::mojom::DnsConfigOverrides_Tristate use_local_ipv6(
      const net::DnsConfigOverrides& overrides);

  static const absl::optional<std::vector<net::DnsOverHttpsServerConfig>>&
  dns_over_https_servers(const net::DnsConfigOverrides& overrides) {
    return overrides.dns_over_https_servers;
  }

  static network::mojom::OptionalSecureDnsMode secure_dns_mode(
      const net::DnsConfigOverrides& overrides);

  static network::mojom::DnsConfigOverrides_Tristate
  allow_dns_over_https_upgrade(const net::DnsConfigOverrides& overrides);

  static const absl::optional<std::vector<std::string>>&
  disabled_upgrade_providers(const net::DnsConfigOverrides& overrides) {
    return overrides.disabled_upgrade_providers;
  }

  static bool clear_hosts(const net::DnsConfigOverrides& overrides) {
    return overrides.clear_hosts;
  }

  static bool Read(network::mojom::DnsConfigOverridesDataView data,
                   net::DnsConfigOverrides* out);
};

template <>
struct EnumTraits<network::mojom::DnsQueryType, net::DnsQueryType> {
  static network::mojom::DnsQueryType ToMojom(net::DnsQueryType input);
  static bool FromMojom(network::mojom::DnsQueryType input,
                        net::DnsQueryType* output);
};

template <>
struct EnumTraits<network::mojom::ResolveHostParameters_Source,
                  net::HostResolverSource> {
  static network::mojom::ResolveHostParameters_Source ToMojom(
      net::HostResolverSource input);
  static bool FromMojom(network::mojom::ResolveHostParameters_Source input,
                        net::HostResolverSource* output);
};

template <>
struct EnumTraits<network::mojom::MdnsListenClient_UpdateType,
                  net::MdnsListenerUpdateType> {
  static network::mojom::MdnsListenClient_UpdateType ToMojom(
      net::MdnsListenerUpdateType input);
  static bool FromMojom(network::mojom::MdnsListenClient_UpdateType input,
                        net::MdnsListenerUpdateType* output);
};

template <>
struct EnumTraits<network::mojom::SecureDnsMode, net::SecureDnsMode> {
  static network::mojom::SecureDnsMode ToMojom(
      net::SecureDnsMode secure_dns_mode);
  static bool FromMojom(network::mojom::SecureDnsMode in,
                        net::SecureDnsMode* out);
};

template <>
struct EnumTraits<network::mojom::SecureDnsPolicy, net::SecureDnsPolicy> {
  static network::mojom::SecureDnsPolicy ToMojom(
      net::SecureDnsPolicy secure_dns_mode);
  static bool FromMojom(network::mojom::SecureDnsPolicy in,
                        net::SecureDnsPolicy* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_HOST_RESOLVER_MOJOM_TRAITS_H_
