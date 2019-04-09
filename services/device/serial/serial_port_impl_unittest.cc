// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_port_impl.h"

#include "base/macros.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/mojom/serial.mojom.h"

namespace device {

namespace {

class SerialPortImplTest : public DeviceServiceTestBase {
 public:
  SerialPortImplTest() = default;
  ~SerialPortImplTest() override = default;

 protected:
  DISALLOW_COPY_AND_ASSIGN(SerialPortImplTest);
};

TEST_F(SerialPortImplTest, WatcherClosedWhenPortClosed) {
  mojom::SerialPortPtr serial_port;
  mojom::SerialPortConnectionWatcherPtrInfo watcher_ptr;
  auto watcher_binding = mojo::MakeStrongBinding(
      std::make_unique<mojom::SerialPortConnectionWatcher>(),
      mojo::MakeRequest(&watcher_ptr));
  SerialPortImpl::Create(base::FilePath(), mojo::MakeRequest(&serial_port),
                         std::move(watcher_ptr),
                         base::ThreadTaskRunnerHandle::Get());

  // To start with both the serial port connection and the connection watcher
  // connection should remain open.
  serial_port.FlushForTesting();
  EXPECT_FALSE(serial_port.encountered_error());
  watcher_binding->FlushForTesting();
  EXPECT_TRUE(watcher_binding);

  // When the serial port connection is closed the watcher connection should be
  // closed.
  serial_port.reset();
  watcher_binding->FlushForTesting();
  EXPECT_FALSE(watcher_binding);
}

TEST_F(SerialPortImplTest, PortClosedWhenWatcherClosed) {
  mojom::SerialPortPtr serial_port;
  mojom::SerialPortConnectionWatcherPtrInfo watcher_ptr;
  auto watcher_binding = mojo::MakeStrongBinding(
      std::make_unique<mojom::SerialPortConnectionWatcher>(),
      mojo::MakeRequest(&watcher_ptr));
  SerialPortImpl::Create(base::FilePath(), mojo::MakeRequest(&serial_port),
                         std::move(watcher_ptr),
                         base::ThreadTaskRunnerHandle::Get());

  // To start with both the serial port connection and the connection watcher
  // connection should remain open.
  serial_port.FlushForTesting();
  EXPECT_FALSE(serial_port.encountered_error());
  watcher_binding->FlushForTesting();
  EXPECT_TRUE(watcher_binding);

  // When the watcher connection is closed, for safety, the serial port
  // connection should also be closed.
  watcher_binding->Close();
  serial_port.FlushForTesting();
  EXPECT_TRUE(serial_port.encountered_error());
}

}  // namespace

}  // namespace device
