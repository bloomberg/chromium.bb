// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_port_impl.h"

#include "base/macros.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/serial.mojom.h"

namespace device {

namespace {

class SerialPortImplTest : public DeviceServiceTestBase {
 public:
  SerialPortImplTest() = default;
  ~SerialPortImplTest() override = default;

 protected:
  void SetUp() override {
    DeviceServiceTestBase::SetUp();
    connector()->BindInterface(mojom::kServiceName, &port_manager_);
  }

  void TearDown() override { port_manager_.reset(); }

  mojom::SerialPortManagerPtr port_manager_;
  DISALLOW_COPY_AND_ASSIGN(SerialPortImplTest);
};

// This is to simply test that on Linux/Mac/Windows a client can connect to
// Device Service and bind the serial SerialPort interface correctly.
// TODO(leonhsl): figure out how to add more robust tests.
TEST_F(SerialPortImplTest, SimpleConnectTest) {
  mojom::SerialPortPtr serial_port;
  port_manager_->GetPort(base::UnguessableToken::Create(),
                         mojo::MakeRequest(&serial_port));
  serial_port.FlushForTesting();
}

}  // namespace

}  // namespace device
