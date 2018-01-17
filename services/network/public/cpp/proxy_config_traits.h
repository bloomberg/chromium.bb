// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_PROXY_CONFIG_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_PROXY_CONFIG_TRAITS_H_

#include "net/proxy/proxy_bypass_rules.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_source.h"
#include "net/proxy/proxy_list.h"
#include "services/network/public/interfaces/proxy_config.mojom-shared.h"
#include "url/mojo/url_gurl_struct_traits.h"

// This file handles the serialization of net::ProxyConfig.

namespace mojo {

template <>
struct StructTraits<network::mojom::ProxyBypassRulesDataView,
                    net::ProxyBypassRules> {
 public:
  static std::vector<std::string> rules(const net::ProxyBypassRules& r);
  static bool Read(network::mojom::ProxyBypassRulesDataView data,
                   net::ProxyBypassRules* out_proxy_bypass_rules);
};

template <>
struct StructTraits<network::mojom::ProxyListDataView, net::ProxyList> {
 public:
  static std::vector<std::string> proxies(const net::ProxyList& r);
  static bool Read(network::mojom::ProxyListDataView data,
                   net::ProxyList* out_proxy_list);
};

template <>
struct EnumTraits<network::mojom::ProxyRulesType,
                  net::ProxyConfig::ProxyRules::Type> {
 public:
  static network::mojom::ProxyRulesType ToMojom(
      net::ProxyConfig::ProxyRules::Type net_proxy_rules_type);
  static bool FromMojom(network::mojom::ProxyRulesType mojo_proxy_rules_type,
                        net::ProxyConfig::ProxyRules::Type* out);
};

template <>
struct StructTraits<network::mojom::ProxyRulesDataView,
                    net::ProxyConfig::ProxyRules> {
 public:
  static const net::ProxyBypassRules& bypass_rules(
      const net::ProxyConfig::ProxyRules& r) {
    return r.bypass_rules;
  }
  static bool reverse_bypass(const net::ProxyConfig::ProxyRules& r) {
    return r.reverse_bypass;
  }
  static net::ProxyConfig::ProxyRules::Type type(
      const net::ProxyConfig::ProxyRules& r) {
    return r.type;
  }
  static const net::ProxyList& single_proxies(
      const net::ProxyConfig::ProxyRules& r) {
    return r.single_proxies;
  }
  static const net::ProxyList& proxies_for_http(
      const net::ProxyConfig::ProxyRules& r) {
    return r.proxies_for_http;
  }
  static const net::ProxyList& proxies_for_https(
      const net::ProxyConfig::ProxyRules& r) {
    return r.proxies_for_https;
  }
  static const net::ProxyList& proxies_for_ftp(
      const net::ProxyConfig::ProxyRules& r) {
    return r.proxies_for_ftp;
  }
  static const net::ProxyList& fallback_proxies(
      const net::ProxyConfig::ProxyRules& r) {
    return r.fallback_proxies;
  }

  static bool Read(network::mojom::ProxyRulesDataView data,
                   net::ProxyConfig::ProxyRules* out_proxy_rules);
};

template <>
struct EnumTraits<network::mojom::ProxyConfigSource, net::ProxyConfigSource> {
 public:
  static network::mojom::ProxyConfigSource ToMojom(
      net::ProxyConfigSource net_proxy_config_source);
  static bool FromMojom(
      network::mojom::ProxyConfigSource mojo_proxy_config_source,
      net::ProxyConfigSource* out);
};

template <>
struct StructTraits<network::mojom::ProxyConfigDataView, net::ProxyConfig> {
 public:
  static bool auto_detect(const net::ProxyConfig& r) { return r.auto_detect(); }
  static const GURL& pac_url(const net::ProxyConfig& r) { return r.pac_url(); }
  static bool pac_mandatory(const net::ProxyConfig& r) {
    return r.pac_mandatory();
  }
  static const net::ProxyConfig::ProxyRules& proxy_rules(
      const net::ProxyConfig& r) {
    return r.proxy_rules();
  }
  static net::ProxyConfigSource source(const net::ProxyConfig& r) {
    return r.source();
  }
  static int32_t id(const net::ProxyConfig& r) { return r.id(); }
  static bool Read(network::mojom::ProxyConfigDataView data,
                   net::ProxyConfig* out_proxy_config);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_PROXY_CONFIG_TRAITS_H_
