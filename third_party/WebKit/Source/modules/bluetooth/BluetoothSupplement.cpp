// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/bluetooth/BluetoothSupplement.h"

#include "core/frame/LocalDOMWindow.h"
#include "public/platform/Platform.h"

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

WebBluetooth* BluetoothSupplement::from(ScriptState* scriptState)
{
    LocalDOMWindow* window = scriptState->domWindow();
    if (window && window->frame()) {
        BluetoothSupplement* supplement = static_cast<BluetoothSupplement*>(WillBeHeapSupplement<LocalFrame>::from(window->frame(), supplementName()));
        if (supplement && supplement->m_bluetooth)
            return supplement->m_bluetooth;
    }
    return Platform::current()->bluetooth();
}

DEFINE_TRACE(BluetoothSupplement)
{
    WillBeHeapSupplement<LocalFrame>::trace(visitor);
}

}; // blink
