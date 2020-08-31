// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/device_info_fetcher_win.h"

#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/win/windows_types.h"
#include "base/win/wmi.h"
#include "net/base/network_interfaces.h"

// Those headers need defines from windows_types.h, thus have to come after it.
#include <powersetting.h>  // NOLINT(build/include_order)
#include <propsys.h>       // NOLINT(build/include_order)
#include <shobjidl.h>      // NOLINT(build/include_order)

namespace enterprise_reporting_private =
    ::extensions::api::enterprise_reporting_private;

namespace extensions {
namespace enterprise_reporting {

namespace {

// Possible results of the "System.Volume.BitLockerProtection" shell property.
// These values are undocumented but were directly validated on a Windows 10
// machine. See the comment above the GetDiskEncryption() method.
// The values in this enum should be kep in sync with the analogous definiotion
// in the native app implementation.
enum class BitLockerStatus {
  // Encryption is on, and the volume is unlocked
  kOn = 1,
  // Encryption is off
  kOff = 2,
  // Encryption is in progress
  kEncryptionInProgress = 3,
  // Decryption is in progress
  kDecryptionInProgress = 4,
  // Encryption is on, but temporarily suspended
  kSuspended = 5,
  // Encryption is on, and the volume is locked
  kLocked = 6,
};

// Retrieves the computer serial number from WMI.
std::string GetSerialNumber() {
  base::win::WmiComputerSystemInfo sys_info =
      base::win::WmiComputerSystemInfo::Get();
  return base::UTF16ToUTF8(sys_info.serial_number());
}

// Retrieves the state of the screen locking feature from the screen saver
// settings.
base::Optional<bool> GetScreenLockStatus() {
  base::Optional<bool> status;
  BOOL value = FALSE;
  if (::SystemParametersInfo(SPI_GETSCREENSAVESECURE, 0, &value, 0))
    status = static_cast<bool>(value);
  return status;
}

// Checks if locking is enabled at the currently active power scheme.
base::Optional<bool> GetConsoleLockStatus() {
  base::Optional<bool> status;
  SYSTEM_POWER_STATUS sps;
  // https://docs.microsoft.com/en-us/windows/desktop/api/winbase/nf-winbase-getsystempowerstatus
  // Retrieves the power status of the system. The status indicates whether the
  // system is running on AC or DC power.
  if (!::GetSystemPowerStatus(&sps))
    return status;

  LPGUID p_active_policy = nullptr;
  // https://docs.microsoft.com/en-us/windows/desktop/api/powersetting/nf-powersetting-powergetactivescheme
  // Retrieves the active power scheme and returns a GUID that identifies the
  // scheme.
  if (::PowerGetActiveScheme(nullptr, &p_active_policy) == ERROR_SUCCESS) {
    constexpr GUID kConsoleLock = {
        0x0E796BDB,
        0x100D,
        0x47D6,
        {0xA2, 0xD5, 0xF7, 0xD2, 0xDA, 0xA5, 0x1F, 0x51}};
    const GUID active_policy = *p_active_policy;
    ::LocalFree(p_active_policy);

    auto const power_read_current_value_func =
        sps.ACLineStatus != 0U ? &PowerReadACValue : &PowerReadDCValue;
    ULONG type;
    DWORD value;
    DWORD value_size = sizeof(value);
    // https://docs.microsoft.com/en-us/windows/desktop/api/powersetting/nf-powersetting-powerreadacvalue
    // Retrieves the AC/DC power value for the specified power setting.
    // NO_SUBGROUP_GUID to retrieve the setting for the default power scheme.
    // LPBYTE case is safe and is needed as the function expects generic byte
    // array buffer regardless of the exact value read as it is a generic
    // interface.
    if (power_read_current_value_func(
            nullptr, &active_policy, &NO_SUBGROUP_GUID, &kConsoleLock, &type,
            reinterpret_cast<LPBYTE>(&value), &value_size) == ERROR_SUCCESS) {
      status = value != 0U;
    }
  }

  return status;
}

// Gets cumulative screen locking policy based on the screen saver and console
// lock status.
enterprise_reporting_private::SettingValue GetScreenlockSecured() {
  const base::Optional<bool> screen_lock_status = GetScreenLockStatus();
  if (screen_lock_status.value_or(false))
    return enterprise_reporting_private::SETTING_VALUE_ENABLED;

  const base::Optional<bool> console_lock_status = GetConsoleLockStatus();
  if (console_lock_status.value_or(false))
    return enterprise_reporting_private::SETTING_VALUE_ENABLED;

  if (screen_lock_status.has_value() || console_lock_status.has_value()) {
    return enterprise_reporting_private::SETTING_VALUE_DISABLED;
  }

  return enterprise_reporting_private::SETTING_VALUE_UNKNOWN;
}

// Returns the volume where the Windows OS is installed.
base::Optional<std::wstring> GetOsVolume() {
  base::Optional<std::wstring> volume;
  base::FilePath windows_dir;
  if (base::PathService::Get(base::DIR_WINDOWS, &windows_dir) &&
      windows_dir.IsAbsolute()) {
    std::vector<std::wstring> components;
    windows_dir.GetComponents(&components);
    DCHECK(components.size());
    volume = components[0];
  }
  return volume;
}

bool GetPropVariantAsInt64(PROPVARIANT variant, int64_t* out_value) {
  switch (variant.vt) {
    case VT_I2:
      *out_value = variant.iVal;
      return true;
    case VT_UI2:
      *out_value = variant.uiVal;
      return true;
    case VT_I4:
      *out_value = variant.lVal;
      return true;
    case VT_UI4:
      *out_value = variant.ulVal;
      return true;
    case VT_INT:
      *out_value = variant.intVal;
      return true;
    case VT_UINT:
      *out_value = variant.uintVal;
      return true;
  }
  return false;
}

// The ideal solution to check the disk encryption (BitLocker) status is to
// use the WMI APIs (Win32_EncryptableVolume). However, only programs running
// with elevated priledges can access those APIs.
//
// Our alternative solution is based on the value of the undocumented (shell)
// property: "System.Volume.BitLockerProtection". That property is essentially
// an enum containing the current BitLocker status for a given volume. This
// approached was suggested here:
// https://stackoverflow.com/questions/41308245/detect-bitlocker-programmatically-from-c-sharp-without-admin/41310139
//
// Note that the link above doesn't give any explanation / meaning for the
// enum values, it simply says that 1, 3 or 5 means the disk is encrypted.
//
// I directly tested and validated this strategy on a Windows 10 machine.
// The values given in the BitLockerStatus enum contain the relevant values
// for the shell property. I also directly validated them.
enterprise_reporting_private::SettingValue GetDiskEncrypted() {
  // |volume| has to be a |wstring| because SHCreateItemFromParsingName() only
  // accepts |PCWSTR| which is |wchar_t*|.
  base::Optional<std::wstring> volume = GetOsVolume();
  if (!volume.has_value())
    return enterprise_reporting_private::SETTING_VALUE_UNKNOWN;

  PROPERTYKEY key;
  const HRESULT property_key_result =
      PSGetPropertyKeyFromName(L"System.Volume.BitLockerProtection", &key);
  if (FAILED(property_key_result))
    return enterprise_reporting_private::SETTING_VALUE_UNKNOWN;

  Microsoft::WRL::ComPtr<IShellItem2> item;
  const HRESULT create_item_result = SHCreateItemFromParsingName(
      volume.value().c_str(), nullptr, IID_IShellItem2, &item);
  if (FAILED(create_item_result) || !item)
    return enterprise_reporting_private::SETTING_VALUE_UNKNOWN;

  PROPVARIANT prop_status;
  const HRESULT property_result = item->GetProperty(key, &prop_status);
  int64_t status;
  if (FAILED(property_result) || !GetPropVariantAsInt64(prop_status, &status))
    return enterprise_reporting_private::SETTING_VALUE_UNKNOWN;

  // Note that we are not considering BitLockerStatus::Suspended as ENABLED.
  if (status == static_cast<int64_t>(BitLockerStatus::kOn) ||
      status == static_cast<int64_t>(BitLockerStatus::kEncryptionInProgress) ||
      status == static_cast<int64_t>(BitLockerStatus::kLocked)) {
    return enterprise_reporting_private::SETTING_VALUE_ENABLED;
  }

  return enterprise_reporting_private::SETTING_VALUE_DISABLED;
}

}  // namespace

DeviceInfoFetcherWin::DeviceInfoFetcherWin() = default;

DeviceInfoFetcherWin::~DeviceInfoFetcherWin() = default;

DeviceInfo DeviceInfoFetcherWin::Fetch() {
  DeviceInfo device_info;
  device_info.os_name = "windows";
  device_info.os_version = base::SysInfo::OperatingSystemVersion();
  device_info.device_host_name = net::GetHostName();
  device_info.device_model = base::SysInfo::HardwareModelName();
  device_info.serial_number = GetSerialNumber();
  device_info.screen_lock_secured = GetScreenlockSecured();
  device_info.disk_encrypted = GetDiskEncrypted();
  return device_info;
}

}  // namespace enterprise_reporting
}  // namespace extensions
