// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_WIN_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_WIN_H_

#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "services/device/serial/serial_device_enumerator.h"

namespace device {

// Discovers and enumerates serial devices available to the host.
class SerialDeviceEnumeratorWin : public SerialDeviceEnumerator {
 public:
  SerialDeviceEnumeratorWin();
  ~SerialDeviceEnumeratorWin() override;

  // Implementation for SerialDeviceEnumerator.
  std::vector<mojom::SerialPortInfoPtr> GetDevices() override;

  // Searches for the COM port in the device's friendly name and returns the
  // appropriate device path or nullopt if the input did not contain a valid
  // name.
  static base::Optional<base::FilePath> GetPath(
      const std::string& friendly_name);

 private:
  std::vector<mojom::SerialPortInfoPtr> GetDevicesNew();
  std::vector<mojom::SerialPortInfoPtr> GetDevicesOld();

  DISALLOW_COPY_AND_ASSIGN(SerialDeviceEnumeratorWin);
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_WIN_H_
