// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothSupplement.h"

#include "core/dom/Document.h"

namespace blink {

const char* BluetoothSupplement::supplementName()
{
    return "BluetoothSupplement";
}

BluetoothSupplement::BluetoothSupplement(WebBluetooth* bluetooth)
    : m_bluetooth(bluetooth)
{
}

void BluetoothSupplement::provideTo(LocalFrame& frame, WebBluetooth* bluetooth)
{
    OwnPtrWillBeRawPtr<BluetoothSupplement> bluetoothSupplement = adoptPtrWillBeNoop(new BluetoothSupplement(bluetooth));
    WillBeHeapSupplement<LocalFrame>::provideTo(frame, supplementName(), bluetoothSupplement.release());
};

WebBluetooth* BluetoothSupplement::from(LocalFrame* frame)
{
    BluetoothSupplement* supplement = static_cast<BluetoothSupplement*>(WillBeHeapSupplement<LocalFrame>::from(frame, supplementName()));

    ASSERT(supplement);
    ASSERT(supplement->m_bluetooth);

    return supplement->m_bluetooth;
}

WebBluetooth* BluetoothSupplement::fromScriptState(ScriptState* scriptState)
{
    return fromExecutionContext(scriptState->executionContext());
}

WebBluetooth* BluetoothSupplement::fromExecutionContext(ExecutionContext* executionContext)
{
    if (!executionContext->isDocument()) {
        return nullptr;
    }
    return BluetoothSupplement::from(static_cast<Document*>(executionContext)->frame());
}

DEFINE_TRACE(BluetoothSupplement)
{
    WillBeHeapSupplement<LocalFrame>::trace(visitor);
}

}; // blink
