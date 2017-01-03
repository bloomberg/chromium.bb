// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothRemoteGATTCharacteristic.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMDataView.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/Event.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/bluetooth/Bluetooth.h"
#include "modules/bluetooth/BluetoothCharacteristicProperties.h"
#include "modules/bluetooth/BluetoothDevice.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothRemoteGATTService.h"
#include <memory>

namespace blink {

namespace {

const char kGATTServerDisconnected[] =
    "GATT Server disconnected while performing a GATT operation.";
const char kGATTServerNotConnected[] =
    "GATT Server is disconnected. Cannot perform GATT operations.";
const char kInvalidCharacteristic[] =
    "Characteristic is no longer valid. Remember to retrieve the "
    "characteristic again after reconnecting.";

DOMDataView* ConvertWTFVectorToDataView(const Vector<uint8_t>& wtfVector) {
  static_assert(sizeof(*wtfVector.data()) == 1,
                "uint8_t should be a single byte");
  DOMArrayBuffer* domBuffer =
      DOMArrayBuffer::create(wtfVector.data(), wtfVector.size());
  return DOMDataView::create(domBuffer, 0, wtfVector.size());
}

}  // anonymous namespace

BluetoothRemoteGATTCharacteristic::BluetoothRemoteGATTCharacteristic(
    ExecutionContext* context,
    const String& characteristicInstanceId,
    const String& serviceInstanceId,
    const String& uuid,
    uint32_t characteristicProperties,
    BluetoothRemoteGATTService* service,
    BluetoothDevice* device)
    : ContextLifecycleObserver(context),
      m_characteristicInstanceId(characteristicInstanceId),
      m_serviceInstanceId(serviceInstanceId),
      m_uuid(uuid),
      m_characteristicProperties(characteristicProperties),
      m_service(service),
      m_stopped(false),
      m_device(device) {
  m_properties =
      BluetoothCharacteristicProperties::create(m_characteristicProperties);
}

BluetoothRemoteGATTCharacteristic* BluetoothRemoteGATTCharacteristic::create(
    ExecutionContext* context,
    const String& characteristicInstanceId,
    const String& serviceInstanceId,
    const String& uuid,
    uint32_t characteristicProperties,
    BluetoothRemoteGATTService* service,
    BluetoothDevice* device) {
  return new BluetoothRemoteGATTCharacteristic(
      context, characteristicInstanceId, serviceInstanceId, uuid,
      characteristicProperties, service, device);
}

void BluetoothRemoteGATTCharacteristic::setValue(DOMDataView* domDataView) {
  m_value = domDataView;
}

void BluetoothRemoteGATTCharacteristic::dispatchCharacteristicValueChanged(
    const Vector<uint8_t>& value) {
  this->setValue(ConvertWTFVectorToDataView(value));
  dispatchEvent(Event::create(EventTypeNames::characteristicvaluechanged));
}

void BluetoothRemoteGATTCharacteristic::contextDestroyed() {
  notifyCharacteristicObjectRemoved();
}

void BluetoothRemoteGATTCharacteristic::dispose() {
  notifyCharacteristicObjectRemoved();
}

void BluetoothRemoteGATTCharacteristic::notifyCharacteristicObjectRemoved() {
  if (!m_stopped) {
    m_stopped = true;
    m_device->bluetooth()->characteristicObjectRemoved(
        m_characteristicInstanceId);
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
        m_characteristicInstanceId, this);
  }
}

void BluetoothRemoteGATTCharacteristic::ReadValueCallback(
    ScriptPromiseResolver* resolver,
    mojom::blink::WebBluetoothResult result,
    const Optional<Vector<uint8_t>>& value) {
  if (!resolver->getExecutionContext() ||
      resolver->getExecutionContext()->isContextDestroyed())
    return;

  // If the resolver is not in the set of ActiveAlgorithms then the frame
  // disconnected so we reject.
  if (!gatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->reject(
        DOMException::create(NetworkError, kGATTServerDisconnected));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    DCHECK(value);
    DOMDataView* domDataView = ConvertWTFVectorToDataView(value.value());
    setValue(domDataView);
    resolver->resolve(domDataView);
  } else {
    resolver->reject(BluetoothError::take(resolver, result));
  }
}

ScriptPromise BluetoothRemoteGATTCharacteristic::readValue(
    ScriptState* scriptState) {
  // We always check that the device is connected.
  if (!gatt()->connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(NetworkError, kGATTServerNotConnected));
  }

  if (!gatt()->device()->isValidCharacteristic(m_characteristicInstanceId)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(InvalidStateError, kInvalidCharacteristic));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  gatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->service();
  service->RemoteCharacteristicReadValue(
      m_characteristicInstanceId,
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

  // If the resolver is not in the set of ActiveAlgorithms then the frame
  // disconnected so we reject.
  if (!gatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->reject(
        DOMException::create(NetworkError, kGATTServerDisconnected));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    setValue(ConvertWTFVectorToDataView(value));
    resolver->resolve();
  } else {
    resolver->reject(BluetoothError::take(resolver, result));
  }
}

ScriptPromise BluetoothRemoteGATTCharacteristic::writeValue(
    ScriptState* scriptState,
    const DOMArrayPiece& value) {
  // We always check that the device is connected.
  if (!gatt()->connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(NetworkError, kGATTServerNotConnected));
  }

  if (!gatt()->device()->isValidCharacteristic(m_characteristicInstanceId)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(InvalidStateError, kInvalidCharacteristic));
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
  gatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->service();
  service->RemoteCharacteristicWriteValue(
      m_characteristicInstanceId, valueVector,
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

  // If the resolver is not in the set of ActiveAlgorithms then the frame
  // disconnected so we reject.
  if (!gatt()->RemoveFromActiveAlgorithms(resolver)) {
    resolver->reject(
        DOMException::create(NetworkError, kGATTServerDisconnected));
    return;
  }

  if (result == mojom::blink::WebBluetoothResult::SUCCESS) {
    resolver->resolve(this);
  } else {
    resolver->reject(BluetoothError::take(resolver, result));
  }
}

ScriptPromise BluetoothRemoteGATTCharacteristic::startNotifications(
    ScriptState* scriptState) {
  // We always check that the device is connected.
  if (!gatt()->connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(NetworkError, kGATTServerNotConnected));
  }

  if (!gatt()->device()->isValidCharacteristic(m_characteristicInstanceId)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(InvalidStateError, kInvalidCharacteristic));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  gatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->service();
  service->RemoteCharacteristicStartNotifications(
      m_characteristicInstanceId,
      convertToBaseCallback(
          WTF::bind(&BluetoothRemoteGATTCharacteristic::NotificationsCallback,
                    wrapPersistent(this), wrapPersistent(resolver))));

  return promise;
}

ScriptPromise BluetoothRemoteGATTCharacteristic::stopNotifications(
    ScriptState* scriptState) {
  // We always check that the device is connected.
  if (!gatt()->connected()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(NetworkError, kGATTServerNotConnected));
  }

  if (!gatt()->device()->isValidCharacteristic(m_characteristicInstanceId)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(InvalidStateError, kInvalidCharacteristic));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  gatt()->AddToActiveAlgorithms(resolver);

  mojom::blink::WebBluetoothService* service = m_device->bluetooth()->service();
  service->RemoteCharacteristicStopNotifications(
      m_characteristicInstanceId,
      convertToBaseCallback(
          WTF::bind(&BluetoothRemoteGATTCharacteristic::NotificationsCallback,
                    wrapPersistent(this), wrapPersistent(resolver),
                    mojom::blink::WebBluetoothResult::SUCCESS)));
  return promise;
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
