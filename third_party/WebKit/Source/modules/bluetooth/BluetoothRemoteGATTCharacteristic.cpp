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
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"

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
      m_device(device) {
  m_properties =
      BluetoothCharacteristicProperties::Create(m_characteristic->properties);
}

BluetoothRemoteGATTCharacteristic* BluetoothRemoteGATTCharacteristic::Create(
    ExecutionContext* context,
    mojom::blink::WebBluetoothRemoteGATTCharacteristicPtr characteristic,
    BluetoothRemoteGATTService* service,
    BluetoothDevice* device) {
  return new BluetoothRemoteGATTCharacteristic(
      context, std::move(characteristic), service, device);
}

void BluetoothRemoteGATTCharacteristic::SetValue(DOMDataView* domDataView) {
  m_value = domDataView;
}

void BluetoothRemoteGATTCharacteristic::RemoteCharacteristicValueChanged(
    const Vector<uint8_t>& value) {
  if (!GetGatt()->connected())
    return;
  this->SetValue(BluetoothRemoteGATTUtils::ConvertWTFVectorToDataView(value));
  dispatchEvent(Event::create(EventTypeNames::characteristicvaluechanged));
}

void BluetoothRemoteGATTCharacteristic::contextDestroyed(ExecutionContext*) {
  Dispose();
}

void BluetoothRemoteGATTCharacteristic::Dispose() {
  m_clientBindings.CloseAllBindings();
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
}

void BluetoothRemoteGATTCharacteristic::ReadValueCallback(
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
    SetValue(domDataView);
    dispatchEvent(Event::create(EventTypeNames::characteristicvaluechanged));
    resolver->resolve(domDataView);
  } else {
    resolver->reject(BluetoothError::CreateDOMException(result));
  }
}

ScriptPromise BluetoothRemoteGATTCharacteristic::readValue(
    ScriptState* scriptState) {
  if (!GetGatt()->connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        BluetoothError::CreateNotConnectedException(BluetoothOperation::GATT));
  }

  if (!GetGatt()->device()->IsValidCharacteristic(
          m_characteristic->instance_id)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, CreateInvalidCharacteristicError());
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  GetGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->Service();
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
  if (!GetGatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->reject(
        BluetoothError::CreateNotConnectedException(BluetoothOperation::GATT));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    SetValue(BluetoothRemoteGATTUtils::ConvertWTFVectorToDataView(value));
    resolver->resolve();
  } else {
    resolver->reject(BluetoothError::CreateDOMException(result));
  }
}

ScriptPromise BluetoothRemoteGATTCharacteristic::writeValue(
    ScriptState* scriptState,
    const DOMArrayPiece& value) {
  if (!GetGatt()->connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        BluetoothError::CreateNotConnectedException(BluetoothOperation::GATT));
  }

  if (!GetGatt()->device()->IsValidCharacteristic(
          m_characteristic->instance_id)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, CreateInvalidCharacteristicError());
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
  GetGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->Service();
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
  if (!GetGatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->reject(
        BluetoothError::CreateNotConnectedException(BluetoothOperation::GATT));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    resolver->resolve(this);
  } else {
    resolver->reject(BluetoothError::CreateDOMException(result));
  }
}

ScriptPromise BluetoothRemoteGATTCharacteristic::startNotifications(
    ScriptState* scriptState) {
  if (!GetGatt()->connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        BluetoothError::CreateNotConnectedException(BluetoothOperation::GATT));
  }

  if (!GetGatt()->device()->IsValidCharacteristic(
          m_characteristic->instance_id)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, CreateInvalidCharacteristicError());
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  GetGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->Service();
  mojom::blink::WebBluetoothCharacteristicClientAssociatedPtrInfo ptrInfo;
  auto request = mojo::MakeRequest(&ptrInfo);
  m_clientBindings.AddBinding(this, std::move(request));

  service->RemoteCharacteristicStartNotifications(
      m_characteristic->instance_id, std::move(ptrInfo),
      convertToBaseCallback(
          WTF::bind(&BluetoothRemoteGATTCharacteristic::NotificationsCallback,
                    wrapPersistent(this), wrapPersistent(resolver))));

  return promise;
}

ScriptPromise BluetoothRemoteGATTCharacteristic::stopNotifications(
    ScriptState* scriptState) {
  if (!GetGatt()->connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        BluetoothError::CreateNotConnectedException(BluetoothOperation::GATT));
  }

  if (!GetGatt()->device()->IsValidCharacteristic(
          m_characteristic->instance_id)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, CreateInvalidCharacteristicError());
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  GetGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->Service();
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

  return GetDescriptorsImpl(scriptState,
                            mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE,
                            descriptor);
}

ScriptPromise BluetoothRemoteGATTCharacteristic::getDescriptors(
    ScriptState* scriptState,
    ExceptionState&) {
  return GetDescriptorsImpl(
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

  return GetDescriptorsImpl(
      scriptState, mojom::blink::WebBluetoothGATTQueryQuantity::MULTIPLE,
      descriptor);
}

ScriptPromise BluetoothRemoteGATTCharacteristic::GetDescriptorsImpl(
    ScriptState* scriptState,
    mojom::blink::WebBluetoothGATTQueryQuantity quantity,
    const String& descriptorsUUID) {
  if (!GetGatt()->connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, BluetoothError::CreateNotConnectedException(
                         BluetoothOperation::DescriptorsRetrieval));
  }

  if (!GetGatt()->device()->IsValidCharacteristic(
          m_characteristic->instance_id)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, CreateInvalidCharacteristicError());
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  GetGatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->Service();
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
  if (!m_service->device()->gatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->reject(BluetoothError::CreateNotConnectedException(
        BluetoothOperation::DescriptorsRetrieval));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    DCHECK(descriptors);

    if (quantity == mojom::blink::WebBluetoothGATTQueryQuantity::SINGLE) {
      DCHECK_EQ(1u, descriptors->size());
      resolver->resolve(
          m_service->device()->GetOrCreateBluetoothRemoteGATTDescriptor(
              std::move(descriptors.value()[0]), this));
      return;
    }

    HeapVector<Member<BluetoothRemoteGATTDescriptor>> gattDescriptors;
    gattDescriptors.reserveInitialCapacity(descriptors->size());
    for (auto& descriptor : descriptors.value()) {
      gattDescriptors.push_back(
          m_service->device()->GetOrCreateBluetoothRemoteGATTDescriptor(
              std::move(descriptor), this));
    }
    resolver->resolve(gattDescriptors);
  } else {
    if (result == mojom::blink::WebBluetoothResult::DESCRIPTOR_NOT_FOUND) {
      resolver->reject(BluetoothError::CreateDOMException(
          BluetoothErrorCode::DescriptorNotFound,
          "No Descriptors matching UUID " + requestedDescriptorUUID +
              " found in Characteristic with UUID " + uuid() + "."));
    } else {
      resolver->reject(BluetoothError::CreateDOMException(result));
    }
  }
}

DOMException*
BluetoothRemoteGATTCharacteristic::CreateInvalidCharacteristicError() {
  return BluetoothError::CreateDOMException(
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
