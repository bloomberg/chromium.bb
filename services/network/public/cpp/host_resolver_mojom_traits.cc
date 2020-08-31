// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/host_resolver_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "services/network/public/cpp/ip_address_mojom_traits.h"
#include "services/network/public/cpp/ip_endpoint_mojom_traits.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

namespace mojo {

using network::mojom::DnsConfigOverrides;
using network::mojom::DnsConfigOverridesDataView;
using network::mojom::DnsHost;
using network::mojom::DnsHostDataView;
using network::mojom::DnsHostPtr;
using network::mojom::DnsOverHttpsServer;
using network::mojom::DnsOverHttpsServerDataView;
using network::mojom::DnsOverHttpsServerPtr;
using network::mojom::DnsQueryType;
using network::mojom::MdnsListenClient;
using network::mojom::OptionalSecureDnsMode;
using network::mojom::ResolveHostParameters;

namespace {

DnsConfigOverrides::Tristate ToTristate(base::Optional<bool> optional) {
  if (!optional)
    return DnsConfigOverrides::Tristate::NO_OVERRIDE;
  if (optional.value())
    return DnsConfigOverrides::Tristate::TRISTATE_TRUE;
  return DnsConfigOverrides::Tristate::TRISTATE_FALSE;
}

base::Optional<bool> FromTristate(DnsConfigOverrides::Tristate tristate) {
  switch (tristate) {
    case DnsConfigOverrides::Tristate::NO_OVERRIDE:
      return base::nullopt;
    case DnsConfigOverrides::Tristate::TRISTATE_TRUE:
      return true;
    case DnsConfigOverrides::Tristate::TRISTATE_FALSE:
      return false;
  }
}

bool ReadHostData(mojo::ArrayDataView<DnsHostDataView> data,
                  base::Optional<net::DnsHosts>* out) {
  if (data.is_null()) {
    out->reset();
    return true;
  }

  out->emplace();
  for (size_t i = 0; i < data.size(); ++i) {
    DnsHostDataView host_data;
    data.GetDataView(i, &host_data);

    std::string hostname;
    if (!host_data.ReadHostname(&hostname))
      return false;

    net::IPAddress address;
    if (!host_data.ReadAddress(&address) || !address.IsValid())
      return false;

    net::AddressFamily address_family;
    if (address.IsIPv4()) {
      address_family = net::ADDRESS_FAMILY_IPV4;
    } else if (address.IsIPv6()) {
      address_family = net::ADDRESS_FAMILY_IPV6;
    } else {
      return false;
    }

    net::DnsHostsKey key = std::make_pair(std::move(hostname), address_family);
    if (out->value().find(key) != out->value().end()) {
      // Each DnsHostsKey expected to be unique.
      return false;
    }
    out->value()[std::move(key)] = std::move(address);
  }

  return true;
}

bool ReadDnsOverHttpsServerData(
    mojo::ArrayDataView<DnsOverHttpsServerDataView> data,
    base::Optional<std::vector<net::DnsOverHttpsServerConfig>>* out) {
  if (data.is_null()) {
    out->reset();
    return true;
  }

  out->emplace();
  for (size_t i = 0; i < data.size(); ++i) {
    DnsOverHttpsServerDataView server_data;
    data.GetDataView(i, &server_data);

    std::string server_template;
    if (!server_data.ReadServerTemplate(&server_template))
      return false;

    out->value().emplace_back(std::move(server_template),
                              server_data.use_post());
  }

  return true;
}

OptionalSecureDnsMode ToOptionalSecureDnsMode(
    base::Optional<net::DnsConfig::SecureDnsMode> optional) {
  if (!optional)
    return OptionalSecureDnsMode::NO_OVERRIDE;
  switch (optional.value()) {
    case net::DnsConfig::SecureDnsMode::OFF:
      return OptionalSecureDnsMode::OFF;
    case net::DnsConfig::SecureDnsMode::AUTOMATIC:
      return OptionalSecureDnsMode::AUTOMATIC;
    case net::DnsConfig::SecureDnsMode::SECURE:
      return OptionalSecureDnsMode::SECURE;
  }
}

}  // namespace

base::Optional<net::DnsConfig::SecureDnsMode> FromOptionalSecureDnsMode(
    OptionalSecureDnsMode mode) {
  switch (mode) {
    case OptionalSecureDnsMode::NO_OVERRIDE:
      return base::nullopt;
    case OptionalSecureDnsMode::OFF:
      return net::DnsConfig::SecureDnsMode::OFF;
    case OptionalSecureDnsMode::AUTOMATIC:
      return net::DnsConfig::SecureDnsMode::AUTOMATIC;
    case OptionalSecureDnsMode::SECURE:
      return net::DnsConfig::SecureDnsMode::SECURE;
  }
}

// static
base::Optional<std::vector<DnsHostPtr>>
StructTraits<DnsConfigOverridesDataView, net::DnsConfigOverrides>::hosts(
    const net::DnsConfigOverrides& overrides) {
  if (!overrides.hosts)
    return base::nullopt;

  std::vector<DnsHostPtr> out_hosts;
  for (const net::DnsHosts::value_type& host : overrides.hosts.value()) {
    out_hosts.push_back(DnsHost::New(host.first.first, host.second));
  }

  return base::make_optional(std::move(out_hosts));
}

// static
DnsConfigOverrides::Tristate
StructTraits<DnsConfigOverridesDataView, net::DnsConfigOverrides>::
    append_to_multi_label_name(const net::DnsConfigOverrides& overrides) {
  return ToTristate(overrides.append_to_multi_label_name);
}

// static
DnsConfigOverrides::Tristate
StructTraits<DnsConfigOverridesDataView, net::DnsConfigOverrides>::
    randomize_ports(const net::DnsConfigOverrides& overrides) {
  return ToTristate(overrides.randomize_ports);
}

// static
DnsConfigOverrides::Tristate
StructTraits<DnsConfigOverridesDataView, net::DnsConfigOverrides>::rotate(
    const net::DnsConfigOverrides& overrides) {
  return ToTristate(overrides.rotate);
}

// static
DnsConfigOverrides::Tristate
StructTraits<DnsConfigOverridesDataView, net::DnsConfigOverrides>::
    use_local_ipv6(const net::DnsConfigOverrides& overrides) {
  return ToTristate(overrides.use_local_ipv6);
}

// static
base::Optional<std::vector<DnsOverHttpsServerPtr>>
StructTraits<DnsConfigOverridesDataView, net::DnsConfigOverrides>::
    dns_over_https_servers(const net::DnsConfigOverrides& overrides) {
  if (!overrides.dns_over_https_servers)
    return base::nullopt;

  std::vector<DnsOverHttpsServerPtr> out_servers;
  for (net::DnsOverHttpsServerConfig server :
       overrides.dns_over_https_servers.value()) {
    out_servers.push_back(
        DnsOverHttpsServer::New(server.server_template, server.use_post));
  }

  return base::make_optional(std::move(out_servers));
}

// static
OptionalSecureDnsMode
StructTraits<DnsConfigOverridesDataView, net::DnsConfigOverrides>::
    secure_dns_mode(const net::DnsConfigOverrides& overrides) {
  return ToOptionalSecureDnsMode(overrides.secure_dns_mode);
}

// static
DnsConfigOverrides::Tristate
StructTraits<DnsConfigOverridesDataView, net::DnsConfigOverrides>::
    allow_dns_over_https_upgrade(const net::DnsConfigOverrides& overrides) {
  return ToTristate(overrides.allow_dns_over_https_upgrade);
}

// static
bool StructTraits<DnsConfigOverridesDataView, net::DnsConfigOverrides>::Read(
    DnsConfigOverridesDataView data,
    net::DnsConfigOverrides* out) {
  if (!data.ReadNameservers(&out->nameservers))
    return false;
  if (!data.ReadSearch(&out->search))
    return false;

  mojo::ArrayDataView<DnsHostDataView> hosts_data;
  data.GetHostsDataView(&hosts_data);
  if (!ReadHostData(hosts_data, &out->hosts))
    return false;

  out->append_to_multi_label_name =
      FromTristate(data.append_to_multi_label_name());
  out->randomize_ports = FromTristate(data.randomize_ports());

  if (data.ndots() < -1)
    return false;
  if (data.ndots() >= 0)
    out->ndots = data.ndots();
  // if == -1, leave nullopt.

  if (!data.ReadTimeout(&out->timeout))
    return false;

  if (data.attempts() < -1)
    return false;
  if (data.attempts() >= 0)
    out->attempts = data.attempts();
  // if == -1, leave nullopt.

  out->rotate = FromTristate(data.rotate());
  out->use_local_ipv6 = FromTristate(data.use_local_ipv6());

  mojo::ArrayDataView<DnsOverHttpsServerDataView> dns_over_https_servers_data;
  data.GetDnsOverHttpsServersDataView(&dns_over_https_servers_data);
  if (!ReadDnsOverHttpsServerData(dns_over_https_servers_data,
                                  &out->dns_over_https_servers)) {
    return false;
  }

  out->secure_dns_mode = FromOptionalSecureDnsMode(data.secure_dns_mode());

  out->allow_dns_over_https_upgrade =
      FromTristate(data.allow_dns_over_https_upgrade());
  if (!data.ReadDisabledUpgradeProviders(&out->disabled_upgrade_providers))
    return false;

  return true;
}

// static
DnsQueryType EnumTraits<DnsQueryType, net::DnsQueryType>::ToMojom(
    net::DnsQueryType input) {
  switch (input) {
    case net::DnsQueryType::UNSPECIFIED:
      return DnsQueryType::UNSPECIFIED;
    case net::DnsQueryType::A:
      return DnsQueryType::A;
    case net::DnsQueryType::AAAA:
      return DnsQueryType::AAAA;
    case net::DnsQueryType::TXT:
      return DnsQueryType::TXT;
    case net::DnsQueryType::PTR:
      return DnsQueryType::PTR;
    case net::DnsQueryType::SRV:
      return DnsQueryType::SRV;
    case net::DnsQueryType::ESNI:
      NOTIMPLEMENTED();
      return DnsQueryType::UNSPECIFIED;
  }
}

// static
bool EnumTraits<DnsQueryType, net::DnsQueryType>::FromMojom(
    DnsQueryType input,
    net::DnsQueryType* output) {
  switch (input) {
    case DnsQueryType::UNSPECIFIED:
      *output = net::DnsQueryType::UNSPECIFIED;
      return true;
    case DnsQueryType::A:
      *output = net::DnsQueryType::A;
      return true;
    case DnsQueryType::AAAA:
      *output = net::DnsQueryType::AAAA;
      return true;
    case DnsQueryType::TXT:
      *output = net::DnsQueryType::TXT;
      return true;
    case DnsQueryType::PTR:
      *output = net::DnsQueryType::PTR;
      return true;
    case DnsQueryType::SRV:
      *output = net::DnsQueryType::SRV;
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
    case net::HostResolverSource::LOCAL_ONLY:
      return ResolveHostParameters::Source::LOCAL_ONLY;
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
    case ResolveHostParameters::Source::LOCAL_ONLY:
      *output = net::HostResolverSource::LOCAL_ONLY;
      return true;
  }
}

// static
MdnsListenClient::UpdateType
EnumTraits<MdnsListenClient::UpdateType,
           net::HostResolver::MdnsListener::Delegate::UpdateType>::
    ToMojom(net::HostResolver::MdnsListener::Delegate::UpdateType input) {
  switch (input) {
    case net::HostResolver::MdnsListener::Delegate::UpdateType::ADDED:
      return MdnsListenClient::UpdateType::ADDED;
    case net::HostResolver::MdnsListener::Delegate::UpdateType::CHANGED:
      return MdnsListenClient::UpdateType::CHANGED;
    case net::HostResolver::MdnsListener::Delegate::UpdateType::REMOVED:
      return MdnsListenClient::UpdateType::REMOVED;
  }
}

// static
bool EnumTraits<MdnsListenClient::UpdateType,
                net::HostResolver::MdnsListener::Delegate::UpdateType>::
    FromMojom(MdnsListenClient::UpdateType input,
              net::HostResolver::MdnsListener::Delegate::UpdateType* output) {
  switch (input) {
    case MdnsListenClient::UpdateType::ADDED:
      *output = net::HostResolver::MdnsListener::Delegate::UpdateType::ADDED;
      return true;
    case MdnsListenClient::UpdateType::CHANGED:
      *output = net::HostResolver::MdnsListener::Delegate::UpdateType::CHANGED;
      return true;
    case MdnsListenClient::UpdateType::REMOVED:
      *output = net::HostResolver::MdnsListener::Delegate::UpdateType::REMOVED;
      return true;
  }
}

// static
network::mojom::SecureDnsMode
EnumTraits<network::mojom::SecureDnsMode, net::DnsConfig::SecureDnsMode>::
    ToMojom(net::DnsConfig::SecureDnsMode secure_dns_mode) {
  switch (secure_dns_mode) {
    case net::DnsConfig::SecureDnsMode::OFF:
      return network::mojom::SecureDnsMode::OFF;
    case net::DnsConfig::SecureDnsMode::AUTOMATIC:
      return network::mojom::SecureDnsMode::AUTOMATIC;
    case net::DnsConfig::SecureDnsMode::SECURE:
      return network::mojom::SecureDnsMode::SECURE;
  }
  NOTREACHED();
  return network::mojom::SecureDnsMode::OFF;
}

// static
bool EnumTraits<network::mojom::SecureDnsMode, net::DnsConfig::SecureDnsMode>::
    FromMojom(network::mojom::SecureDnsMode in,
              net::DnsConfig::SecureDnsMode* out) {
  switch (in) {
    case network::mojom::SecureDnsMode::OFF:
      *out = net::DnsConfig::SecureDnsMode::OFF;
      return true;
    case network::mojom::SecureDnsMode::AUTOMATIC:
      *out = net::DnsConfig::SecureDnsMode::AUTOMATIC;
      return true;
    case network::mojom::SecureDnsMode::SECURE:
      *out = net::DnsConfig::SecureDnsMode::SECURE;
      return true;
  }
  return false;
}

// static
bool StructTraits<
    network::mojom::ResolveErrorInfoDataView,
    net::ResolveErrorInfo>::Read(network::mojom::ResolveErrorInfoDataView data,
                                 net::ResolveErrorInfo* out) {
  // There should not be a secure network error if the error code indicates no
  // error.
  if (data.error() == net::OK && data.is_secure_network_error())
    return false;
  *out = net::ResolveErrorInfo(data.error(), data.is_secure_network_error());
  return true;
}

}  // namespace mojo
