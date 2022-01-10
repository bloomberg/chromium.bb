// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_FAKE_POSITION_CACHE_H_
#define SERVICES_DEVICE_GEOLOCATION_FAKE_POSITION_CACHE_H_

#include <utility>
#include <vector>

#include "services/device/geolocation/position_cache.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace device {

class FakePositionCache : public PositionCache {
 public:
  FakePositionCache();

  FakePositionCache(const FakePositionCache&) = delete;
  FakePositionCache& operator=(const FakePositionCache&) = delete;

  ~FakePositionCache() override;

  void CachePosition(const WifiData& wifi_data,
                     const mojom::Geoposition& position) override;
  const mojom::Geoposition* FindPosition(
      const WifiData& wifi_data) const override;
  size_t GetPositionCacheSize() const override;
  const mojom::Geoposition& GetLastUsedNetworkPosition() const override;
  void SetLastUsedNetworkPosition(const mojom::Geoposition& position) override;

 private:
  std::vector<std::pair<WifiData, mojom::Geoposition>> data;
  mojom::Geoposition last_used_position;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_FAKE_POSITION_CACHE_H_
