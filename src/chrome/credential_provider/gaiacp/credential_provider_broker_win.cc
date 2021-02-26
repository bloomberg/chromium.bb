// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/credential_provider_broker_win.h"

#define INITGUID

// clang-format off
#include <windows.h>
#include <hidclass.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <setupapi.h>
// clang-format on

#include "base/memory/free_deleter.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_devinfo.h"
#include "mojo/public/cpp/platform/platform_handle.h"

using mojo::PlatformHandle;

namespace credential_provider {

namespace {

struct PreparsedDataScopedTraits {
  static PHIDP_PREPARSED_DATA InvalidValue() { return nullptr; }
  static void Free(PHIDP_PREPARSED_DATA h) { HidD_FreePreparsedData(h); }
};
using ScopedPreparsedData =
    base::ScopedGeneric<PHIDP_PREPARSED_DATA, PreparsedDataScopedTraits>;

// Opens the HID device handle with the input |device_path| and
// returns a ScopedHandle.
base::win::ScopedHandle OpenHidDevice(const base::string16& device_path) {
  base::win::ScopedHandle file(
      CreateFile(device_path.c_str(), GENERIC_WRITE | GENERIC_READ,
                 FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                 FILE_FLAG_OVERLAPPED, nullptr));
  if (!file.IsValid() && GetLastError() == ERROR_ACCESS_DENIED) {
    file.Set(CreateFile(device_path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                        nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr));
  }
  return file;
}

// Gets the usage page for the input device |handle|.
base::Optional<uint16_t> GetUsagePage(HANDLE handle) {
  ScopedPreparsedData scoped_preparsed_data;
  if (!HidD_GetPreparsedData(
          handle, ScopedPreparsedData::Receiver(scoped_preparsed_data).get()) ||
      !scoped_preparsed_data.is_valid()) {
    return base::nullopt;
  }

  HIDP_CAPS capabilities = {};
  if (HidP_GetCaps(scoped_preparsed_data.get(), &capabilities) !=
      HIDP_STATUS_SUCCESS) {
    return base::nullopt;
  }

  return capabilities.UsagePage;
}

// Extracts the device path from |device_info_set| and |device_interface_data|
// input fields.
base::Optional<base::string16> GetDevicePath(
    HDEVINFO device_info_set,
    PSP_DEVICE_INTERFACE_DATA device_interface_data) {
  DWORD required_size = 0;

  // Determine the required size of detail struct.
  SetupDiGetDeviceInterfaceDetail(device_info_set, device_interface_data,
                                  nullptr, 0, &required_size, nullptr);

  std::unique_ptr<SP_DEVICE_INTERFACE_DETAIL_DATA, base::FreeDeleter>
      device_interface_detail_data(
          static_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(malloc(required_size)));
  device_interface_detail_data->cbSize =
      sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

  // Get the detailed data for this device.
  if (!SetupDiGetDeviceInterfaceDetail(device_info_set, device_interface_data,
                                       device_interface_detail_data.get(),
                                       required_size, nullptr, nullptr)) {
    return base::nullopt;
  }
  // Extract the device path and compare it with input device path
  // and ignore it if it doesn't match the input device path.
  return base::string16(device_interface_detail_data->DevicePath);
}
}  // namespace

constexpr uint16_t FIDO_USAGE_PAGE = 0xf1d0;  // FIDO alliance HID usage page

CredentialProviderBrokerWin::CredentialProviderBrokerWin() = default;

CredentialProviderBrokerWin::~CredentialProviderBrokerWin() = default;

void CredentialProviderBrokerWin::OpenDevice(
    const base::string16& input_device_path,
    OpenDeviceCallback callback) {
  base::win::ScopedDevInfo device_info_set(
      SetupDiGetClassDevs(&GUID_DEVINTERFACE_HID, nullptr, nullptr,
                          DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));

  if (device_info_set.get() != INVALID_HANDLE_VALUE) {
    SP_DEVICE_INTERFACE_DATA device_interface_data = {};
    device_interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (int device_index = 0; SetupDiEnumDeviceInterfaces(
             device_info_set.get(), nullptr, &GUID_DEVINTERFACE_HID,
             device_index, &device_interface_data);
         ++device_index) {
      base::Optional<base::string16> device_path =
          GetDevicePath(device_info_set.get(), &device_interface_data);
      if (!device_path)
        continue;

      DCHECK(base::IsStringASCII(device_path.value()));
      if (!base::EqualsCaseInsensitiveASCII(device_path.value(),
                                            input_device_path)) {
        continue;
      }

      base::win::ScopedHandle device_handle =
          OpenHidDevice(device_path.value());
      if (!device_handle.IsValid())
        break;

      base::Optional<uint16_t> usage_page = GetUsagePage(device_handle.Get());

      // Only if the input device path is corresponding to a FIDO
      // device, we will return appropriate device handle. Otherwise,
      // return an invalid device handle.
      if (usage_page && usage_page.value() == FIDO_USAGE_PAGE)
        std::move(callback).Run(PlatformHandle(std::move(device_handle)));
      break;
    }
  }

  // Return default PlatformHandle when we can't find any appropriate device
  // handle.
  std::move(callback).Run(PlatformHandle());
  return;
}
}  // namespace credential_provider
