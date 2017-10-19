// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothRemoteGATTServer_h
#define BluetoothRemoteGATTServer_h

#include "bindings/modules/v8/string_or_unsigned_long.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "modules/bluetooth/BluetoothDevice.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Heap.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/bluetooth/web_bluetooth.mojom-blink.h"

namespace blink {

class BluetoothDevice;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

// BluetoothRemoteGATTServer provides a way to interact with a connected
// bluetooth peripheral.
class BluetoothRemoteGATTServer
    : public GarbageCollectedFinalized<BluetoothRemoteGATTServer>,
      public ScriptWrappable,
      public ContextLifecycleObserver,
      public mojom::blink::WebBluetoothServerClient {
  USING_PRE_FINALIZER(BluetoothRemoteGATTServer, Dispose);
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(BluetoothRemoteGATTServer);

 public:
  BluetoothRemoteGATTServer(ExecutionContext*, BluetoothDevice*);

  static BluetoothRemoteGATTServer* Create(ExecutionContext*, BluetoothDevice*);

  // ContextLifecycleObserver:
  void ContextDestroyed(ExecutionContext*) override;

  // mojom::blink::WebBluetoothServerClient:
  void GATTServerDisconnected() override;

  void SetConnected(bool connected) { connected_ = connected; }

  // The Active Algorithms set is maintained so that disconnection, i.e.
  // disconnect() method or the device disconnecting by itself, can be detected
  // by algorithms. They check via RemoveFromActiveAlgorithms that their
  // resolvers is still in the set of active algorithms.
  //
  // Adds |resolver| to the set of Active Algorithms. CHECK-fails if
  // |resolver| was already added.
  void AddToActiveAlgorithms(ScriptPromiseResolver*);
  // Removes |resolver| from the set of Active Algorithms if it was in the set
  // and returns true, false otherwise.
  bool RemoveFromActiveAlgorithms(ScriptPromiseResolver*);
  // Removes all ScriptPromiseResolvers from the set of Active Algorithms.
  void ClearActiveAlgorithms() { active_algorithms_.clear(); }

  // If gatt is connected then sets gatt.connected to false and disconnects.
  // This function only performs the necessary steps to ensure a device
  // disconnects therefore it should only be used when the object is being
  // garbage collected or the context is being destroyed.
  void DisconnectIfConnected();

  // Performs necessary cleanup when a device disconnects and fires
  // gattserverdisconnected event.
  void CleanupDisconnectedDeviceAndFireEvent();

  void DispatchDisconnected();

  // USING_PRE_FINALIZER interface.
  // Called before the object gets garbage collected.
  void Dispose();

  // Interface required by Garbage Collectoin:
  virtual void Trace(blink::Visitor*);

  // IDL exposed interface:
  BluetoothDevice* device() { return device_; }
  bool connected() { return connected_; }
  ScriptPromise connect(ScriptState*);
  void disconnect(ScriptState*);
  ScriptPromise getPrimaryService(ScriptState*,
                                  const StringOrUnsignedLong& service,
                                  ExceptionState&);
  ScriptPromise getPrimaryServices(ScriptState*,
                                   const StringOrUnsignedLong& service,
                                   ExceptionState&);
  ScriptPromise getPrimaryServices(ScriptState*, ExceptionState&);

 private:
  ScriptPromise GetPrimaryServicesImpl(
      ScriptState*,
      mojom::blink::WebBluetoothGATTQueryQuantity,
      String service_uuid = String());

  void ConnectCallback(ScriptPromiseResolver*,
                       mojom::blink::WebBluetoothResult);
  void GetPrimaryServicesCallback(
      const String& requested_service_uuid,
      mojom::blink::WebBluetoothGATTQueryQuantity,
      ScriptPromiseResolver*,
      mojom::blink::WebBluetoothResult,
      Optional<Vector<mojom::blink::WebBluetoothRemoteGATTServicePtr>>
          services);

  // Contains a ScriptPromiseResolver corresponding to each active algorithm
  // using this server’s connection.
  HeapHashSet<Member<ScriptPromiseResolver>> active_algorithms_;

  mojo::AssociatedBindingSet<mojom::blink::WebBluetoothServerClient>
      client_bindings_;

  Member<BluetoothDevice> device_;
  bool connected_;
};

}  // namespace blink

#endif  // BluetoothDevice_h
