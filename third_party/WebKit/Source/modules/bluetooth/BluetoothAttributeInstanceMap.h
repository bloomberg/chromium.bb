// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothAttributeInstanceMap_h
#define BluetoothAttributeInstanceMap_h

#include "modules/bluetooth/BluetoothRemoteGATTService.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include <memory>

namespace blink {

class BluetoothDevice;
class ScriptPromiseResolver;
struct WebBluetoothRemoteGATTService;

// Map that holds all GATT attributes, i.e. BluetoothRemoteGATTService,
// BluetoothRemoteGATTCharacteristic, BluetoothRemoteGATTDescriptor, for
// the BluetoothDevice passed in when constructing the object.
// Methods in this map are used to create or retrieve these attributes.
class BluetoothAttributeInstanceMap final
    : public GarbageCollected<BluetoothAttributeInstanceMap> {
 public:
  BluetoothAttributeInstanceMap(BluetoothDevice*);

  BluetoothRemoteGATTService* getOrCreateBluetoothRemoteGATTService(
      std::unique_ptr<WebBluetoothRemoteGATTService>);

  // TODO(crbug.com/654950): Implement methods to retrieve all attributes
  // associated with the device.

  DECLARE_VIRTUAL_TRACE();

 private:
  // BluetoothDevice that owns this map.
  Member<BluetoothDevice> m_device;
  // Map of service instance ids to objects.
  HeapHashMap<String, Member<BluetoothRemoteGATTService>> m_serviceIdToObject;
};

}  // namespace blink

#endif  // BluetoothAttributeInstanceMap_h
