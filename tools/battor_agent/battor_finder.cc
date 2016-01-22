// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/battor_agent/battor_finder.h"

#include "device/serial/serial.mojom.h"
#include "device/serial/serial_device_enumerator.h"
#include "mojo/public/cpp/bindings/array.h"

namespace battor {

namespace {

// The USB display name prefix that all BattOrs have.
const char kBattOrDisplayNamePrefix[] = "BattOr";

}  // namespace

// Returns the path of the first BattOr that we find.
std::string BattOrFinder::FindBattOr() {
  scoped_ptr<device::SerialDeviceEnumerator> serial_device_enumerator =
      device::SerialDeviceEnumerator::Create();

  mojo::Array<device::serial::DeviceInfoPtr> devices =
      serial_device_enumerator->GetDevices();

  for (size_t i = 0; i < devices.size(); i++) {
    std::string display_name = devices[i]->display_name.get();

    if (display_name.find(kBattOrDisplayNamePrefix) != std::string::npos) {
      return devices[i]->path;
    }
  }

  return std::string();
}

}  // namespace battor
