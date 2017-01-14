// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothRemoteGATTDescriptor.h"

#include "core/dom/DOMException.h"
#include "modules/bluetooth/BluetoothRemoteGATTService.h"
#include <memory>

namespace blink {

BluetoothRemoteGATTDescriptor::BluetoothRemoteGATTDescriptor(
    mojom::blink::WebBluetoothRemoteGATTDescriptorPtr descriptor,
    BluetoothRemoteGATTCharacteristic* characteristic)
    : m_descriptor(std::move(descriptor)), m_characteristic(characteristic) {}

BluetoothRemoteGATTDescriptor* BluetoothRemoteGATTDescriptor::create(
    mojom::blink::WebBluetoothRemoteGATTDescriptorPtr descriptor,

    BluetoothRemoteGATTCharacteristic* characteristic) {
  BluetoothRemoteGATTDescriptor* result =
      new BluetoothRemoteGATTDescriptor(std::move(descriptor), characteristic);
  return result;
}

ScriptPromise BluetoothRemoteGATTDescriptor::readValue(
    ScriptState* scriptState) {
  // TODO(668837): Implement WebBluetooth descriptor.readValue()
  return ScriptPromise::rejectWithDOMException(
      scriptState,
      DOMException::create(NotSupportedError,
                           "descriptor readValue is not implemented "
                           "yet. See https://goo.gl/J6ASzs"));
}

ScriptPromise BluetoothRemoteGATTDescriptor::writeValue(
    ScriptState* scriptState,
    const DOMArrayPiece& value) {
  // TODO(668838): Implement WebBluetooth descriptor.writeValue()
  return ScriptPromise::rejectWithDOMException(
      scriptState,
      DOMException::create(NotSupportedError,
                           "descriptor writeValue is not implemented "
                           "yet. See https://goo.gl/J6ASzs"));
}

DEFINE_TRACE(BluetoothRemoteGATTDescriptor) {
  visitor->trace(m_characteristic);
  visitor->trace(m_value);
}

}  // namespace blink
