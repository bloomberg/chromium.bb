// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothGATTRemoteServer_h
#define BluetoothGATTRemoteServer_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/modules/v8/UnionTypesModules.h"
#include "platform/heap/Heap.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

// BluetoothGATTRemoteServer provides a way to interact with a connected bluetooth peripheral.
class BluetoothGATTRemoteServer final
    : public GarbageCollectedFinalized<BluetoothGATTRemoteServer>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    BluetoothGATTRemoteServer(const String& deviceId);

    static BluetoothGATTRemoteServer* create(const String& deviceId);

    void disconnectIfConnected(ExecutionContext*);
    void setConnected(bool connected) { m_connected = connected; }

    // Interface required by Garbage Collectoin:
    DEFINE_INLINE_TRACE() { }

    // IDL exposed interface:
    bool connected() { return m_connected; }
    ScriptPromise connect(ScriptState*);
    void disconnect(ScriptState*);
    ScriptPromise getPrimaryService(ScriptState*, const StringOrUnsignedLong& service, ExceptionState&);

private:
    String m_deviceId;
    bool m_connected;
};

} // namespace blink

#endif // BluetoothDevice_h
