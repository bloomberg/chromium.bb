// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/device_info_fetcher_linux.h"

#if defined(USE_GIO)
#include <gio/gio.h>
#endif  // defined(USE_GIO)
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <string>

#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "net/base/network_interfaces.h"

namespace enterprise_reporting_private =
    ::extensions::api::enterprise_reporting_private;

namespace extensions {
namespace enterprise_reporting {

namespace {

std::string GetDeviceModel() {
  return base::SysInfo::HardwareModelName();
}

std::string GetOsVersion() {
  return base::SysInfo::OperatingSystemVersion();
}

std::string GetDeviceHostName() {
  return net::GetHostName();
}

std::string GetSerialNumber() {
  return std::string();
}

// Implements the logic from the native client setup script. It reads the
// setting value straight from gsettings but picks the schema relevant to the
// currently active desktop environment.
// The current implementation support Gnone and Cinnamon only.
enterprise_reporting_private::SettingValue GetScreenlockSecured() {
#if defined(USE_GIO)
  constexpr char kLockScreenKey[] = "lock-enabled";

  std::unique_ptr<base::Environment> env(base::Environment::Create());
  const base::nix::DesktopEnvironment desktop_env =
      base::nix::GetDesktopEnvironment(env.get());
  if (desktop_env != base::nix::DESKTOP_ENVIRONMENT_CINNAMON &&
      desktop_env != base::nix::DESKTOP_ENVIRONMENT_GNOME) {
    return enterprise_reporting_private::SETTING_VALUE_UNKNOWN;
  }

  const std::string settings_schema = base::StringPrintf(
      "org.%s.desktop.screensaver",
      desktop_env == base::nix::DESKTOP_ENVIRONMENT_CINNAMON ? "cinnamon"
                                                             : "gnome");

  GSettingsSchema* screensaver_schema = g_settings_schema_source_lookup(
      g_settings_schema_source_get_default(), settings_schema.c_str(), FALSE);
  GSettings* screensaver_settings = nullptr;
  if (!screensaver_schema ||
      !g_settings_schema_has_key(screensaver_schema, kLockScreenKey)) {
    return enterprise_reporting_private::SETTING_VALUE_UNKNOWN;
  }
  screensaver_settings = g_settings_new(settings_schema.c_str());
  if (!screensaver_settings)
    return enterprise_reporting_private::SETTING_VALUE_UNKNOWN;
  gboolean lock_screen_enabled =
      g_settings_get_boolean(screensaver_settings, kLockScreenKey);
  g_object_unref(screensaver_settings);

  return lock_screen_enabled
             ? enterprise_reporting_private::SETTING_VALUE_ENABLED
             : enterprise_reporting_private::SETTING_VALUE_DISABLED;
#else
  return enterprise_reporting_private::SETTING_VALUE_UNKNOWN;
#endif  // defined(USE_GIO)
}

// Implements the logic from the native host installation script. First find the
// root device identifier, then locate its parent and get its type.
enterprise_reporting_private::SettingValue GetDiskEncrypted() {
  struct stat info;
  // First figure out the device identifier.
  stat("/", &info);
  int dev_major = major(info.st_dev);
  // The parent identifier will have the same major and minor 0. If and only if
  // it is a dm device can it also be an encrypted device (as evident from the
  // source code of the lsblk command).
  base::FilePath dev_uuid(
      base::StringPrintf("/sys/dev/block/%d:0/dm/uuid", dev_major));
  std::string uuid;
  if (base::PathExists(dev_uuid) &&
      base::ReadFileToStringWithMaxSize(dev_uuid, &uuid, 1024)) {
    // The device uuid starts with the driver type responsible for it. If it is
    // the "crypt" driver then it is an encrypted device.
    bool is_encrypted =
        base::StartsWith(uuid, "crypt-", base::CompareCase::INSENSITIVE_ASCII);
    return is_encrypted ? enterprise_reporting_private::SETTING_VALUE_ENABLED
                        : enterprise_reporting_private::SETTING_VALUE_DISABLED;
  }
  return enterprise_reporting_private::SETTING_VALUE_UNKNOWN;
}

}  // namespace

DeviceInfoFetcherLinux::DeviceInfoFetcherLinux() = default;

DeviceInfoFetcherLinux::~DeviceInfoFetcherLinux() = default;

DeviceInfo DeviceInfoFetcherLinux::Fetch() {
  DeviceInfo device_info;
  device_info.os_name = "linux";
  device_info.os_version = GetOsVersion();
  device_info.device_host_name = GetDeviceHostName();
  device_info.device_model = GetDeviceModel();
  device_info.serial_number = GetSerialNumber();
  device_info.screen_lock_secured = GetScreenlockSecured();
  device_info.disk_encrypted = GetDiskEncrypted();
  return device_info;
}

}  // namespace enterprise_reporting
}  // namespace extensions
