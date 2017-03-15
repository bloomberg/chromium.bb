// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothRemoteGATTDescriptor.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothRemoteGATTService.h"
#include "modules/bluetooth/BluetoothRemoteGATTUtils.h"
#include <memory>

namespace blink {

BluetoothRemoteGATTDescriptor::BluetoothRemoteGATTDescriptor(
    mojom::blink::WebBluetoothRemoteGATTDescriptorPtr descriptor,
    BluetoothRemoteGATTCharacteristic* characteristic)
    : m_descriptor(std::move(descriptor)), m_characteristic(characteristic) {}

BluetoothRemoteGATTDescriptor* BluetoothRemoteGATTDescriptor::Create(
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
  if (!GetGatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->reject(
        BluetoothError::CreateNotConnectedException(BluetoothOperation::GATT));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    DCHECK(value);
    DOMDataView* domDataView =
        BluetoothRemoteGATTUtils::ConvertWTFVectorToDataView(value.value());
    m_value = domDataView;
    resolver->resolve(domDataView);
  } else {
    resolver->reject(BluetoothError::CreateDOMException(result));
  }
}

ScriptPromise BluetoothRemoteGATTDescriptor::readValue(
    ScriptState* scriptState) {
  if (!GetGatt()->connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        BluetoothError::CreateNotConnectedException(BluetoothOperation::GATT));
  }

  if (!GetGatt()->device()->IsValidDescriptor(m_descriptor->instance_id)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, CreateInvalidDescriptorError());
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  GetGatt()->AddToActiveAlgorithms(resolver);
  GetService()->RemoteDescriptorReadValue(
      m_descriptor->instance_id,
      convertToBaseCallback(
          WTF::bind(&BluetoothRemoteGATTDescriptor::ReadValueCallback,
                    wrapPersistent(this), wrapPersistent(resolver))));

  return promise;
}

void BluetoothRemoteGATTDescriptor::WriteValueCallback(
    ScriptPromiseResolver* resolver,
    const Vector<uint8_t>& value,
    mojom::blink::WebBluetoothResult result) {
  if (!resolver->getExecutionContext() ||
      resolver->getExecutionContext()->isContextDestroyed())
    return;

  // If the resolver is not in the set of ActiveAlgorithms then the frame
  // disconnected so we reject.
  if (!GetGatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->reject(
        BluetoothError::CreateNotConnectedException(BluetoothOperation::GATT));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    m_value = BluetoothRemoteGATTUtils::ConvertWTFVectorToDataView(value);
    resolver->resolve();
  } else {
    resolver->reject(BluetoothError::CreateDOMException(result));
  }
}

ScriptPromise BluetoothRemoteGATTDescriptor::writeValue(
    ScriptState* scriptState,
    const DOMArrayPiece& value) {
  if (!GetGatt()->connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        BluetoothError::CreateNotConnectedException(BluetoothOperation::GATT));
  }

  if (!GetGatt()->device()->IsValidDescriptor(m_descriptor->instance_id)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, CreateInvalidDescriptorError());
  }

  // Partial implementation of writeValue algorithm:
  // https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetoothremotegattdescriptor-writevalue

  // If bytes is more than 512 bytes long (the maximum length of an attribute
  // value, per Long Attribute Values) return a promise rejected with an
  // InvalidModificationError and abort.
  if (value.byteLength() > 512) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidModificationError,
                                          "Value can't exceed 512 bytes."));
  }

  // Let valueVector be a copy of the bytes held by value.
  Vector<uint8_t> valueVector;
  valueVector.append(value.bytes(), value.byteLength());

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  GetGatt()->AddToActiveAlgorithms(resolver);
  GetService()->RemoteDescriptorWriteValue(
      m_descriptor->instance_id, valueVector,
      convertToBaseCallback(WTF::bind(
          &BluetoothRemoteGATTDescriptor::WriteValueCallback,
          wrapPersistent(this), wrapPersistent(resolver), valueVector)));

  return promise;
}

DOMException* BluetoothRemoteGATTDescriptor::CreateInvalidDescriptorError() {
  return BluetoothError::CreateDOMException(
      BluetoothErrorCode::InvalidDescriptor,
      "Descriptor with UUID " + uuid() +
          " is no longer valid. Remember to retrieve the Descriptor again "
          "after reconnecting.");
}

DEFINE_TRACE(BluetoothRemoteGATTDescriptor) {
  visitor->trace(m_characteristic);
  visitor->trace(m_value);
}

}  // namespace blink
