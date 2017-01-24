// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Bluetooth_h
#define Bluetooth_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/bluetooth/BluetoothDevice.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/bluetooth/web_bluetooth.mojom-blink.h"
#include <memory>

namespace blink {

class BluetoothRemoteGATTCharacteristic;
class RequestDeviceOptions;
class ScriptPromise;
class ScriptState;

class Bluetooth : public GarbageCollectedFinalized<Bluetooth>,
                  public ScriptWrappable,
                  public mojom::blink::WebBluetoothServiceClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(Bluetooth, dispose);

 public:
  static Bluetooth* create() { return new Bluetooth(); }

  void dispose();

  // BluetoothDiscovery interface
  ScriptPromise requestDevice(ScriptState*,
                              const RequestDeviceOptions&,
                              ExceptionState&);

  mojom::blink::WebBluetoothService* service() { return m_service.get(); }

  void addToConnectedDevicesMap(const String& deviceId, BluetoothDevice*);

  void removeFromConnectedDevicesMap(const String& deviceId);

  void registerCharacteristicObject(const String& characteristicInstanceId,
                                    BluetoothRemoteGATTCharacteristic*);
  void characteristicObjectRemoved(const String& characteristicInstanceId);

  // Interface required by Garbage Collection:
  DECLARE_VIRTUAL_TRACE();

 private:
  Bluetooth();

  // mojom::blink::WebBluetoothServiceClient:
  void RemoteCharacteristicValueChanged(
      const WTF::String& characteristicInstanceId,
      const WTF::Vector<uint8_t>& value) override;
  void GattServerDisconnected(const WTF::String& deviceId) override;

  BluetoothDevice* getBluetoothDeviceRepresentingDevice(
      mojom::blink::WebBluetoothDevicePtr,
      ScriptPromiseResolver*);

  void RequestDeviceCallback(ScriptPromiseResolver*,
                             mojom::blink::WebBluetoothResult,
                             mojom::blink::WebBluetoothDevicePtr);

  // Map of device ids to BluetoothDevice objects.
  // Ensures only one BluetoothDevice instance represents each
  // Bluetooth device inside a single global object.
  HeapHashMap<String, Member<BluetoothDevice>> m_deviceInstanceMap;

  // Map of characteristic instance ids to BluetoothRemoteGATTCharacteristic.
  // When characteristicObjectRemoved is called the characteristic should be
  // removed from the map. Keeps track of what characteristics have listeners.
  HeapHashMap<String, Member<BluetoothRemoteGATTCharacteristic>>
      m_activeCharacteristics;

  // Map of device ids to BluetoothDevice. Added in
  // BluetoothRemoteGATTServer::connect() and removed in
  // BluetoothRemoteGATTServer::disconnect(). This means a device may not
  // actually be connected while in this map, but that it will definitely be
  // removed when the page navigates.
  HeapHashMap<String, Member<BluetoothDevice>> m_connectedDevices;

  mojom::blink::WebBluetoothServicePtr m_service;

  // Binding associated with |m_service|.
  mojo::AssociatedBinding<mojom::blink::WebBluetoothServiceClient>
      m_clientBinding;
};

}  // namespace blink

#endif  // Bluetooth_h
