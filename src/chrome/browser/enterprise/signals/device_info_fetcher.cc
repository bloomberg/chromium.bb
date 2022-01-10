// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/device_info_fetcher.h"

#include "build/build_config.h"

#if defined(OS_MAC)
#include "chrome/browser/enterprise/signals/device_info_fetcher_mac.h"
#elif defined(OS_WIN)
#include "chrome/browser/enterprise/signals/device_info_fetcher_win.h"
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "chrome/browser/enterprise/signals/device_info_fetcher_linux.h"
#endif

namespace enterprise_signals {

namespace {

// When true, will force DeviceInfoFetcher::CreateInstance to return a stubbed
// instance. Used for testing.
bool force_stub_for_testing = false;

// Stub implementation of DeviceInfoFetcher.
class StubDeviceFetcher : public DeviceInfoFetcher {
 public:
  StubDeviceFetcher() = default;
  ~StubDeviceFetcher() override = default;

  StubDeviceFetcher(const StubDeviceFetcher&) = delete;
  StubDeviceFetcher& operator=(const StubDeviceFetcher&) = delete;

  DeviceInfo Fetch() override {
    DeviceInfo device_info;
    device_info.os_name = "stubOS";
    device_info.os_version = "0.0.0.0";
    device_info.security_patch_level = "security patch level";
    device_info.device_host_name = "midnightshift";
    device_info.device_model = "topshot";
    device_info.serial_number = "twirlchange";
    device_info.screen_lock_secured = SettingValue::ENABLED;
    device_info.disk_encrypted = SettingValue::DISABLED;
    device_info.mac_addresses.push_back("00:00:00:00:00:00");
    device_info.windows_machine_domain = "MACHINE_DOMAIN";
    device_info.windows_user_domain = "USER_DOMAIN";
    return device_info;
  }
};

}  // namespace

DeviceInfo::DeviceInfo() = default;
DeviceInfo::~DeviceInfo() = default;
DeviceInfo::DeviceInfo(const DeviceInfo&) = default;
DeviceInfo::DeviceInfo(DeviceInfo&&) = default;

DeviceInfoFetcher::DeviceInfoFetcher() = default;
DeviceInfoFetcher::~DeviceInfoFetcher() = default;

// static
std::unique_ptr<DeviceInfoFetcher> DeviceInfoFetcher::CreateInstance() {
  if (force_stub_for_testing) {
    return std::make_unique<StubDeviceFetcher>();
  }

// TODO(pastarmovj): Instead of the if-defs implement the CreateInstance
// function in the platform specific classes.
#if defined(OS_MAC)
  return std::make_unique<DeviceInfoFetcherMac>();
#elif defined(OS_WIN)
  return std::make_unique<DeviceInfoFetcherWin>();
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
  return std::make_unique<DeviceInfoFetcherLinux>();
#else
  return std::make_unique<StubDeviceFetcher>();
#endif
}

// static
std::unique_ptr<DeviceInfoFetcher>
DeviceInfoFetcher::CreateStubInstanceForTesting() {
  return std::make_unique<StubDeviceFetcher>();
}

// static
void DeviceInfoFetcher::SetForceStubForTesting(bool should_force) {
  force_stub_for_testing = should_force;
}

}  // namespace enterprise_signals
