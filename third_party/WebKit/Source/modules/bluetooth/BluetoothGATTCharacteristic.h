// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothGATTCharacteristic_h
#define BluetoothGATTCharacteristic_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/DOMArrayPiece.h"
#include "modules/EventTargetModules.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/bluetooth/WebBluetoothGATTCharacteristic.h"
#include "public/platform/modules/bluetooth/WebBluetoothGATTCharacteristicInit.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class BluetoothCharacteristicProperties;
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
    : public RefCountedGarbageCollectedEventTargetWithInlineData<BluetoothGATTCharacteristic>
    , public ActiveDOMObject
    , public WebBluetoothGATTCharacteristic {
    USING_PRE_FINALIZER(BluetoothGATTCharacteristic, dispose);
    DEFINE_WRAPPERTYPEINFO();
    REFCOUNTED_GARBAGE_COLLECTED_EVENT_TARGET(BluetoothGATTCharacteristic);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(BluetoothGATTCharacteristic);
public:
    explicit BluetoothGATTCharacteristic(ExecutionContext*, PassOwnPtr<WebBluetoothGATTCharacteristicInit>);

    // Interface required by CallbackPromiseAdapter.
    using WebType = OwnPtr<WebBluetoothGATTCharacteristicInit>;
    static BluetoothGATTCharacteristic* take(ScriptPromiseResolver*, PassOwnPtr<WebBluetoothGATTCharacteristicInit>);

    // WebBluetoothGATTCharacteristic interface:
    void dispatchCharacteristicValueChanged(const WebVector<uint8_t>& value) override;

    // ActiveDOMObject interface.
    void stop() override;

    // USING_PRE_FINALIZER interface.
    // Called before the object gets garbage collected.
    void dispose();

    // Notify our embedder that we should stop any notifications.
    // The function only notifies the embedder once.
    void notifyCharacteristicObjectRemoved();

    // EventTarget methods:
    const AtomicString& interfaceName() const override;
    ExecutionContext* executionContext() const;

    // Interface required by garbage collection.
    DECLARE_VIRTUAL_TRACE();

    // IDL exposed interface:
    String uuid() { return m_webCharacteristic->uuid; }

    BluetoothCharacteristicProperties* properties() { return m_properties; }
    PassRefPtr<DOMArrayBuffer> value() const { return m_value; }
    ScriptPromise readValue(ScriptState*);
    ScriptPromise writeValue(ScriptState*, const DOMArrayPiece&);
    ScriptPromise startNotifications(ScriptState*);
    ScriptPromise stopNotifications(ScriptState*);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(characteristicvaluechanged);

protected:
    // EventTarget overrides.
    bool addEventListenerInternal(const AtomicString& eventType, PassRefPtrWillBeRawPtr<EventListener>, const EventListenerOptions&) override;

private:
    OwnPtr<WebBluetoothGATTCharacteristicInit> m_webCharacteristic;
    bool m_stopped;
    Member<BluetoothCharacteristicProperties> m_properties;
    RefPtr<DOMArrayBuffer> m_value;
};

} // namespace blink

#endif // BluetoothGATTCharacteristic_h
