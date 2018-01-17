// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/proxy_config_traits.h"

#include "net/proxy/proxy_bypass_rules.h"
#include "net/proxy/proxy_config.h"
#include "services/network/public/interfaces/proxy_config.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {
namespace {

// Tests that serializing and then deserializing |original_config| to send it
// over Mojo results in a ProxyConfig that matches it.
bool TestProxyConfigRoundTrip(net::ProxyConfig& original_config) {
  net::ProxyConfig copied_config;
  EXPECT_TRUE(mojom::ProxyConfig::Deserialize(
      mojom::ProxyConfig::Serialize(&original_config), &copied_config));

  return original_config.Equals(copied_config) &&
         original_config.id() == copied_config.id() &&
         original_config.source() == copied_config.source() &&
         original_config.is_valid() == copied_config.is_valid();
}

TEST(ProxyConfigTraitsTest, AutoDetect) {
  net::ProxyConfig proxy_config = net::ProxyConfig::CreateAutoDetect();
  proxy_config.set_id(1);
  proxy_config.set_source(net::ProxyConfigSource::PROXY_CONFIG_SOURCE_KDE);
  EXPECT_TRUE(TestProxyConfigRoundTrip(proxy_config));
}

TEST(ProxyConfigTraitsTest, Direct) {
  net::ProxyConfig proxy_config = net::ProxyConfig::CreateDirect();
  proxy_config.set_id(2);
  proxy_config.set_source(
      net::ProxyConfigSource::PROXY_CONFIG_SOURCE_GSETTINGS);
  EXPECT_TRUE(TestProxyConfigRoundTrip(proxy_config));
}

TEST(ProxyConfigTraitsTest, CustomPacURL) {
  net::ProxyConfig proxy_config =
      net::ProxyConfig::CreateFromCustomPacURL(GURL("http://foo/"));
  EXPECT_TRUE(TestProxyConfigRoundTrip(proxy_config));
}

TEST(ProxyConfigTraitsTest, ProxyRules) {
  // Test cases copied directly from proxy_config_unittests, so should be
  // correctly formatted and have good coverage.
  const char* kTestCases[] = {
      "myproxy:80",
      "myproxy:80,https://myotherproxy",
      "http=myproxy:80",
      "ftp=ftp-proxy ; https=socks4://foopy",
      "foopy ; ftp=ftp-proxy",
      "ftp=ftp-proxy ; foopy",
      "ftp=ftp1,ftp2,ftp3",
      "http=http1,http2; http=http3",
      "ftp=ftp1,ftp2,ftp3 ; http=http1,http2; ",
      "http=https://secure_proxy; ftp=socks4://socks_proxy; "
      "https=socks://foo",
      "socks=foopy",
      "http=httpproxy ; https=httpsproxy ; ftp=ftpproxy ; socks=foopy ",
      "http=httpproxy ; https=httpsproxy ; socks=socks5://foopy ",
      "crazy=foopy ; foo=bar ; https=myhttpsproxy",
      "http=direct://,myhttpproxy; https=direct://",
      "http=myhttpproxy,direct://",
  };

  for (const char* test_case : kTestCases) {
    net::ProxyConfig proxy_config;
    proxy_config.proxy_rules().ParseFromString(test_case);
    EXPECT_TRUE(TestProxyConfigRoundTrip(proxy_config));
  }
}

TEST(ProxyConfigTraitsTest, BypassRules) {
  // These should cover every one of the rule types documented in
  // proxy_bypass_rules.h.
  const char* kTestCases[] = {
      ".foo.com", "*foo1.com:80, foo2.com", "*",
      "<local>",  "http://1.2.3.4:99",      "1.2.3.4/16",
  };

  for (const char* test_case : kTestCases) {
    net::ProxyConfig proxy_config;
    proxy_config.proxy_rules().ParseFromString("myproxy:80");
    proxy_config.proxy_rules().bypass_rules.ParseFromString(test_case);
    proxy_config.proxy_rules().reverse_bypass = false;
    // Make sure that the test case is properly formatted.
    EXPECT_GE(proxy_config.proxy_rules().bypass_rules.rules().size(), 1u);

    EXPECT_TRUE(TestProxyConfigRoundTrip(proxy_config));

    // Run the test again, except reversing the meaning of the bypass rules.
    proxy_config.proxy_rules().reverse_bypass = true;
    EXPECT_TRUE(TestProxyConfigRoundTrip(proxy_config));
  }
}

}  // namespace
}  // namespace network
