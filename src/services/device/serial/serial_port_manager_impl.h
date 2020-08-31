// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_PORT_MANAGER_IMPL_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_PORT_MANAGER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "services/device/serial/serial_device_enumerator.h"

namespace base {
class SingleThreadTaskRunner;
class UnguessableToken;
}

namespace device {

// TODO(leonhsl): Merge this class with SerialDeviceEnumerator if/once
// SerialDeviceEnumerator is exposed only via the Device Service.
// crbug.com/748505
class SerialPortManagerImpl : public mojom::SerialPortManager,
                              public SerialDeviceEnumerator::Observer {
 public:
  SerialPortManagerImpl(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  ~SerialPortManagerImpl() override;

  void Bind(mojo::PendingReceiver<mojom::SerialPortManager> receiver);
  void SetSerialEnumeratorForTesting(
      std::unique_ptr<SerialDeviceEnumerator> fake_enumerator);

 private:
  // mojom::SerialPortManager methods:
  void SetClient(
      mojo::PendingRemote<mojom::SerialPortManagerClient> client) override;
  void GetDevices(GetDevicesCallback callback) override;
  void GetPort(
      const base::UnguessableToken& token,
      mojo::PendingReceiver<mojom::SerialPort> receiver,
      mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher) override;

  // SerialDeviceEnumerator::Observer methods:
  void OnPortAdded(const mojom::SerialPortInfo& port) override;
  void OnPortRemoved(const mojom::SerialPortInfo& port) override;

  std::unique_ptr<SerialDeviceEnumerator> enumerator_;
  ScopedObserver<SerialDeviceEnumerator, SerialDeviceEnumerator::Observer>
      observed_enumerator_{this};

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  mojo::ReceiverSet<SerialPortManager> receivers_;
  mojo::RemoteSet<mojom::SerialPortManagerClient> clients_;

  DISALLOW_COPY_AND_ASSIGN(SerialPortManagerImpl);
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_PORT_MANAGER_IMPL_H_
