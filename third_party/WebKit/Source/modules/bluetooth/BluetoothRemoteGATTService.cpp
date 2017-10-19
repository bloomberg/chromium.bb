// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothRemoteGATTService.h"

#include <utility>
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/bluetooth/Bluetooth.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothRemoteGATTCharacteristic.h"
#include "modules/bluetooth/BluetoothUUID.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

BluetoothRemoteGATTService::BluetoothRemoteGATTService(
    mojom::blink::WebBluetoothRemoteGATTServicePtr service,
    bool is_primary,
    const String& device_instance_id,
    BluetoothDevice* device)
    : service_(std::move(service)),
      is_primary_(is_primary),
      device_instance_id_(device_instance_id),
      device_(device) {}

void BluetoothRemoteGATTService::Trace(blink::Visitor* visitor) {
  visitor->Trace(device_);
}

// Callback that allows us to resolve the promise with a single characteristic
// or with a vector owning the characteristics.
void BluetoothRemoteGATTService::GetCharacteristicsCallback(
    const String& service_instance_id,
    const String& requested_characteristic_uuid,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    ScriptPromiseResolver* resolver,
    mojom::blink::WebBluetoothResult result,
    Optional<Vector<mojom::blink::WebBluetoothRemoteGATTCharacteristicPtr>>
        characteristics) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  // If the device is disconnected, reject.
  if (!device_->gatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->Reject(BluetoothError::CreateNotConnectedException(
        BluetoothOperation::kCharacteristicsRetrieval));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    DCHECK(characteristics);

    if (quantity == mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE) {
      DCHECK_EQ(1u, characteristics->size());
      resolver->Resolve(device_->GetOrCreateRemoteGATTCharacteristic(
          resolver->GetExecutionContext(),
          std::move(characteristics.value()[0]), this));
      return;
    }

    HeapVector<Member<BluetoothRemoteGATTCharacteristic>> gatt_characteristics;
    gatt_characteristics.ReserveInitialCapacity(characteristics->size());
    for (auto& characteristic : characteristics.value()) {
      gatt_characteristics.push_back(
          device_->GetOrCreateRemoteGATTCharacteristic(
              resolver->GetExecutionContext(), std::move(characteristic),
              this));
    }
    resolver->Resolve(gatt_characteristics);
  } else {
    if (result == mojom::blink::WebBluetoothResult::CHARACTERISTIC_NOT_FOUND) {
      resolver->Reject(BluetoothError::CreateDOMException(
          BluetoothErrorCode::kCharacteristicNotFound,
          "No Characteristics matching UUID " + requested_characteristic_uuid +
              " found in Service with UUID " + uuid() + "."));
    } else {
      resolver->Reject(BluetoothError::CreateDOMException(result));
    }
  }
}

ScriptPromise BluetoothRemoteGATTService::getCharacteristic(
    ScriptState* script_state,
    const StringOrUnsignedLong& characteristic,
    ExceptionState& exception_state) {
  String characteristic_uuid =
      BluetoothUUID::getCharacteristic(characteristic, exception_state);
  if (exception_state.HadException())
    return exception_state.Reject(script_state);

  return GetCharacteristicsImpl(
      script_state, mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE,
      characteristic_uuid);
}

ScriptPromise BluetoothRemoteGATTService::getCharacteristics(
    ScriptState* script_state,
    const StringOrUnsignedLong& characteristic,
    ExceptionState& exception_state) {
  String characteristic_uuid =
      BluetoothUUID::getCharacteristic(characteristic, exception_state);
  if (exception_state.HadException())
    return exception_state.Reject(script_state);

  return GetCharacteristicsImpl(
      script_state, mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE,
      characteristic_uuid);
}

ScriptPromise BluetoothRemoteGATTService::getCharacteristics(
    ScriptState* script_state,
    ExceptionState&) {
  return GetCharacteristicsImpl(
      script_state, mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE);
}

ScriptPromise BluetoothRemoteGATTService::GetCharacteristicsImpl(
    ScriptState* script_state,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    const String& characteristics_uuid) {
  if (!device_->gatt()->connected()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, BluetoothError::CreateNotConnectedException(
                          BluetoothOperation::kCharacteristicsRetrieval));
  }

  if (!device_->IsValidService(service_->instance_id)) {
    return ScriptPromise::RejectWithDOMException(
        script_state, BluetoothError::CreateDOMException(
                          BluetoothErrorCode::kInvalidService,
                          "Service with UUID " + service_->uuid +
                              " is no longer valid. Remember to retrieve "
                              "the service again after reconnecting."));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  device_->gatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service =
      device_->GetBluetooth()->Service();
  service->RemoteServiceGetCharacteristics(
      service_->instance_id, quantity, characteristics_uuid,
      ConvertToBaseCallback(
          WTF::Bind(&BluetoothRemoteGATTService::GetCharacteristicsCallback,
                    WrapPersistent(this), service_->instance_id,
                    characteristics_uuid, quantity, WrapPersistent(resolver))));

  return promise;
}

}  // namespace blink
