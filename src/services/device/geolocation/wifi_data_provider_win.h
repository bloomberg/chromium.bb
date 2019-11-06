// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_WIN_H_
#define SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_WIN_H_

#include "base/macros.h"
#include "services/device/geolocation/wifi_data_provider_common.h"

namespace device {

class WifiDataProviderWin : public WifiDataProviderCommon {
 public:
  WifiDataProviderWin();

 private:
  ~WifiDataProviderWin() override;

  // WifiDataProviderCommon implementation
  std::unique_ptr<WlanApiInterface> CreateWlanApi() override;
  std::unique_ptr<WifiPollingPolicy> CreatePollingPolicy() override;

  DISALLOW_COPY_AND_ASSIGN(WifiDataProviderWin);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_WIN_H_
