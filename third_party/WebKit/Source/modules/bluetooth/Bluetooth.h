// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Bluetooth_h
#define Bluetooth_h

#include <memory>
#include "bindings/core/v8/ScriptPromise.h"
#include "modules/bluetooth/BluetoothDevice.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/bluetooth/web_bluetooth.mojom-blink.h"

namespace blink {

class RequestDeviceOptions;
class ScriptPromise;
class ScriptState;

class Bluetooth final : public GarbageCollectedFinalized<Bluetooth>,
                        public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Bluetooth* Create() { return new Bluetooth(); }

  // IDL exposed interface:
  ScriptPromise requestDevice(ScriptState*,
                              const RequestDeviceOptions&,
                              ExceptionState&);

  mojom::blink::WebBluetoothService* Service() { return service_.get(); }

  // Interface required by Garbage Collection:
  virtual void Trace(blink::Visitor*);

 private:
  Bluetooth();

  BluetoothDevice* GetBluetoothDeviceRepresentingDevice(
      mojom::blink::WebBluetoothDevicePtr,
      ScriptPromiseResolver*);

  void RequestDeviceCallback(ScriptPromiseResolver*,
                             mojom::blink::WebBluetoothResult,
                             mojom::blink::WebBluetoothDevicePtr);

  // Map of device ids to BluetoothDevice objects.
  // Ensures only one BluetoothDevice instance represents each
  // Bluetooth device inside a single global object.
  HeapHashMap<String, Member<BluetoothDevice>> device_instance_map_;

  mojom::blink::WebBluetoothServicePtr service_;
};

}  // namespace blink

#endif  // Bluetooth_h
