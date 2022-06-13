// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/network_settings_translation.h"

#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "net/base/proxy_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kPacUrl[] = "http://pac.pac/";

base::Value GetPacProxyConfig(const std::string& pac_url, bool pac_mandatory) {
  return ProxyConfigDictionary::CreatePacScript(pac_url, pac_mandatory);
}

base::Value GetManualProxyConfig(const std::string& proxy_servers,
                                 const std::string& bypass_list) {
  return ProxyConfigDictionary::CreateFixedServers(proxy_servers, bypass_list);
}

}  // namespace

namespace crosapi {

TEST(NetworkSettingsTranslationTest, ProxyConfigToCrosapiProxyDirect) {
  ProxyConfigDictionary proxy_dict(ProxyConfigDictionary::CreateDirect());
  crosapi::mojom::ProxyConfigPtr actual =
      crosapi::ProxyConfigToCrosapiProxy(&proxy_dict,
                                         /*wpad_url=*/GURL(""));
  EXPECT_TRUE(actual->proxy_settings->is_direct());
}

TEST(NetworkSettingsTranslationTest, ProxyConfigToCrosapiProxyWpadNoUrl) {
  ProxyConfigDictionary proxy_dict(ProxyConfigDictionary::CreateAutoDetect());
  crosapi::mojom::ProxyConfigPtr actual =
      crosapi::ProxyConfigToCrosapiProxy(&proxy_dict,
                                         /*wpad_url=*/GURL(""));
  GURL default_wpad_url("http://wpad/wpad.dat");
  ASSERT_TRUE(actual->proxy_settings->is_wpad());
  EXPECT_EQ(actual->proxy_settings->get_wpad()->pac_url, default_wpad_url);
}

TEST(NetworkSettingsTranslationTest, ProxyConfigToCrosapiProxyWpadUrl) {
  GURL wpad_url(kPacUrl);
  ProxyConfigDictionary proxy_dict(ProxyConfigDictionary::CreateAutoDetect());

  crosapi::mojom::ProxyConfigPtr actual =
      crosapi::ProxyConfigToCrosapiProxy(&proxy_dict, wpad_url);
  ASSERT_TRUE(actual->proxy_settings->is_wpad());
  EXPECT_EQ(actual->proxy_settings->get_wpad()->pac_url, wpad_url);
}

TEST(NetworkSettingsTranslationTest, ProxyConfigToCrosapiProxyPacMandatory) {
  ProxyConfigDictionary proxy_dict(
      GetPacProxyConfig(kPacUrl, /*pac_mandatory=*/true));
  crosapi::mojom::ProxyConfigPtr actual =
      crosapi::ProxyConfigToCrosapiProxy(&proxy_dict,
                                         /*wpad_url=*/GURL(""));

  ASSERT_TRUE(actual->proxy_settings->is_pac());
  EXPECT_EQ(actual->proxy_settings->get_pac()->pac_url, GURL(kPacUrl));
  EXPECT_EQ(actual->proxy_settings->get_pac()->pac_mandatory, true);
}

TEST(NetworkSettingsTranslationTest, ProxyConfigToCrosapiProxyPacNotMandatory) {
  ProxyConfigDictionary proxy_dict(
      GetPacProxyConfig(kPacUrl, /*pac_mandatory=*/false));
  crosapi::mojom::ProxyConfigPtr actual =
      crosapi::ProxyConfigToCrosapiProxy(&proxy_dict,
                                         /*wpad_url=*/GURL(""));

  ASSERT_TRUE(actual->proxy_settings->is_pac());
  EXPECT_EQ(actual->proxy_settings->get_pac()->pac_url, GURL(kPacUrl));
  EXPECT_EQ(actual->proxy_settings->get_pac()->pac_mandatory, false);
}

TEST(NetworkSettingsTranslationTest, ProxyConfigToCrosapiProxyManual) {
  std::string proxy_servers =
      "http=proxy:80;http=proxy2:80;https=secure_proxy:81;socks=socks_proxy:"
      "82;";
  std::string bypass_list = "localhost;google.com;";
  ProxyConfigDictionary proxy_dict(
      GetManualProxyConfig(proxy_servers, bypass_list));
  crosapi::mojom::ProxyConfigPtr actual =
      crosapi::ProxyConfigToCrosapiProxy(&proxy_dict,
                                         /*wpad_url=*/GURL(""));
  ASSERT_TRUE(actual->proxy_settings->is_manual());

  std::vector<crosapi::mojom::ProxyLocationPtr> proxy_ptr =
      std::move(actual->proxy_settings->get_manual()->http_proxies);
  ASSERT_EQ(proxy_ptr.size(), 2u);
  EXPECT_EQ(proxy_ptr[0]->host, "proxy");
  EXPECT_EQ(proxy_ptr[0]->port, 80);
  EXPECT_EQ(proxy_ptr[1]->host, "proxy2");
  EXPECT_EQ(proxy_ptr[1]->port, 80);
  proxy_ptr =
      std::move(actual->proxy_settings->get_manual()->secure_http_proxies);
  ASSERT_EQ(proxy_ptr.size(), 1u);
  EXPECT_EQ(proxy_ptr[0]->host, "secure_proxy");
  EXPECT_EQ(proxy_ptr[0]->port, 81);

  proxy_ptr = std::move(actual->proxy_settings->get_manual()->socks_proxies);
  ASSERT_EQ(proxy_ptr.size(), 1u);
  EXPECT_EQ(proxy_ptr[0]->host, "socks_proxy");
  EXPECT_EQ(proxy_ptr[0]->port, 82);

  const std::vector<std::string> exclude_domains =
      actual->proxy_settings->get_manual()->exclude_domains;
  ASSERT_EQ(exclude_domains.size(), 2u);
  EXPECT_EQ(exclude_domains[0], "localhost");
  EXPECT_EQ(exclude_domains[1], "google.com");
}

TEST(NetworkSettingsTranslationTest, CrosapiProxyToProxyConfigDirect) {
  crosapi::mojom::ProxyConfigPtr ptr = crosapi::mojom::ProxyConfig::New();
  crosapi::mojom::ProxySettingsPtr proxy = crosapi::mojom::ProxySettings::New();
  crosapi::mojom::ProxySettingsDirectPtr direct =
      crosapi::mojom::ProxySettingsDirect::New();
  proxy->set_direct(std::move(direct));
  ptr->proxy_settings = std::move(proxy);

  EXPECT_EQ(CrosapiProxyToProxyConfig(std::move(ptr)).GetDictionary(),
            ProxyConfigDictionary::CreateDirect());
}

TEST(NetworkSettingsTranslationTest, CrosapiProxyToProxyConfigWpad) {
  crosapi::mojom::ProxyConfigPtr ptr = crosapi::mojom::ProxyConfig::New();
  crosapi::mojom::ProxySettingsPtr proxy = crosapi::mojom::ProxySettings::New();
  crosapi::mojom::ProxySettingsWpadPtr wpad =
      crosapi::mojom::ProxySettingsWpad::New();
  wpad->pac_url = GURL("pac.pac");
  proxy->set_wpad(std::move(wpad));
  ptr->proxy_settings = std::move(proxy);

  EXPECT_EQ(CrosapiProxyToProxyConfig(std::move(ptr)).GetDictionary(),
            ProxyConfigDictionary::CreateAutoDetect());
}

TEST(NetworkSettingsTranslationTest, CrosapiProxyToProxyConfigPac) {
  crosapi::mojom::ProxyConfigPtr ptr = crosapi::mojom::ProxyConfig::New();
  crosapi::mojom::ProxySettingsPtr proxy = crosapi::mojom::ProxySettings::New();
  crosapi::mojom::ProxySettingsPacPtr pac =
      crosapi::mojom::ProxySettingsPac::New();
  pac->pac_url = GURL(kPacUrl);
  pac->pac_mandatory = true;
  proxy->set_pac(pac.Clone());
  ptr->proxy_settings = proxy.Clone();
  EXPECT_EQ(CrosapiProxyToProxyConfig(ptr.Clone()).GetDictionary(),
            GetPacProxyConfig(kPacUrl, true));

  pac->pac_mandatory = false;
  proxy->set_pac(pac.Clone());
  ptr->proxy_settings = std::move(proxy);
  EXPECT_EQ(CrosapiProxyToProxyConfig(std::move(ptr)).GetDictionary(),
            GetPacProxyConfig(kPacUrl, false));
}

TEST(NetworkSettingsTranslationTest, CrosapiProxyToProxyConfigManual) {
  crosapi::mojom::ProxyConfigPtr ptr = crosapi::mojom::ProxyConfig::New();
  crosapi::mojom::ProxySettingsPtr proxy = crosapi::mojom::ProxySettings::New();
  crosapi::mojom::ProxySettingsManualPtr manual =
      crosapi::mojom::ProxySettingsManual::New();
  crosapi::mojom::ProxyLocationPtr location =
      crosapi::mojom::ProxyLocation::New();
  location->host = "proxy1";
  location->port = 80;
  manual->http_proxies.push_back(location.Clone());
  location->host = "proxy2";
  location->port = 80;
  manual->http_proxies.push_back(location.Clone());
  location->host = "secure_proxy";
  location->port = 81;
  manual->secure_http_proxies.push_back(location.Clone());
  location->host = "socks_proxy";
  location->port = 82;
  manual->socks_proxies.push_back(std::move(location));
  manual->exclude_domains = {"localhost", "google.com"};
  proxy->set_manual(std::move(manual));
  ptr->proxy_settings = std::move(proxy);
  EXPECT_EQ(CrosapiProxyToProxyConfig(std::move(ptr)).GetDictionary(),
            GetManualProxyConfig("http=proxy1:80;http=proxy2:80;https=secure_"
                                 "proxy:81;socks=socks_proxy:82",
                                 /*bypass_list=*/"localhost;google.com"));
}

}  // namespace crosapi
