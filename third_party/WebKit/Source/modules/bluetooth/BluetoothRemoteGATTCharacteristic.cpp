// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothRemoteGATTCharacteristic.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/events/Event.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/bluetooth/Bluetooth.h"
#include "modules/bluetooth/BluetoothCharacteristicProperties.h"
#include "modules/bluetooth/BluetoothDevice.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothRemoteGATTDescriptor.h"
#include "modules/bluetooth/BluetoothRemoteGATTService.h"
#include "modules/bluetooth/BluetoothRemoteGATTUtils.h"
#include "modules/bluetooth/BluetoothUUID.h"

#include <memory>
#include <utility>

namespace blink {

BluetoothRemoteGATTCharacteristic::BluetoothRemoteGATTCharacteristic(
    ExecutionContext* context,
    mojom::blink::WebBluetoothRemoteGATTCharacteristicPtr characteristic,
    BluetoothRemoteGATTService* service,
    BluetoothDevice* device)
    : ContextLifecycleObserver(context),
      m_characteristic(std::move(characteristic)),
      m_service(service),
      m_stopped(false),
      m_device(device) {
  m_properties =
      BluetoothCharacteristicProperties::create(m_characteristic->properties);
}

BluetoothRemoteGATTCharacteristic* BluetoothRemoteGATTCharacteristic::create(
    ExecutionContext* context,
    mojom::blink::WebBluetoothRemoteGATTCharacteristicPtr characteristic,
    BluetoothRemoteGATTService* service,
    BluetoothDevice* device) {
  return new BluetoothRemoteGATTCharacteristic(
      context, std::move(characteristic), service, device);
}

void BluetoothRemoteGATTCharacteristic::setValue(DOMDataView* domDataView) {
  m_value = domDataView;
}

void BluetoothRemoteGATTCharacteristic::dispatchCharacteristicValueChanged(
    const Vector<uint8_t>& value) {
  if (!getGatt()->connected())
    return;
  this->setValue(BluetoothRemoteGATTUtils::ConvertWTFVectorToDataView(value));
  dispatchEvent(Event::create(EventTypeNames::characteristicvaluechanged));
}

void BluetoothRemoteGATTCharacteristic::contextDestroyed(ExecutionContext*) {
  notifyCharacteristicObjectRemoved();
}

void BluetoothRemoteGATTCharacteristic::dispose() {
  notifyCharacteristicObjectRemoved();
}

void BluetoothRemoteGATTCharacteristic::notifyCharacteristicObjectRemoved() {
  if (!m_stopped) {
    m_stopped = true;
    m_device->bluetooth()->characteristicObjectRemoved(
        m_characteristic->instance_id);
  }
}

const WTF::AtomicString& BluetoothRemoteGATTCharacteristic::interfaceName()
    const {
  return EventTargetNames::BluetoothRemoteGATTCharacteristic;
}

ExecutionContext* BluetoothRemoteGATTCharacteristic::getExecutionContext()
    const {
  return ContextLifecycleObserver::getExecutionContext();
}

void BluetoothRemoteGATTCharacteristic::addedEventListener(
    const AtomicString& eventType,
    RegisteredEventListener& registeredListener) {
  EventTargetWithInlineData::addedEventListener(eventType, registeredListener);
  // We will also need to unregister a characteristic once all the event
  // listeners have been removed. See http://crbug.com/541390
  if (eventType == EventTypeNames::characteristicvaluechanged) {
    m_device->bluetooth()->registerCharacteristicObject(
        m_characteristic->instance_id, this);
  }
}

void BluetoothRemoteGATTCharacteristic::ReadValueCallback(
    ScriptPromiseResolver* resolver,
    mojom::blink::WebBluetoothResult result,
    const Optional<Vector<uint8_t>>& value) {
  if (!resolver->getExecutionContext() ||
      resolver->getExecutionContext()->isContextDestroyed())
    return;

  // If the device is disconnected, reject.
  if (!getGatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->reject(BluetoothError::createDOMException(
        blink::mojom::WebBluetoothResult::GATT_SERVER_DISCONNECTED));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    DCHECK(value);
    DOMDataView* domDataView =
        BluetoothRemoteGATTUtils::ConvertWTFVectorToDataView(value.value());
    setValue(domDataView);
    resolver->resolve(domDataView);
  } else {
    resolver->reject(BluetoothError::createDOMException(result));
  }
}

ScriptPromise BluetoothRemoteGATTCharacteristic::readValue(
    ScriptState* scriptState) {
  if (!getGatt()->connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        BluetoothError::createDOMException(
            blink::mojom::WebBluetoothResult::GATT_SERVER_NOT_CONNECTED));
  }

