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
                                 mojom::blink::WebBluetoothDevicePtr device,
                                 Bluetooth* bluetooth)
    : ContextLifecycleObserver(context),
      m_attributeInstanceMap(new BluetoothAttributeInstanceMap(this)),
      m_device(std::move(device)),
      m_gatt(BluetoothRemoteGATTServer::Create(context, this)),
      m_bluetooth(bluetooth) {}

// static
BluetoothDevice* BluetoothDevice::take(
    ScriptPromiseResolver* resolver,
    mojom::blink::WebBluetoothDevicePtr device,
    Bluetooth* bluetooth) {
  return new BluetoothDevice(resolver->getExecutionContext(), std::move(device),
                             bluetooth);
}

BluetoothRemoteGATTService* BluetoothDevice::GetOrCreateRemoteGATTService(
    mojom::blink::WebBluetoothRemoteGATTServicePtr service,
    bool isPrimary,
    const String& deviceInstanceId) {
  return m_attributeInstanceMap->GetOrCreateRemoteGATTService(
      std::move(service), isPrimary, deviceInstanceId);
}

bool BluetoothDevice::IsValidService(const String& serviceInstanceId) {
  return m_attributeInstanceMap->ContainsService(serviceInstanceId);
}

BluetoothRemoteGATTCharacteristic*
BluetoothDevice::GetOrCreateRemoteGATTCharacteristic(
    ExecutionContext* context,
    mojom::blink::WebBluetoothRemoteGATTCharacteristicPtr characteristic,
    BluetoothRemoteGATTService* service) {
  return m_attributeInstanceMap->GetOrCreateRemoteGATTCharacteristic(
      context, std::move(characteristic), service);
}

bool BluetoothDevice::IsValidCharacteristic(
    const String& characteristicInstanceId) {
  return m_attributeInstanceMap->ContainsCharacteristic(
      characteristicInstanceId);
}

BluetoothRemoteGATTDescriptor*
BluetoothDevice::GetOrCreateBluetoothRemoteGATTDescriptor(
    mojom::blink::WebBluetoothRemoteGATTDescriptorPtr descriptor,
    BluetoothRemoteGATTCharacteristic* characteristic) {
  return m_attributeInstanceMap->GetOrCreateBluetoothRemoteGATTDescriptor(
      std::move(descriptor), characteristic);
}

bool BluetoothDevice::IsValidDescriptor(const String& descriptorInstanceId) {
  return m_attributeInstanceMap->ContainsDescriptor(descriptorInstanceId);
}

void BluetoothDevice::ClearAttributeInstanceMapAndFireEvent() {
  m_attributeInstanceMap->Clear();
  dispatchEvent(Event::createBubble(EventTypeNames::gattserverdisconnected));
}

const WTF::AtomicString& BluetoothDevice::interfaceName() const {
  return EventTargetNames::BluetoothDevice;
}

ExecutionContext* BluetoothDevice::getExecutionContext() const {
  return ContextLifecycleObserver::getExecutionContext();
}

DEFINE_TRACE(BluetoothDevice) {
  visitor->trace(m_attributeInstanceMap);
  visitor->trace(m_gatt);
  visitor->trace(m_bluetooth);
  EventTargetWithInlineData::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink
