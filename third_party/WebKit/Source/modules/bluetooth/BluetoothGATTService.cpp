// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/bluetooth/BluetoothGATTService.h"

#include "public/platform/modules/bluetooth/WebBluetoothGATTRemoteServer.h"
#include "wtf/OwnPtr.h"

namespace blink {

BluetoothGATTService::BluetoothGATTService(const WebBluetoothGATTService& webService)
    : m_webService(webService)
{
}

BluetoothGATTService* BluetoothGATTService::take(ScriptPromiseResolver*, WebBluetoothGATTService* webServiceRawPointer)
{
    if (!webServiceRawPointer) {
        return nullptr;
    }
    OwnPtr<WebBluetoothGATTService> webService = adoptPtr(webServiceRawPointer);
    return new BluetoothGATTService(*webService);
}

void BluetoothGATTService::dispose(WebBluetoothGATTService* webService)
{
    delete webService;
}

} // namespace blink
