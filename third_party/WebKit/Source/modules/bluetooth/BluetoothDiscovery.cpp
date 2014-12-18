// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/bluetooth/BluetoothDiscovery.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/bluetooth/BluetoothDevice.h"
#include "modules/bluetooth/BluetoothError.h"
#include "public/platform/Platform.h"
#include "public/platform/WebBluetooth.h"

namespace blink {

ScriptPromise BluetoothDiscovery::requestDevice(ScriptState* scriptState)
{
    WebBluetooth* webbluetooth = Platform::current()->bluetooth();
    if (!webbluetooth)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));

    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    webbluetooth->requestDevice(new CallbackPromiseAdapter<BluetoothDevice, BluetoothError>(resolver));
    return promise;
}

} // namespace blink
