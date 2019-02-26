// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_THROTTLE_MANAGER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_THROTTLE_MANAGER_H_

#include "components/data_reduction_proxy/core/common/data_reduction_proxy.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace data_reduction_proxy {

class DataReductionProxyServer;
struct DataReductionProxyTypeInfo;

// Helper that encapsulates the shared state between
// DataReductionProxyURLThrottles, whose main responsibility is keeping an up to
// date view of the config.
class DataReductionProxyThrottleManager
    : public mojom::DataReductionProxyThrottleConfigObserver {
 public:
  // Observes |data_reduction_proxy| for changes to the config, and starts off
  // with the initial value (possibly empty) |initial_config|.
  DataReductionProxyThrottleManager(
      mojom::DataReductionProxyPtr data_reduction_proxy,
      mojom::DataReductionProxyThrottleConfigPtr initial_config);

  ~DataReductionProxyThrottleManager() override;

  base::Optional<DataReductionProxyTypeInfo> FindConfiguredDataReductionProxy(
      const net::ProxyServer& proxy_server);

  void MarkProxiesAsBad(
      base::TimeDelta bypass_duration,
      const net::ProxyList& bad_proxies,
      mojom::DataReductionProxy::MarkProxiesAsBadCallback callback);

  std::unique_ptr<DataReductionProxyThrottleManager> Clone();

  // mojom::DataReductionProxyThrottleConfigObserver implementation.
  void OnThrottleConfigChanged(
      mojom::DataReductionProxyThrottleConfigPtr config) override;

  static mojom::DataReductionProxyThrottleConfigPtr CreateConfig(
      const std::vector<DataReductionProxyServer>& proxies_for_http);

 private:
  void SetDataReductionProxy(mojom::DataReductionProxyPtr data_reduction_proxy);

  mojom::DataReductionProxyPtr data_reduction_proxy_;

  // The last seen config values.
  std::vector<DataReductionProxyServer> proxies_for_http_;

  mojo::Binding<
      data_reduction_proxy::mojom::DataReductionProxyThrottleConfigObserver>
      binding_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyThrottleManager);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_THROTTLE_MANAGER_H_
