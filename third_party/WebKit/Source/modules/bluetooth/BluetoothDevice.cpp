// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothDevice.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/events/Event.h"
#include "modules/bluetooth/Bluetooth.h"
#include "modules/bluetooth/BluetoothAttributeInstanceMap.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothRemoteGATTServer.h"
#include <memory>
#include <utility>

namespace blink {

BluetoothDevice::BluetoothDevice(ExecutionContext* context,
                                 const String& id,
                                 const String& name,
                                 Bluetooth* bluetooth)
    : ContextLifecycleObserver(context),
      m_attributeInstanceMap(new BluetoothAttributeInstanceMap(this)),
      m_id(id),
      m_name(name),
      m_gatt(BluetoothRemoteGATTServer::create(this)),
      m_bluetooth(bluetooth) {}

// static
BluetoothDevice* BluetoothDevice::take(ScriptPromiseResolver* resolver,
                                       const String& id,
                                       const String& name,
                                       Bluetooth* bluetooth) {
  return new BluetoothDevice(resolver->getExecutionContext(), id, name,
                             bluetooth);
}

// static
mojom::blink::WebBluetoothDeviceIdPtr BluetoothDevice::createMojoDeviceId(
    const String& deviceId) {
  auto result = mojom::blink::WebBluetoothDeviceId::New();
  result->device_id = deviceId;
  return result;
}

BluetoothRemoteGATTService*
BluetoothDevice::getOrCreateBluetoothRemoteGATTService(
    const String& serviceInstanceId,
    const String& uuid,
    bool isPrimary,
    const String& deviceInstanceId) {
  return m_attributeInstanceMap->getOrCreateBluetoothRemoteGATTService(
      serviceInstanceId, uuid, isPrimary, deviceInstanceId);
}

bool BluetoothDevice::isValidService(const String& serviceInstanceId) {
  return m_attributeInstanceMap->containsService(serviceInstanceId);
}

BluetoothRemoteGATTCharacteristic*
BluetoothDevice::getOrCreateBluetoothRemoteGATTCharacteristic(
    ExecutionContext* context,
    const String& characteristicInstanceId,
    const String& serviceInstanceId,
    const String& uuid,
    uint32_t characteristicProperties,
    BluetoothRemoteGATTService* service) {
  return m_attributeInstanceMap->getOrCreateBluetoothRemoteGATTCharacteristic(
      context, characteristicInstanceId, serviceInstanceId, uuid,
      characteristicProperties, service);
}

bool BluetoothDevice::isValidCharacteristic(
    const String& characteristicInstanceId) {
  return m_attributeInstanceMap->containsCharacteristic(
      characteristicInstanceId);
}

void BluetoothDevice::dispose() {
  disconnectGATTIfConnected();
}

void BluetoothDevice::contextDestroyed() {
  disconnectGATTIfConnected();
}

void BluetoothDevice::disconnectGATTIfConnected() {
  if (m_gatt->connected()) {
    m_gatt->setConnected(false);
    m_gatt->ClearActiveAlgorithms();
    m_bluetooth->removeDevice(id());
    mojom::blink::WebBluetoothService* service = m_bluetooth->service();
    auto deviceId = mojom::blink::WebBluetoothDeviceId::New();
    deviceId->device_id = id();
    service->RemoteServerDisconnect(std::move(deviceId));
  }
}

void BluetoothDevice::cleanupDisconnectedDeviceAndFireEvent() {
  DCHECK(m_gatt->connected());
  m_gatt->setConnected(false);
  m_gatt->ClearActiveAlgorithms();
  m_attributeInstanceMap->Clear();
  dispatchEvent(Event::createBubble(EventTypeNames::gattserverdisconnected));
}

const WTF::AtomicString& BluetoothDevice::interfaceName() const {
  return EventTargetNames::BluetoothDevice;
}

ExecutionContext* BluetoothDevice::getExecutionContext() const {
  return ContextLifecycleObserver::getExecutionContext();
}

void BluetoothDevice::dispatchGattServerDisconnected() {
  if (!m_gatt->connected()) {
    return;
  }
  cleanupDisconnectedDeviceAndFireEvent();
}

DEFINE_TRACE(BluetoothDevice) {
  EventTargetWithInlineData::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
  visitor->trace(m_attributeInstanceMap);
  visitor->trace(m_gatt);
  visitor->trace(m_bluetooth);
}

}  // namespace blink
