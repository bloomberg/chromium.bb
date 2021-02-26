// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/device_info_fetcher_mac.h"

#import <Foundation/Foundation.h>

#include <IOKit/IOKitLib.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include "base/files/file_util.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/process/launch.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
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
  return base::mac::GetPlatformSerialNumber();
}

enterprise_reporting_private::SettingValue GetScreenlockSecured() {
  CFStringRef screen_saver_application = CFSTR("com.apple.screensaver");
  CFStringRef ask_for_password_key = CFSTR("askForPassword");
  base::ScopedCFTypeRef<CFTypeRef> ask_for_password(CFPreferencesCopyAppValue(
      ask_for_password_key, screen_saver_application));

  if (!ask_for_password || !base::mac::CFCast<CFBooleanRef>(ask_for_password))
    return enterprise_reporting_private::SETTING_VALUE_UNKNOWN;

  bool screen_lock_enabled =
      base::mac::CFCastStrict<CFBooleanRef>(ask_for_password) == kCFBooleanTrue;
  return screen_lock_enabled
             ? enterprise_reporting_private::SETTING_VALUE_ENABLED
             : enterprise_reporting_private::SETTING_VALUE_DISABLED;
}

enterprise_reporting_private::SettingValue GetDiskEncrypted() {
  base::FilePath fdesetup_path("/usr/bin/fdesetup");
  if (!base::PathExists(fdesetup_path))
    return enterprise_reporting_private::SETTING_VALUE_UNKNOWN;

  base::CommandLine command(fdesetup_path);
  command.AppendArg("status");
  std::string output;
  if (!base::GetAppOutput(command, &output))
    return enterprise_reporting_private::SETTING_VALUE_UNKNOWN;

  if (output.find("FileVault is On") != std::string::npos)
    return enterprise_reporting_private::SETTING_VALUE_ENABLED;
  if (output.find("FileVault is Off") != std::string::npos)
    return enterprise_reporting_private::SETTING_VALUE_DISABLED;

  return enterprise_reporting_private::SETTING_VALUE_UNKNOWN;
}

std::vector<std::string> GetMacAddresses() {
  std::vector<std::string> result;
  struct ifaddrs* ifa = nullptr;

  if (getifaddrs(&ifa) != 0)
    return result;

  struct ifaddrs* interface = ifa;
  for (; interface != nullptr; interface = interface->ifa_next) {
    if (interface->ifa_addr == nullptr ||
        interface->ifa_addr->sa_family != AF_LINK) {
      continue;
    }
    struct sockaddr_dl* sdl =
        reinterpret_cast<struct sockaddr_dl*>(interface->ifa_addr);
    if (!sdl || sdl->sdl_alen != 6)
      continue;
    char* link_address = static_cast<char*>(LLADDR(sdl));
    result.push_back(base::StringPrintf(
        "%02x:%02x:%02x:%02x:%02x:%02x", link_address[0] & 0xff,
        link_address[1] & 0xff, link_address[2] & 0xff, link_address[3] & 0xff,
        link_address[4] & 0xff, link_address[5] & 0xff));
  }
  return result;
}

}  // namespace

DeviceInfoFetcherMac::DeviceInfoFetcherMac() {}

DeviceInfoFetcherMac::~DeviceInfoFetcherMac() {}

DeviceInfo DeviceInfoFetcherMac::Fetch() {
  DeviceInfo device_info;
  device_info.os_name = "macOS";
  device_info.os_version = GetOsVersion();
  device_info.device_host_name = GetDeviceHostName();
  device_info.device_model = GetDeviceModel();
  device_info.serial_number = GetSerialNumber();
  device_info.screen_lock_secured = GetScreenlockSecured();
  device_info.disk_encrypted = GetDiskEncrypted();
  device_info.mac_addresses = GetMacAddresses();
  return device_info;
}

}  // namespace enterprise_reporting
}  // namespace extensions
