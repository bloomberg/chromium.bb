// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/bluetooth/BluetoothGATTRemoteServer.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothGATTService.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/bluetooth/WebBluetooth.h"
#include "wtf/OwnPtr.h"

namespace blink {

BluetoothGATTRemoteServer::BluetoothGATTRemoteServer(PassOwnPtr<WebBluetoothGATTRemoteServer> webGATT)
    : m_webGATT(webGATT)
{
}

BluetoothGATTRemoteServer* BluetoothGATTRemoteServer::take(ScriptPromiseResolver*, WebBluetoothGATTRemoteServer* webGATTRawPointer)
{
    return new BluetoothGATTRemoteServer(adoptPtr(webGATTRawPointer));
}

void BluetoothGATTRemoteServer::dispose(WebBluetoothGATTRemoteServer* webGATTRaw)
{
    delete webGATTRaw;
}

ScriptPromise BluetoothGATTRemoteServer::getPrimaryService(ScriptState* scriptState, String serviceUUID)
{
    // TODO(ortuno): BluetoothUUID.getService(serviceUUID)
    // https://crbug.com/491441
    WebBluetooth* webbluetooth = Platform::current()->bluetooth();

    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    webbluetooth->getPrimaryService(m_webGATT->deviceInstanceID, serviceUUID, new CallbackPromiseAdapter<BluetoothGATTService, BluetoothError>(resolver));

    return promise;
}

} // namespace blink
