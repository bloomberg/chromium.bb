// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothRemoteGATTServer_h
#define BluetoothRemoteGATTServer_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/modules/v8/StringOrUnsignedLong.h"
#include "modules/bluetooth/BluetoothDevice.h"
#include "platform/heap/Heap.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/web_bluetooth.mojom-blink.h"
#include "wtf/text/WTFString.h"

namespace blink {

class BluetoothDevice;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

// BluetoothRemoteGATTServer provides a way to interact with a connected
// bluetooth peripheral.
class BluetoothRemoteGATTServer final
    : public GarbageCollected<BluetoothRemoteGATTServer>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  BluetoothRemoteGATTServer(BluetoothDevice*);

  static BluetoothRemoteGATTServer* create(BluetoothDevice*);

  void setConnected(bool connected) { m_connected = connected; }

  // Adds |resolver| to the set of Active Algorithms. CHECK-fails if
  // |resolver| was already added.
  void AddToActiveAlgorithms(ScriptPromiseResolver*);
  // Returns false if |resolver| was not in the set of Active Algorithms.
  // Otherwise it removes |resolver| from the set of Active Algorithms and
  // returns true.
  bool RemoveFromActiveAlgorithms(ScriptPromiseResolver*);
  // Removes all ScriptPromiseResolvers from the set of Active Algorithms.
  void ClearActiveAlgorithms() { m_activeAlgorithms.clear(); }

  // Interface required by Garbage Collectoin:
  DECLARE_VIRTUAL_TRACE();

  // IDL exposed interface:
  BluetoothDevice* device() { return m_device; }
  bool connected() { return m_connected; }
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
  ScriptPromise getPrimaryServicesImpl(
      ScriptState*,
      mojom::blink::WebBluetoothGATTQueryQuantity,
      String serviceUUID = String());

  // Contains a ScriptPromiseResolver corresponding to each algorithm using
  // this server’s connection. Disconnection i.e. disconnect() method or the
  // device disconnecting by itself, empties this set so that the algorithm
  // can tell whether its realm was ever disconnected while it was running.
  HeapHashSet<Member<ScriptPromiseResolver>> m_activeAlgorithms;

  Member<BluetoothDevice> m_device;
  bool m_connected;
};

}  // namespace blink

#endif  // BluetoothDevice_h
