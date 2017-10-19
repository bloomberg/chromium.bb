// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothRemoteGATTService_h
#define BluetoothRemoteGATTService_h

#include <memory>
#include "bindings/modules/v8/string_or_unsigned_long.h"
#include "modules/bluetooth/BluetoothDevice.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/bluetooth/web_bluetooth.mojom-blink.h"

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
                             bool is_primary,
                             const String& device_instance_id,
                             BluetoothDevice*);

  // Interface required by garbage collection.
  virtual void Trace(blink::Visitor*);

  // IDL exposed interface:
  String uuid() { return service_->uuid; }
  bool isPrimary() { return is_primary_; }
  BluetoothDevice* device() { return device_; }
  ScriptPromise getCharacteristic(ScriptState*,
                                  const StringOrUnsignedLong& characteristic,
                                  ExceptionState&);
  ScriptPromise getCharacteristics(ScriptState*,
                                   const StringOrUnsignedLong& characteristic,
                                   ExceptionState&);
  ScriptPromise getCharacteristics(ScriptState*, ExceptionState&);

 private:
  void GetCharacteristicsCallback(
      const String& service_instance_id,
      const String& requested_characteristic_uuid,
      mojom::blink::WebBluetoothGATTQueryQuantity,
      ScriptPromiseResolver*,
      mojom::blink::WebBluetoothResult,
      Optional<Vector<mojom::blink::WebBluetoothRemoteGATTCharacteristicPtr>>
          characteristics);

  ScriptPromise GetCharacteristicsImpl(
      ScriptState*,
      mojom::blink::WebBluetoothGATTQueryQuantity,
      const String& characteristic_uuid = String());

  mojom::blink::WebBluetoothRemoteGATTServicePtr service_;
  const bool is_primary_;
  const String device_instance_id_;
  Member<BluetoothDevice> device_;
};

}  // namespace blink

#endif  // BluetoothRemoteGATTService_h
