// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothGATTCharacteristic_h
#define BluetoothGATTCharacteristic_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/DOMArrayPiece.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/bluetooth/WebBluetoothGATTCharacteristic.h"
#include "public/platform/modules/bluetooth/WebBluetoothGATTCharacteristicInit.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

// BluetoothGATTCharacteristic represents a GATT Characteristic, which is a
// basic data element that provides further information about a peripheral's
// service.
//
// Callbacks providing WebBluetoothGATTCharacteristicInit objects are handled by
// CallbackPromiseAdapter templatized with this class. See this class's
// "Interface required by CallbackPromiseAdapter" section and the
// CallbackPromiseAdapter class comments.
class BluetoothGATTCharacteristic final
    : public GarbageCollectedFinalized<BluetoothGATTCharacteristic>
    , public ScriptWrappable
    , public ActiveDOMObject
    , public WebBluetoothGATTCharacteristic {
    USING_PRE_FINALIZER(BluetoothGATTCharacteristic, dispose);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(BluetoothGATTCharacteristic);
    DEFINE_WRAPPERTYPEINFO();
public:
    explicit BluetoothGATTCharacteristic(ExecutionContext*, PassOwnPtr<WebBluetoothGATTCharacteristicInit>);

    // Interface required by CallbackPromiseAdapter.
    using WebType = OwnPtr<WebBluetoothGATTCharacteristicInit>;
    static BluetoothGATTCharacteristic* take(ScriptPromiseResolver*, PassOwnPtr<WebBluetoothGATTCharacteristicInit>);

    // ActiveDOMObject interface.
    void stop() override;

    // USING_PRE_FINALIZER interface.
    // Called before the object gets garbage collected.
    void dispose();

    // Notify our embedder that we should stop any notifications.
    // The function only notifies the embedder once.
    void notifyCharacteristicObjectRemoved();

    // Interface required by garbage collection.
    DECLARE_VIRTUAL_TRACE();

    // IDL exposed interface:
    String uuid() { return m_webCharacteristic->uuid; }
    ScriptPromise readValue(ScriptState*);
    ScriptPromise writeValue(ScriptState*, const DOMArrayPiece&);
    ScriptPromise startNotifications(ScriptState*);
    ScriptPromise stopNotifications(ScriptState*);

private:
    OwnPtr<WebBluetoothGATTCharacteristicInit> m_webCharacteristic;
    bool m_stopped;
};

} // namespace blink

#endif // BluetoothGATTCharacteristic_h
