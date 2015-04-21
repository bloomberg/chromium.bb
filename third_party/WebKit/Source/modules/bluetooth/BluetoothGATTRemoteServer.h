// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothGATTRemoteServer_h
#define BluetoothGATTRemoteServer_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Heap.h"
#include "public/platform/modules/bluetooth/WebBluetoothGATTRemoteServer.h"

namespace blink {

class ScriptPromiseResolver;

// BluetoothGATTRemoteServer provides a way to interact with a connected bluetooth peripheral.
//
// Callbacks providing WebBluetoothGATTRemoteServer objects are handled by
// CallbackPromiseAdapter templatized with this class. See this class's
// "Interface required by CallbackPromiseAdapter" section and the
// CallbackPromiseAdapter class comments.
class BluetoothGATTRemoteServer final
    : public GarbageCollectedFinalized<BluetoothGATTRemoteServer>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    BluetoothGATTRemoteServer(const WebBluetoothGATTRemoteServer&);

    // Interface required by CallbackPromiseAdapter:
    typedef WebBluetoothGATTRemoteServer WebType;
    static BluetoothGATTRemoteServer* take(ScriptPromiseResolver*, WebBluetoothGATTRemoteServer*);
    static void dispose(WebBluetoothGATTRemoteServer*);

    // Interface required by Garbage Collectoin:
    DEFINE_INLINE_TRACE() { }

    // IDL exposed interface:
    bool connected() { return m_webGATT.connected; }

private:
    WebBluetoothGATTRemoteServer m_webGATT;
};

} // namespace blink

#endif // BluetoothDevice_h