  if (!getGatt()->device()->isValidCharacteristic(
          m_characteristic->instance_id)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, createInvalidCharacteristicError());
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  getGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->service();
  service->RemoteCharacteristicReadValue(
      m_characteristic->instance_id,
      convertToBaseCallback(
          WTF::bind(&BluetoothRemoteGATTCharacteristic::ReadValueCallback,
                    wrapPersistent(this), wrapPersistent(resolver))));

  return promise;
}

void BluetoothRemoteGATTCharacteristic::WriteValueCallback(
    ScriptPromiseResolver* resolver,
    const Vector<uint8_t>& value,
    mojom::blink::WebBluetoothResult result) {
  if (!resolver->getExecutionContext() ||
      resolver->getExecutionContext()->isContextDestroyed())
    return;

  // If the device is disconnected, reject.
  if (!getGatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->reject(BluetoothError::createDOMException(
        blink::mojom::WebBluetoothResult::GATT_SERVER_DISCONNECTED));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    setValue(BluetoothRemoteGATTUtils::ConvertWTFVectorToDataView(value));
    resolver->resolve();
  } else {
    resolver->reject(BluetoothError::createDOMException(result));
  }
}

ScriptPromise BluetoothRemoteGATTCharacteristic::writeValue(
    ScriptState* scriptState,
    const DOMArrayPiece& value) {
  if (!getGatt()->connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        BluetoothError::createDOMException(
            blink::mojom::WebBluetoothResult::GATT_SERVER_NOT_CONNECTED));
  }

  if (!getGatt()->device()->isValidCharacteristic(
          m_characteristic->instance_id)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, createInvalidCharacteristicError());
  }

  // Partial implementation of writeValue algorithm:
  // https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetoothremotegattcharacteristic-writevalue

  // If bytes is more than 512 bytes long (the maximum length of an attribute
  // value, per Long Attribute Values) return a promise rejected with an
  // InvalidModificationError and abort.
  if (value.byteLength() > 512)
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidModificationError,
                                          "Value can't exceed 512 bytes."));

  // Let valueVector be a copy of the bytes held by value.
  Vector<uint8_t> valueVector;
  valueVector.append(value.bytes(), value.byteLength());

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  getGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->service();
  service->RemoteCharacteristicWriteValue(
      m_characteristic->instance_id, valueVector,
      convertToBaseCallback(WTF::bind(
          &BluetoothRemoteGATTCharacteristic::WriteValueCallback,
          wrapPersistent(this), wrapPersistent(resolver), valueVector)));

  return promise;
}

void BluetoothRemoteGATTCharacteristic::NotificationsCallback(
    ScriptPromiseResolver* resolver,
    mojom::blink::WebBluetoothResult result) {
  if (!resolver->getExecutionContext() ||
      resolver->getExecutionContext()->isContextDestroyed())
    return;

  // If the device is disconnected, reject.
  if (!getGatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->reject(BluetoothError::createDOMException(
        blink::mojom::WebBluetoothResult::GATT_SERVER_DISCONNECTED));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    resolver->resolve(this);
  } else {
    resolver->reject(BluetoothError::createDOMException(result));
  }
}

ScriptPromise BluetoothRemoteGATTCharacteristic::startNotifications(
    ScriptState* scriptState) {
  if (!getGatt()->connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        BluetoothError::createDOMException(
            blink::mojom::WebBluetoothResult::GATT_SERVER_NOT_CONNECTED));
  }

  if (!getGatt()->device()->isValidCharacteristic(
          m_characteristic->instance_id)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, createInvalidCharacteristicError());
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  getGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->service();
  service->RemoteCharacteristicStartNotifications(
      m_characteristic->instance_id,
      convertToBaseCallback(
          WTF::bind(&BluetoothRemoteGATTCharacteristic::NotificationsCallback,
                    wrapPersistent(this), wrapPersistent(resolver))));

  return promise;
}

ScriptPromise BluetoothRemoteGATTCharacteristic::stopNotifications(
    ScriptState* scriptState) {
  if (!getGatt()->connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        BluetoothError::createDOMException(
            blink::mojom::WebBluetoothResult::GATT_SERVER_NOT_CONNECTED));
  }

  if (!getGatt()->device()->isValidCharacteristic(
          m_characteristic->instance_id)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, createInvalidCharacteristicError());
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  getGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->service();
  service->RemoteCharacteristicStopNotifications(
      m_characteristic->instance_id,
      convertToBaseCallback(
          WTF::bind(&BluetoothRemoteGATTCharacteristic::NotificationsCallback,
                    wrapPersistent(this), wrapPersistent(resolver),
                    mojom::blink::WebBluetoothResult::SUCCESS)));
  return promise;
}

