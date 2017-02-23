// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothAttributeInstanceMap.h"

#include "modules/bluetooth/BluetoothDevice.h"
#include "modules/bluetooth/BluetoothRemoteGATTService.h"
#include <memory>
#include <utility>

namespace blink {

BluetoothAttributeInstanceMap::BluetoothAttributeInstanceMap(
    BluetoothDevice* device)
    : m_device(device) {}

BluetoothRemoteGATTService*
BluetoothAttributeInstanceMap::getOrCreateRemoteGATTService(
    mojom::blink::WebBluetoothRemoteGATTServicePtr remoteGATTService,
    bool isPrimary,
    const String& deviceInstanceId) {
  String serviceInstanceId = remoteGATTService->instance_id;
  BluetoothRemoteGATTService* service =
      m_serviceIdToObject.at(serviceInstanceId);

  if (!service) {
    service = new BluetoothRemoteGATTService(
        std::move(remoteGATTService), isPrimary, deviceInstanceId, m_device);
    m_serviceIdToObject.insert(serviceInstanceId, service);
  }

  return service;
}

bool BluetoothAttributeInstanceMap::containsService(
    const String& serviceInstanceId) {
  return m_serviceIdToObject.contains(serviceInstanceId);
}

BluetoothRemoteGATTCharacteristic*
BluetoothAttributeInstanceMap::getOrCreateRemoteGATTCharacteristic(
    ExecutionContext* context,
    mojom::blink::WebBluetoothRemoteGATTCharacteristicPtr
        remoteGATTCharacteristic,
    BluetoothRemoteGATTService* service) {
  String instanceId = remoteGATTCharacteristic->instance_id;
  BluetoothRemoteGATTCharacteristic* characteristic =
      m_characteristicIdToObject.at(instanceId);

  if (!characteristic) {
    characteristic = BluetoothRemoteGATTCharacteristic::create(
        context, std::move(remoteGATTCharacteristic), service, m_device);
    m_characteristicIdToObject.insert(instanceId, characteristic);
  }

  return characteristic;
}

bool BluetoothAttributeInstanceMap::containsCharacteristic(
    const String& characteristicInstanceId) {
  return m_characteristicIdToObject.contains(characteristicInstanceId);
}

BluetoothRemoteGATTDescriptor*
BluetoothAttributeInstanceMap::getOrCreateBluetoothRemoteGATTDescriptor(
    mojom::blink::WebBluetoothRemoteGATTDescriptorPtr descriptor,
    BluetoothRemoteGATTCharacteristic* characteristic) {
  String instanceId = descriptor->instance_id;
  BluetoothRemoteGATTDescriptor* result = m_descriptorIdToObject.at(instanceId);

  if (result)
    return result;

  result =
      new BluetoothRemoteGATTDescriptor(std::move(descriptor), characteristic);
  m_descriptorIdToObject.insert(instanceId, result);
  return result;
}

bool BluetoothAttributeInstanceMap::containsDescriptor(
    const String& descriptorInstanceId) {
  return m_descriptorIdToObject.contains(descriptorInstanceId);
}

void BluetoothAttributeInstanceMap::Clear() {
  m_serviceIdToObject.clear();
  m_characteristicIdToObject.clear();
  m_descriptorIdToObject.clear();
}

DEFINE_TRACE(BluetoothAttributeInstanceMap) {
  visitor->trace(m_device);
  visitor->trace(m_serviceIdToObject);
  visitor->trace(m_characteristicIdToObject);
  visitor->trace(m_descriptorIdToObject);
}

}  // namespace blink
