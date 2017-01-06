// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothRemoteGATTService.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/bluetooth/Bluetooth.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothRemoteGATTCharacteristic.h"
#include "modules/bluetooth/BluetoothUUID.h"
#include "wtf/PtrUtil.h"
#include <memory>
#include <utility>

namespace blink {

namespace {

const char kGATTServerDisconnected[] =
    "GATT Server disconnected while retrieving characteristics.";
const char kGATTServerNotConnected[] =
    "GATT Server is disconnected. Cannot retrieve characteristics.";
const char kInvalidService[] =
    "Service is no longer valid. Remember to retrieve the service again after "
    "reconnecting.";

}  // namespace

BluetoothRemoteGATTService::BluetoothRemoteGATTService(
    const String& serviceInstanceId,
    const String& uuid,
    bool isPrimary,
    const String& deviceInstanceId,
    BluetoothDevice* device)
    : m_serviceInstanceId(serviceInstanceId),
      m_uuid(uuid),
      m_isPrimary(isPrimary),
      m_deviceInstanceId(deviceInstanceId),
      m_device(device) {}

DEFINE_TRACE(BluetoothRemoteGATTService) {
  visitor->trace(m_device);
}

// Callback that allows us to resolve the promise with a single Characteristic
// or with a vector owning the characteristics.
void BluetoothRemoteGATTService::GetCharacteristicsCallback(
    const String& serviceInstanceId,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    ScriptPromiseResolver* resolver,
    mojom::blink::WebBluetoothResult result,
    Optional<Vector<mojom::blink::WebBluetoothRemoteGATTCharacteristicPtr>>
        characteristics) {
  if (!resolver->getExecutionContext() ||
      resolver->getExecutionContext()->isContextDestroyed())
    return;

  // If the resolver is not in the set of ActiveAlgorithms then the frame
  // disconnected so we reject.
  if (!device()->gatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->reject(
        DOMException::create(NetworkError, kGATTServerDisconnected));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    DCHECK(characteristics);

    if (quantity == mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE) {
      DCHECK_EQ(1u, characteristics->size());
      resolver->resolve(device()->getOrCreateRemoteGATTCharacteristic(
          resolver->getExecutionContext(),
          characteristics.value()[0]->instance_id, serviceInstanceId,
          characteristics.value()[0]->uuid,
          characteristics.value()[0]->properties, this));
      return;
    }

    HeapVector<Member<BluetoothRemoteGATTCharacteristic>> gattCharacteristics;
    gattCharacteristics.reserveInitialCapacity(characteristics->size());
    for (const auto& characteristic : characteristics.value()) {
      gattCharacteristics.push_back(
          device()->getOrCreateRemoteGATTCharacteristic(
              resolver->getExecutionContext(), characteristic->instance_id,
              serviceInstanceId, characteristic->uuid,
              characteristic->properties, this));
    }
    resolver->resolve(gattCharacteristics);
  } else {
    resolver->reject(BluetoothError::take(resolver, result));
  }
}

ScriptPromise BluetoothRemoteGATTService::getCharacteristic(
    ScriptState* scriptState,
    const StringOrUnsignedLong& characteristic,
    ExceptionState& exceptionState) {
  String characteristicUUID =
      BluetoothUUID::getCharacteristic(characteristic, exceptionState);
  if (exceptionState.hadException())
    return exceptionState.reject(scriptState);

  return getCharacteristicsImpl(
      scriptState, mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE,
      characteristicUUID);
}

ScriptPromise BluetoothRemoteGATTService::getCharacteristics(
    ScriptState* scriptState,
    const StringOrUnsignedLong& characteristic,
    ExceptionState& exceptionState) {
  String characteristicUUID =
      BluetoothUUID::getCharacteristic(characteristic, exceptionState);
  if (exceptionState.hadException())
    return exceptionState.reject(scriptState);

  return getCharacteristicsImpl(
      scriptState, mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE,
      characteristicUUID);
}

ScriptPromise BluetoothRemoteGATTService::getCharacteristics(
    ScriptState* scriptState,
    ExceptionState&) {
  return getCharacteristicsImpl(
      scriptState, mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE);
}

ScriptPromise BluetoothRemoteGATTService::getCharacteristicsImpl(
    ScriptState* scriptState,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    const String& characteristicsUUID) {
  // We always check that the device is connected.
  if (!device()->gatt()->connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(NetworkError, kGATTServerNotConnected));
  }

  if (!device()->isValidService(m_serviceInstanceId)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidStateError, kInvalidService));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  device()->gatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->service();

  WTF::Optional<String> uuid = WTF::nullopt;
  if (!characteristicsUUID.isEmpty())
    uuid = characteristicsUUID;
  service->RemoteServiceGetCharacteristics(
      m_serviceInstanceId, quantity, uuid,
      convertToBaseCallback(
          WTF::bind(&BluetoothRemoteGATTService::GetCharacteristicsCallback,
                    wrapPersistent(this), m_serviceInstanceId, quantity,
                    wrapPersistent(resolver))));

  return promise;
}

}  // namespace blink
