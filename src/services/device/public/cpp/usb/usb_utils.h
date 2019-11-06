// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_USB_USB_UTILS_H_
#define SERVICES_DEVICE_PUBLIC_CPP_USB_USB_UTILS_H_

#include <vector>

#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"

namespace device {

bool UsbDeviceFilterMatches(const mojom::UsbDeviceFilter& filter,
                            const mojom::UsbDeviceInfo& device_info);

bool UsbDeviceFilterMatchesAny(
    const std::vector<mojom::UsbDeviceFilterPtr>& filters,
    const mojom::UsbDeviceInfo& device_info);

std::vector<mojom::UsbIsochronousPacketPtr> BuildIsochronousPacketArray(
    const std::vector<uint32_t>& packet_lengths,
    mojom::UsbTransferStatus status);

uint8_t ConvertEndpointAddressToNumber(uint8_t address);

uint8_t ConvertEndpointNumberToAddress(
    const mojom::UsbEndpointInfo& mojo_endpoint);

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_USB_USB_UTILS_H_