ScriptPromise BluetoothRemoteGATTCharacteristic::getDescriptor(
    ScriptState* scriptState,
    const StringOrUnsignedLong& descriptorUUID,
    ExceptionState& exceptionState) {
  String descriptor =
      BluetoothUUID::getDescriptor(descriptorUUID, exceptionState);
  if (exceptionState.hadException())
    return exceptionState.reject(scriptState);

  return getDescriptorsImpl(scriptState,
                            mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE,
                            descriptor);
}

ScriptPromise BluetoothRemoteGATTCharacteristic::getDescriptors(
    ScriptState* scriptState,
    ExceptionState&) {
  return getDescriptorsImpl(
      scriptState, mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE);
}

ScriptPromise BluetoothRemoteGATTCharacteristic::getDescriptors(
    ScriptState* scriptState,
    const StringOrUnsignedLong& descriptorUUID,
    ExceptionState& exceptionState) {
  String descriptor =
      BluetoothUUID::getDescriptor(descriptorUUID, exceptionState);
  if (exceptionState.hadException())
    return exceptionState.reject(scriptState);

  return getDescriptorsImpl(
      scriptState, mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE,
      descriptor);
}

ScriptPromise BluetoothRemoteGATTCharacteristic::getDescriptorsImpl(
    ScriptState* scriptState,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    const String& descriptorsUUID) {
  if (!getGatt()->connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        BluetoothError::createDOMException(
            blink::mojom::WebBluetoothResult::GATT_SERVER_NOT_CONNECTED));
  }

  if (!getGatt()->device()->isValidCharacteristic(
          m_characteristic->instance_id)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, createInvalidCharacteristicError());
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  getGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->service();
  service->RemoteCharacteristicGetDescriptors(
      m_characteristic->instance_id, quantity, descriptorsUUID,
      convertToBaseCallback(WTF::bind(
          &BluetoothRemoteGATTCharacteristic::GetDescriptorsCallback,
          wrapPersistent(this), descriptorsUUID, m_characteristic->instance_id,
          quantity, wrapPersistent(resolver))));

  return promise;
}

// Callback that allows us to resolve the promise with a single descriptor
// or with a vector owning the descriptors.
void BluetoothRemoteGATTCharacteristic::GetDescriptorsCallback(
    const String& requestedDescriptorUUID,
    const String& characteristicInstanceId,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    ScriptPromiseResolver* resolver,
    mojom::blink::WebBluetoothResult result,
    Optional<Vector<mojom::blink::WebBluetoothRemoteGATTDescriptorPtr>>
        descriptors) {
  if (!resolver->getExecutionContext() ||
      resolver->getExecutionContext()->isContextDestroyed())
    return;

  // If the device is disconnected, reject.
  if (!service()->device()->gatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->reject(BluetoothError::createDOMException(
        blink::mojom::WebBluetoothResult::GATT_SERVER_DISCONNECTED));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    DCHECK(descriptors);

    if (quantity == mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE) {
      DCHECK_EQ(1u, descriptors->size());
      resolver->resolve(
          service()->device()->getOrCreateBluetoothRemoteGATTDescriptor(
              std::move(descriptors.value()[0]), this));
      return;
    }

    HeapVector<Member<BluetoothRemoteGATTDescriptor>> gattDescriptors;
    gattDescriptors.reserveInitialCapacity(descriptors->size());
    for (auto& descriptor : descriptors.value()) {
      gattDescriptors.push_back(
          service()->device()->getOrCreateBluetoothRemoteGATTDescriptor(
              std::move(descriptor), this));
    }
    resolver->resolve(gattDescriptors);
  } else {
    if (result == mojom::blink::WebBluetoothResult::DESCRIPTOR_NOT_FOUND) {
      resolver->reject(BluetoothError::createDOMException(
          BluetoothErrorCode::DescriptorNotFound,
          "No Descriptors matching UUID " + requestedDescriptorUUID +
              " found in Characteristic with UUID " + uuid() + "."));
    } else {
      resolver->reject(BluetoothError::createDOMException(result));
    }
  }
}

DOMException*
BluetoothRemoteGATTCharacteristic::createInvalidCharacteristicError() {
  return BluetoothError::createDOMException(
      BluetoothErrorCode::InvalidCharacteristic,
      "Characteristic with UUID " + uuid() +
          " is no longer valid. Remember to retrieve the characteristic again "
          "after reconnecting.");
}

DEFINE_TRACE(BluetoothRemoteGATTCharacteristic) {
  visitor->trace(m_service);
  visitor->trace(m_properties);
  visitor->trace(m_value);
  visitor->trace(m_device);
  EventTargetWithInlineData::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink
