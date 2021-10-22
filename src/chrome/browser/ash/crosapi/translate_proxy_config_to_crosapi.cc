// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/network_settings_translation.h"

#include "base/strings/string_split.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_server.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace {

std::vector<crosapi::mojom::ProxyLocationPtr> TranslateProxyLocations(
    const net::ProxyList& proxy_list) {
  std::vector<net::ProxyServer> proxies = proxy_list.GetAll();
  std::vector<crosapi::mojom::ProxyLocationPtr> proxy_ptr_list;
  for (const auto& proxy : proxies) {
    crosapi::mojom::ProxyLocationPtr proxy_ptr;
    proxy_ptr = crosapi::mojom::ProxyLocation::New();
    proxy_ptr->host = proxy.host_port_pair().host();
    proxy_ptr->port = proxy.host_port_pair().port();
    proxy_ptr_list.push_back(std::move(proxy_ptr));
  }
  return proxy_ptr_list;
}

crosapi::mojom::ProxySettingsManualPtr TranslateManualProxySettings(
    ProxyConfigDictionary* proxy_config) {
  crosapi::mojom::ProxySettingsManualPtr manual_proxy =
      crosapi::mojom::ProxySettingsManual::New();

  ProxyPrefs::ProxyMode mode;
  DCHECK(proxy_config->GetMode(&mode) &&
         mode == ProxyPrefs::MODE_FIXED_SERVERS);

  std::string proxy_servers;
  if (!proxy_config->GetProxyServer(&proxy_servers)) {
    LOG(ERROR) << "Missing manual proxy servers.";
    return nullptr;
  }

  net::ProxyConfig::ProxyRules rules;
  rules.ParseFromString(proxy_servers);

  switch (rules.type) {
    case net::ProxyConfig::ProxyRules::Type::EMPTY:
      return nullptr;
    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST:
      if (!rules.single_proxies.IsEmpty()) {
        manual_proxy->http_proxies =
            TranslateProxyLocations(rules.single_proxies);
        manual_proxy->secure_http_proxies =
            TranslateProxyLocations(rules.single_proxies);
        manual_proxy->socks_proxies =
            TranslateProxyLocations(rules.single_proxies);
      }
      break;
    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME:
      if (!rules.proxies_for_http.IsEmpty()) {
        manual_proxy->http_proxies =
            TranslateProxyLocations(rules.proxies_for_http);
      }
      if (!rules.proxies_for_https.IsEmpty()) {
        manual_proxy->secure_http_proxies =
            TranslateProxyLocations(rules.proxies_for_https);
      }
      if (!rules.fallback_proxies.IsEmpty()) {
        manual_proxy->socks_proxies =
            TranslateProxyLocations(rules.fallback_proxies);
      }
      break;
  }

  std::string bypass_list;
  if (proxy_config->GetBypassList(&bypass_list) && !bypass_list.empty()) {
    manual_proxy->exclude_domains = base::SplitString(
        bypass_list, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  }
  return manual_proxy;
}

}  // namespace

namespace crosapi {

// static
crosapi::mojom::ProxyConfigPtr ProxyConfigToCrosapiProxy(
    ProxyConfigDictionary* proxy_dict,
    GURL dhcp_wpad_url) {
  crosapi::mojom::ProxyConfigPtr proxy_config =
      crosapi::mojom::ProxyConfig::New();
  crosapi::mojom::ProxySettingsPtr proxy = crosapi::mojom::ProxySettings::New();
  crosapi::mojom::ProxySettingsDirectPtr direct =
      crosapi::mojom::ProxySettingsDirect::New();

  ProxyPrefs::ProxyMode mode;
  if (!proxy_dict || !proxy_dict->GetMode(&mode)) {
    proxy->set_direct(std::move(direct));
    proxy_config->proxy_settings = std::move(proxy);
    return proxy_config;
  }
  switch (mode) {
    case ProxyPrefs::MODE_DIRECT:
      proxy->set_direct(std::move(direct));
      break;
    case ProxyPrefs::MODE_AUTO_DETECT: {
      crosapi::mojom::ProxySettingsWpadPtr wpad =
          crosapi::mojom::ProxySettingsWpad::New();
      // WPAD with DHCP has a higher priority than DNS.
      if (dhcp_wpad_url.is_valid()) {
        wpad->pac_url = dhcp_wpad_url;
      } else {
        // Fallback to WPAD via DNS.
        wpad->pac_url = GURL("http://wpad/wpad.dat");
      }
      proxy->set_wpad(std::move(wpad));
      break;
    }
    case ProxyPrefs::MODE_PAC_SCRIPT: {
      std::string pac_url;
      if (!proxy_dict->GetPacUrl(&pac_url)) {
        proxy->set_direct(std::move(direct));
        LOG(ERROR) << "No pac URL for pac_script proxy mode.";
        break;
      }
      bool pac_mandatory = false;
      proxy_dict->GetPacMandatory(&pac_mandatory);

      crosapi::mojom::ProxySettingsPacPtr pac =
          crosapi::mojom::ProxySettingsPac::New();
      pac->pac_url = GURL(pac_url);
      pac->pac_mandatory = pac_mandatory;
      proxy->set_pac(std::move(pac));
      break;
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      crosapi::mojom::ProxySettingsManualPtr manual =
          TranslateManualProxySettings(proxy_dict);
      proxy->set_manual(std::move(manual));
      break;
    }
    case ProxyPrefs::MODE_SYSTEM:
      // This mode means Chrome is getting the settings from the operating
      // system. On Chrome OS, ash-chrome is the source of truth for proxy
      // settings so this mode is never used.
      NOTREACHED() << "The system mode doesn't apply to Ash-Chrome";
      break;
    default:
      LOG(ERROR) << "Incorrect proxy mode.";
      proxy->set_direct(std::move(direct));
  }

  proxy_config->proxy_settings = std::move(proxy);
  return proxy_config;
}

}  // namespace crosapi
