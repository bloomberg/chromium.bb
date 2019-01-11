// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth_le_scan.h"

namespace blink {

BluetoothLEScan* BluetoothLEScan::Create(
    mojo::BindingId id,
    Bluetooth* bluetooth,
    mojom::blink::WebBluetoothRequestLEScanOptionsPtr options) {
  return MakeGarbageCollected<BluetoothLEScan>(id, bluetooth,
                                               std::move(options));
}

BluetoothLEScan::BluetoothLEScan(
    mojo::BindingId id,
    Bluetooth* bluetooth,
    mojom::blink::WebBluetoothRequestLEScanOptionsPtr options)
    : id_(id),
      bluetooth_(bluetooth),
      keep_repeated_devices_(options ? options->keep_repeated_devices : false),
      accept_all_advertisements_(options ? options->accept_all_advertisements
                                         : false) {}

const HeapVector<Member<BluetoothLEScanFilterInit>>& BluetoothLEScan::filters()
    const {
  // TODO(dougt) We need to support filters.
  return filters_;
}

bool BluetoothLEScan::keepRepeatedDevices() const {
  return keep_repeated_devices_;
}
bool BluetoothLEScan::acceptAllAdvertisements() const {
  return accept_all_advertisements_;
}

bool BluetoothLEScan::active() const {
  return is_active_;
}

bool BluetoothLEScan::stop() {
  bluetooth_->CancelScan(id_);
  is_active_ = false;
  return true;
}

void BluetoothLEScan::Trace(blink::Visitor* visitor) {
  visitor->Trace(filters_);
  visitor->Trace(bluetooth_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
