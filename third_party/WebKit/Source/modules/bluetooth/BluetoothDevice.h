// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothDevice_h
#define BluetoothDevice_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "modules/EventTargetModules.h"
#include "modules/bluetooth/BluetoothRemoteGATTServer.h"
#include "platform/heap/Heap.h"
#include "public/platform/modules/bluetooth/web_bluetooth.mojom-blink.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class Bluetooth;
class BluetoothAttributeInstanceMap;
class BluetoothRemoteGATTCharacteristic;
class BluetoothRemoteGATTDescriptor;
class BluetoothRemoteGATTServer;
class BluetoothRemoteGATTService;
class ScriptPromiseResolver;

// BluetoothDevice represents a physical bluetooth device in the DOM. See IDL.
//
// Callbacks providing WebBluetoothDevice objects are handled by
// CallbackPromiseAdapter templatized with this class. See this class's
// "Interface required by CallbackPromiseAdapter" section and the
// CallbackPromiseAdapter class comments.
class BluetoothDevice final : public EventTargetWithInlineData,
                              public ContextLifecycleObserver {
  USING_PRE_FINALIZER(BluetoothDevice, dispose);
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(BluetoothDevice);

 public:
  BluetoothDevice(ExecutionContext*,
                  mojom::blink::WebBluetoothDevicePtr,
                  Bluetooth*);

  // Interface required by CallbackPromiseAdapter:
  static BluetoothDevice* take(ScriptPromiseResolver*,
                               mojom::blink::WebBluetoothDevicePtr,
                               Bluetooth*);

  BluetoothRemoteGATTService* getOrCreateRemoteGATTService(
      mojom::blink::WebBluetoothRemoteGATTServicePtr,
      bool isPrimary,
      const String& deviceInstanceId);
  bool isValidService(const String& serviceInstanceId);

  BluetoothRemoteGATTCharacteristic* getOrCreateRemoteGATTCharacteristic(
      ExecutionContext*,
      mojom::blink::WebBluetoothRemoteGATTCharacteristicPtr,
      BluetoothRemoteGATTService*);
  bool isValidCharacteristic(const String& characteristicInstanceId);

  BluetoothRemoteGATTDescriptor* getOrCreateBluetoothRemoteGATTDescriptor(
      mojom::blink::WebBluetoothRemoteGATTDescriptorPtr,
      BluetoothRemoteGATTCharacteristic*);
  bool isValidDescriptor(const String& descriptorInstanceId);

  // We should disconnect from the device in all of the following cases:
  // 1. When the object gets GarbageCollected e.g. it went out of scope.
  // dispose() is called in this case.
  // 2. When the parent document gets detached e.g. reloading a page.
  // stop() is called in this case.
  // TODO(ortuno): Users should be able to turn on notifications for
  // events on navigator.bluetooth and still remain connected even if the
  // BluetoothDevice object is garbage collected.

  // USING_PRE_FINALIZER interface.
  // Called before the object gets garbage collected.
  void dispose();

  // ContextLifecycleObserver interface.
  void contextDestroyed(ExecutionContext*) override;

  // If gatt is connected then sets gatt.connected to false and disconnects.
  // This function only performs the necessary steps to ensure a device
  // disconnects therefore it should only be used when the object is being
  // garbage collected or the context is being destroyed.
  void disconnectGATTIfConnected();

  // Performs necessary cleanup when a device disconnects and fires
  // gattserverdisconnected event.
  void cleanupDisconnectedDeviceAndFireEvent();

  // EventTarget methods:
  const AtomicString& interfaceName() const override;
  ExecutionContext* getExecutionContext() const override;

  void dispatchGattServerDisconnected();

  Bluetooth* bluetooth() { return m_bluetooth; }

  // Interface required by Garbage Collection:
  DECLARE_VIRTUAL_TRACE();

  // IDL exposed interface:
  String id() { return m_device->id; }
  String name() { return m_device->name; }
  BluetoothRemoteGATTServer* gatt() { return m_gatt; }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(gattserverdisconnected);

 private:
  // Holds all GATT Attributes associated with this BluetoothDevice.
  Member<BluetoothAttributeInstanceMap> m_attributeInstanceMap;

  mojom::blink::WebBluetoothDevicePtr m_device;
  Member<BluetoothRemoteGATTServer> m_gatt;
  Member<Bluetooth> m_bluetooth;
};

}  // namespace blink

#endif  // BluetoothDevice_h
