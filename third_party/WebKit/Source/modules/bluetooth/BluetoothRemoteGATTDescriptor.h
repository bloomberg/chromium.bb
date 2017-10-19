// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothRemoteGATTDescriptor_h
#define BluetoothRemoteGATTDescriptor_h

#include <memory>
#include "core/typed_arrays/DOMArrayPiece.h"
#include "core/typed_arrays/DOMDataView.h"
#include "modules/EventTargetModules.h"
#include "modules/bluetooth/Bluetooth.h"
#include "modules/bluetooth/BluetoothRemoteGATTCharacteristic.h"
#include "modules/bluetooth/BluetoothRemoteGATTService.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class BluetoothRemoteGATTCharacteristic;
class ScriptPromise;
class ScriptState;

// BluetoothRemoteGATTDescriptor represents a GATT Descriptor, which is
// a basic data element that provides further information about a peripheral's
// characteristic.
class BluetoothRemoteGATTDescriptor final
    : public GarbageCollectedFinalized<BluetoothRemoteGATTDescriptor>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  BluetoothRemoteGATTDescriptor(
      mojom::blink::WebBluetoothRemoteGATTDescriptorPtr,
      BluetoothRemoteGATTCharacteristic*);

  static BluetoothRemoteGATTDescriptor* Create(
      mojom::blink::WebBluetoothRemoteGATTDescriptorPtr,
      BluetoothRemoteGATTCharacteristic*);

  // IDL exposed interface:
  BluetoothRemoteGATTCharacteristic* characteristic() {
    return characteristic_;
  }
  String uuid() { return descriptor_->uuid; }
  DOMDataView* value() const { return value_; }
  ScriptPromise readValue(ScriptState*);
  ScriptPromise writeValue(ScriptState*, const DOMArrayPiece&);

  // Interface required by garbage collection.
  virtual void Trace(blink::Visitor*);

 private:
  friend class DescriptorReadValueCallback;

  BluetoothRemoteGATTServer* GetGatt() { return characteristic_->GetGatt(); }
  mojom::blink::WebBluetoothService* GetService() {
    return characteristic_->device_->GetBluetooth()->Service();
  }

  void ReadValueCallback(ScriptPromiseResolver*,
                         mojom::blink::WebBluetoothResult,
                         const Optional<Vector<uint8_t>>&);

  void WriteValueCallback(ScriptPromiseResolver*,
                          const Vector<uint8_t>&,
                          mojom::blink::WebBluetoothResult);

  DOMException* CreateInvalidDescriptorError();

  mojom::blink::WebBluetoothRemoteGATTDescriptorPtr descriptor_;
  Member<BluetoothRemoteGATTCharacteristic> characteristic_;
  Member<DOMDataView> value_;
};

}  // namespace blink

#endif  // BluetoothRemoteGATTDescriptor_h
