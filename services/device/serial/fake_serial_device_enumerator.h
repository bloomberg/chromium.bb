// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_FAKE_SERIAL_DEVICE_ENUMERATOR_H_
#define SERVICES_DEVICE_SERIAL_FAKE_SERIAL_DEVICE_ENUMERATOR_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "services/device/serial/serial_device_enumerator.h"

namespace device {

class FakeSerialEnumerator : public SerialDeviceEnumerator {
 public:
  FakeSerialEnumerator();
  ~FakeSerialEnumerator() override;

  bool AddDevicePath(const base::FilePath& path);

  std::vector<mojom::SerialPortInfoPtr> GetDevices() override;

 private:
  std::vector<base::FilePath> device_paths_;
  DISALLOW_COPY_AND_ASSIGN(FakeSerialEnumerator);
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_FAKE_SERIAL_DEVICE_ENUMERATOR_H_
