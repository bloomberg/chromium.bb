// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothRemoteGATTService_h
#define BluetoothRemoteGATTService_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/modules/v8/StringOrUnsignedLong.h"
#include "modules/bluetooth/BluetoothDevice.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include "public/platform/modules/bluetooth/web_bluetooth.mojom-blink.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class ScriptPromise;
class ScriptState;

// Represents a GATT Service within a Bluetooth Peripheral, a collection of
// characteristics and relationships to other services that encapsulate the
// behavior of part of a device.
//
// Callbacks providing WebBluetoothRemoteGATTService objects are handled by
// CallbackPromiseAdapter templatized with this class. See this class's
// "Interface required by CallbackPromiseAdapter" section and the
// CallbackPromiseAdapter class comments.
class BluetoothRemoteGATTService final
    : public GarbageCollectedFinalized<BluetoothRemoteGATTService>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  BluetoothRemoteGATTService(mojom::blink::WebBluetoothRemoteGATTServicePtr,
                             bool isPrimary,
                             const String& deviceInstanceId,
                             BluetoothDevice*);

  // Interface required by garbage collection.
  DECLARE_VIRTUAL_TRACE();

  // IDL exposed interface:
  String uuid() { return m_service->uuid; }
  bool isPrimary() { return m_isPrimary; }
  BluetoothDevice* device() { return m_device; }
  ScriptPromise getCharacteristic(ScriptState*,
                                  const StringOrUnsignedLong& characteristic,
                                  ExceptionState&);
  ScriptPromise getCharacteristics(ScriptState*,
                                   const StringOrUnsignedLong& characteristic,
                                   ExceptionState&);
  ScriptPromise getCharacteristics(ScriptState*, ExceptionState&);

 private:
  void GetCharacteristicsCallback(
      const String& serviceInstanceId,
      const String& requestedCharacteristicUUID,
      mojom::blink::WebBluetoothGATTQueryQuantity,
      ScriptPromiseResolver*,
      mojom::blink::WebBluetoothResult,
      Optional<Vector<mojom::blink::WebBluetoothRemoteGATTCharacteristicPtr>>
          characteristics);

  ScriptPromise getCharacteristicsImpl(
      ScriptState*,
      mojom::blink::WebBluetoothGATTQueryQuantity,
      const String& characteristicUUID = String());

  mojom::blink::WebBluetoothRemoteGATTServicePtr m_service;
  const bool m_isPrimary;
  const String m_deviceInstanceId;
  Member<BluetoothDevice> m_device;
};

}  // namespace blink

#endif  // BluetoothRemoteGATTService_h
