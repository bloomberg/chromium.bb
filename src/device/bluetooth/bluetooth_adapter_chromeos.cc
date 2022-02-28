// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/floss/bluetooth_adapter_floss.h"
#include "device/bluetooth/floss/floss_features.h"

namespace device {

scoped_refptr<device::BluetoothAdapter> BluetoothAdapter::CreateAdapter() {
  if (base::FeatureList::IsEnabled(floss::features::kFlossEnabled)) {
    return floss::BluetoothAdapterFloss::CreateAdapter();
  } else {
    return bluez::BluetoothAdapterBlueZ::CreateAdapter();
  }
}

}  // namespace device
