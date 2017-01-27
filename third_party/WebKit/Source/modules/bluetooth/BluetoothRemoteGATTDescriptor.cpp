// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothRemoteGATTDescriptor.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "modules/bluetooth/Bluetooth.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothRemoteGATTService.h"
#include "modules/bluetooth/BluetoothRemoteGATTUtils.h"
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

void BluetoothRemoteGATTDescriptor::ReadValueCallback(
    ScriptPromiseResolver* resolver,
    mojom::blink::WebBluetoothResult result,
    const Optional<Vector<uint8_t>>& value) {
  if (!resolver->getExecutionContext() ||
      resolver->getExecutionContext()->isContextDestroyed())
    return;

  // If the device is disconnected, reject.
  if (!getGatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->reject(BluetoothRemoteGATTUtils::CreateDOMException(
        BluetoothRemoteGATTUtils::ExceptionType::kGATTServerDisconnected));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    DCHECK(value);
    DOMDataView* domDataView =
        BluetoothRemoteGATTUtils::ConvertWTFVectorToDataView(value.value());
    m_value = domDataView;
    resolver->resolve(domDataView);
  } else {
    resolver->reject(BluetoothError::take(resolver, result));
  }
}

ScriptPromise BluetoothRemoteGATTDescriptor::readValue(
    ScriptState* scriptState) {
  if (!getGatt()->connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        BluetoothRemoteGATTUtils::CreateDOMException(
            BluetoothRemoteGATTUtils::ExceptionType::kGATTServerNotConnected));
  }

  if (!getGatt()->device()->isValidDescriptor(m_descriptor->instance_id)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        BluetoothRemoteGATTUtils::CreateDOMException(
            BluetoothRemoteGATTUtils::ExceptionType::kInvalidDescriptor));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  getGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service =
      m_characteristic->m_device->bluetooth()->service();
  service->RemoteDescriptorReadValue(
      m_descriptor->instance_id,
      convertToBaseCallback(
          WTF::bind(&BluetoothRemoteGATTDescriptor::ReadValueCallback,
                    wrapPersistent(this), wrapPersistent(resolver))));

  return promise;
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
