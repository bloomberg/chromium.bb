// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/bluetooth/bluetooth_system.h"

#include <utility>

#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/mojom/bluetooth_system.mojom.h"
#include "services/device/public/mojom/constants.mojom.h"

namespace device {

class BluetoothSystemTest : public DeviceServiceTestBase,
                            public mojom::BluetoothSystemClient {
 public:
  BluetoothSystemTest() = default;
  ~BluetoothSystemTest() override = default;

  void SetUp() override {
    DeviceServiceTestBase::SetUp();
    connector()->BindInterface(mojom::kServiceName, &system_factory_);
  }

 protected:
  mojom::BluetoothSystemFactoryPtr system_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothSystemTest);
};

TEST_F(BluetoothSystemTest, FactoryCreate) {
  mojom::BluetoothSystemPtr system_ptr;
  mojo::Binding<mojom::BluetoothSystemClient> client_binding(this);

  mojom::BluetoothSystemClientPtr client_ptr;
  client_binding.Bind(mojo::MakeRequest(&client_ptr));

  EXPECT_FALSE(system_ptr.is_bound());

  system_factory_->Create(mojo::MakeRequest(&system_ptr),
                          std::move(client_ptr));
  base::RunLoop run_loop;
  system_ptr.FlushAsyncForTesting(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(system_ptr.is_bound());
}

}  // namespace device
