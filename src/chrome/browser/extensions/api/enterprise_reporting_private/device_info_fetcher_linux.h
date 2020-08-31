// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_DEVICE_INFO_FETCHER_LINUX_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_DEVICE_INFO_FETCHER_LINUX_H_

#include "chrome/browser/extensions/api/enterprise_reporting_private/device_info_fetcher.h"

namespace extensions {
namespace enterprise_reporting {

using DeviceInfo = ::extensions::api::enterprise_reporting_private::DeviceInfo;

// Linux implementation of DeviceInfoFetcher.
class DeviceInfoFetcherLinux : public DeviceInfoFetcher {
 public:
  DeviceInfoFetcherLinux();
  DeviceInfoFetcherLinux(const DeviceInfoFetcherLinux&) = delete;
  DeviceInfoFetcherLinux& operator=(const DeviceInfoFetcherLinux&) = delete;
  ~DeviceInfoFetcherLinux() override;

  // Overrides DeviceInfoFetcher:
  DeviceInfo Fetch() override;
};

}  // namespace enterprise_reporting
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_DEVICE_INFO_FETCHER_LINUX_H_
