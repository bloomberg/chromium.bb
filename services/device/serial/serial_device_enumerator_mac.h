// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_MAC_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_MAC_H_

#include <vector>

#include "base/macros.h"
#include "services/device/serial/serial_device_enumerator.h"

namespace device {

// Discovers and enumerates serial devices available to the host.
class SerialDeviceEnumeratorMac : public SerialDeviceEnumerator {
 public:
  SerialDeviceEnumeratorMac();
  ~SerialDeviceEnumeratorMac() override;

  // Implementation for SerialDeviceEnumerator.
  std::vector<mojom::SerialPortInfoPtr> GetDevices() override;

 private:
  std::vector<mojom::SerialPortInfoPtr> GetDevicesNew();
  std::vector<mojom::SerialPortInfoPtr> GetDevicesOld();

  DISALLOW_COPY_AND_ASSIGN(SerialDeviceEnumeratorMac);
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_MAC_H_
