// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothRemoteGATTDescriptor_h
#define BluetoothRemoteGATTDescriptor_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMArrayPiece.h"
#include "core/dom/DOMDataView.h"
#include "modules/EventTargetModules.h"
#include "modules/bluetooth/Bluetooth.h"
#include "modules/bluetooth/BluetoothRemoteGATTCharacteristic.h"
#include "modules/bluetooth/BluetoothRemoteGATTService.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"
#include <memory>

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

  static BluetoothRemoteGATTDescriptor* create(
      mojom::blink::WebBluetoothRemoteGATTDescriptorPtr,
      BluetoothRemoteGATTCharacteristic*);

  // IDL exposed interface:
  BluetoothRemoteGATTCharacteristic* characteristic() {
    return m_characteristic;
  }
  String uuid() { return m_descriptor->uuid; }
  DOMDataView* value() const { return m_value; }
  ScriptPromise readValue(ScriptState*);
  ScriptPromise writeValue(ScriptState*, const DOMArrayPiece&);

  // Interface required by garbage collection.
  DECLARE_VIRTUAL_TRACE();

 private:
  friend class DescriptorReadValueCallback;

  BluetoothRemoteGATTServer* getGatt() { return m_characteristic->getGatt(); }
  mojom::blink::WebBluetoothService* getService() {
    return m_characteristic->m_device->bluetooth()->service();
  }

  void ReadValueCallback(ScriptPromiseResolver*,
                         mojom::blink::WebBluetoothResult,
                         const Optional<Vector<uint8_t>>&);

  void WriteValueCallback(ScriptPromiseResolver*,
                          const Vector<uint8_t>&,
                          mojom::blink::WebBluetoothResult);

  DOMException* createInvalidDescriptorError();

  mojom::blink::WebBluetoothRemoteGATTDescriptorPtr m_descriptor;
  Member<BluetoothRemoteGATTCharacteristic> m_characteristic;
  Member<DOMDataView> m_value;
};

}  // namespace blink

#endif  // BluetoothRemoteGATTDescriptor_h
