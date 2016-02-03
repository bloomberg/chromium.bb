// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothDevice_h
#define BluetoothDevice_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/bluetooth/BluetoothAdvertisingData.h"
#include "modules/bluetooth/BluetoothGATTRemoteServer.h"
#include "platform/heap/Heap.h"
#include "public/platform/modules/bluetooth/WebBluetoothDevice.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

// BluetoothDevice represents a physical bluetooth device in the DOM. See IDL.
//
// Callbacks providing WebBluetoothDevice objects are handled by
// CallbackPromiseAdapter templatized with this class. See this class's
// "Interface required by CallbackPromiseAdapter" section and the
// CallbackPromiseAdapter class comments.
class BluetoothDevice final
    : public GarbageCollectedFinalized<BluetoothDevice>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    BluetoothDevice(ExecutionContext*, PassOwnPtr<WebBluetoothDevice>);

    // Interface required by CallbackPromiseAdapter:
    using WebType = OwnPtr<WebBluetoothDevice>;
    static BluetoothDevice* take(ScriptPromiseResolver*, PassOwnPtr<WebBluetoothDevice>);

    // Interface required by Garbage Collection:
    DECLARE_VIRTUAL_TRACE();

    // IDL exposed interface:
    String id() { return m_webDevice->id; }
    String name() { return m_webDevice->name; }
    BluetoothAdvertisingData* adData() { return m_adData; }
    unsigned deviceClass(bool& isNull);
    String vendorIDSource();
    unsigned vendorID(bool& isNull);
    unsigned productID(bool& isNull);
    unsigned productVersion(bool& isNull);
    BluetoothGATTRemoteServer* gatt() { return m_gatt; }
    Vector<String> uuids();
    // TODO(ortuno): Remove connectGATT
    // http://crbug.com/582292
    ScriptPromise connectGATT(ScriptState*);

private:
    OwnPtr<WebBluetoothDevice> m_webDevice;
    Member<BluetoothAdvertisingData> m_adData;
    Member<BluetoothGATTRemoteServer> m_gatt;
};

} // namespace blink

#endif // BluetoothDevice_h
