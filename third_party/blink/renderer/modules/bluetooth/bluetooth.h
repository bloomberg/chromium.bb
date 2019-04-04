// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_H_

#include <memory>
#include "third_party/blink/public/platform/modules/bluetooth/web_bluetooth.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_device.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class BluetoothLEScanOptions;
class RequestDeviceOptions;
class ScriptPromise;
class ScriptState;

class Bluetooth final : public EventTargetWithInlineData,
                        public ContextLifecycleObserver,
                        public mojom::blink::WebBluetoothScanClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Bluetooth);

 public:
  explicit Bluetooth(ExecutionContext*);
  ~Bluetooth() override;

  // IDL exposed interface:
  ScriptPromise requestDevice(ScriptState*,
                              const RequestDeviceOptions*,
                              ExceptionState&);

  ScriptPromise requestLEScan(ScriptState*,
                              const BluetoothLEScanOptions*,
                              ExceptionState&);

  mojom::blink::WebBluetoothService* Service() { return service_.get(); }

  // mojom::blink::WebBluetoothScanClient:
  void ScanEvent(mojom::blink::WebBluetoothScanResultPtr result) override;

  // EventTarget methods:
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // Interface required by Garbage Collection:
  void Trace(blink::Visitor*) override;

  // ContextLifecycleObserver interface.
  void ContextDestroyed(ExecutionContext*) override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(advertisementreceived, kAdvertisementreceived)

  void CancelScan(mojo::BindingId);

 private:
  BluetoothDevice* GetBluetoothDeviceRepresentingDevice(
      mojom::blink::WebBluetoothDevicePtr,
      ExecutionContext*);

  void RequestDeviceCallback(ScriptPromiseResolver*,
                             mojom::blink::WebBluetoothResult,
                             mojom::blink::WebBluetoothDevicePtr);

  void RequestScanningCallback(ScriptPromiseResolver*,
                               mojo::BindingId id,
                               mojom::blink::RequestScanningStartResultPtr);

  // Map of device ids to BluetoothDevice objects.
  // Ensures only one BluetoothDevice instance represents each
  // Bluetooth device inside a single global object.
  HeapHashMap<String, Member<BluetoothDevice>> device_instance_map_;

  mojo::AssociatedBindingSet<mojom::blink::WebBluetoothScanClient>
      client_bindings_;

  mojom::blink::WebBluetoothServicePtr service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_H_
