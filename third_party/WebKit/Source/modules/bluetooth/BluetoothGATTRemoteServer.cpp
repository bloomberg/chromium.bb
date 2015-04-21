// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/bluetooth/BluetoothGATTRemoteServer.h"

#include "public/platform/modules/bluetooth/WebBluetoothGATTRemoteServer.h"
#include "wtf/OwnPtr.h"

namespace blink {

BluetoothGATTRemoteServer::BluetoothGATTRemoteServer(const WebBluetoothGATTRemoteServer& webGATT)
    : m_webGATT(webGATT)
{
}

BluetoothGATTRemoteServer* BluetoothGATTRemoteServer::take(ScriptPromiseResolver*, WebBluetoothGATTRemoteServer* webGATTRawPointer)
{
    OwnPtr<WebBluetoothGATTRemoteServer> webGATT = adoptPtr(webGATTRawPointer);
    return new BluetoothGATTRemoteServer(*webGATT);
}

void BluetoothGATTRemoteServer::dispose(WebBluetoothGATTRemoteServer* webGATTRaw)
{
    delete webGATTRaw;
}


} // namespace blink
