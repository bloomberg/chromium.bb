// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_PORT_MANAGER_IMPL_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_PORT_MANAGER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "services/device/public/mojom/serial.mojom.h"

namespace base {
class SingleThreadTaskRunner;
class UnguessableToken;
}

namespace device {

class SerialDeviceEnumerator;

// TODO(leonhsl): Merge this class with SerialDeviceEnumerator if/once
// SerialDeviceEnumerator is exposed only via the Device Service.
// crbug.com/748505
class SerialPortManagerImpl : public mojom::SerialPortManager {
 public:
  static void Create(
      mojom::SerialPortManagerRequest request,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  SerialPortManagerImpl(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  ~SerialPortManagerImpl() override;

  void SetSerialEnumeratorForTesting(
      std::unique_ptr<SerialDeviceEnumerator> fake_enumerator);

 private:
  // mojom::SerialPortManager methods:
  void GetDevices(GetDevicesCallback callback) override;
  void GetPort(const base::UnguessableToken& token,
               mojom::SerialPortRequest request) override;

  std::unique_ptr<SerialDeviceEnumerator> enumerator_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SerialPortManagerImpl);
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_PORT_MANAGER_IMPL_H_
