// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_config_service_mojo.h"

#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service.h"
#include "services/network/public/interfaces/proxy_config.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

// Test class for observing proxy config changes.
class TestProxyConfigServiceObserver
    : public net::ProxyConfigService::Observer {
 public:
  explicit TestProxyConfigServiceObserver(net::ProxyConfigService* service)
      : service_(service) {}
  ~TestProxyConfigServiceObserver() override {}

  void OnProxyConfigChanged(
      const net::ProxyConfig& config,
      net::ProxyConfigService::ConfigAvailability availability) override {
    // The ProxyConfigServiceMojo only sends on availability state.
    EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID, availability);

    observed_config_ = config;

    // The passed in config should match the one that GetLatestProxyConfig
    // returns.
    net::ProxyConfig retrieved_config;
    EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
              service_->GetLatestProxyConfig(&retrieved_config));
    EXPECT_TRUE(observed_config_.Equals(retrieved_config));
    ++config_changes_;
  }

  // Returns number of observed config changes since it was last called.
  int GetAndResetConfigChanges() {
    int result = config_changes_;
    config_changes_ = 0;
    return result;
  }

  // Returns last observed config.
  const net::ProxyConfig& observed_config() const { return observed_config_; }

 private:
  net::ProxyConfig observed_config_;

  net::ProxyConfigService* const service_;
  int config_changes_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestProxyConfigServiceObserver);
};

// Most tests of this class are in network_context_unittests.

// Makes sure that a ProxyConfigService::Observer is correctly notified of
// changes when the ProxyConfig changes, and is not informed of them in the case
// of "changes" that result in the same ProxyConfig as before.
TEST(ProxyConfigServiceMojoTest, ObserveProxyChanges) {
  base::test::ScopedTaskEnvironment scoped_task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);

  mojom::ProxyConfigClientPtr config_client;
  ProxyConfigServiceMojo proxy_config_service(
      mojo::MakeRequest(&config_client), base::Optional<net::ProxyConfig>(),
      nullptr);
  TestProxyConfigServiceObserver observer(&proxy_config_service);
  proxy_config_service.AddObserver(&observer);

  net::ProxyConfig proxy_config;
  // The service should start without a config.
  EXPECT_EQ(net::ProxyConfigService::CONFIG_PENDING,
            proxy_config_service.GetLatestProxyConfig(&proxy_config));

  net::ProxyConfig proxy_configs[3];
  proxy_configs[0].proxy_rules().ParseFromString("http=foopy:80");
  proxy_configs[1].proxy_rules().ParseFromString("http=foopy:80;ftp=foopy2");
  proxy_configs[2] = net::ProxyConfig::CreateDirect();

  for (const auto& proxy_config : proxy_configs) {
    // Set the proxy configuration to something that does not match the old one.
    config_client->OnProxyConfigUpdated(proxy_config);
    scoped_task_environment.RunUntilIdle();
    EXPECT_EQ(1, observer.GetAndResetConfigChanges());
    EXPECT_TRUE(proxy_config.Equals(observer.observed_config()));
    net::ProxyConfig retrieved_config;
    EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
              proxy_config_service.GetLatestProxyConfig(&retrieved_config));
    EXPECT_TRUE(proxy_config.Equals(retrieved_config));

    // Set the proxy configuration to the same value again. There should be not
    // be another proxy config changed notification.
    config_client->OnProxyConfigUpdated(proxy_config);
    scoped_task_environment.RunUntilIdle();
    EXPECT_EQ(0, observer.GetAndResetConfigChanges());
    EXPECT_TRUE(proxy_config.Equals(observer.observed_config()));
    EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID,
              proxy_config_service.GetLatestProxyConfig(&retrieved_config));
    EXPECT_TRUE(proxy_config.Equals(retrieved_config));
  }

  proxy_config_service.RemoveObserver(&observer);
}

}  // namespace

}  // namespace network
