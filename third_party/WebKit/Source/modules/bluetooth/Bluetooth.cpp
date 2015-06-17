// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/bluetooth/Bluetooth.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/bluetooth/BluetoothDevice.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/RequestDeviceOptions.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/bluetooth/WebBluetooth.h"
#include "public/platform/modules/bluetooth/WebRequestDeviceOptions.h"

namespace blink {

// Returns a DOMException if the conversion fails, or null if it succeeds.
static DOMException* convertRequestDeviceOptions(const RequestDeviceOptions& options, WebRequestDeviceOptions& result)
{
    if (options.hasFilters()) {
        Vector<WebBluetoothScanFilter> filters;
        for (const BluetoothScanFilter& filter : options.filters()) {
            Vector<WebString> services;
            for (const String& service : filter.services()) {
                // TODO(jyasskin): https://crbug.com/500630: Pass the services
                // through BluetoothUUID.getService() per
                // https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetooth-requestdevice
                services.append(service);
            }
            filters.append(WebBluetoothScanFilter(services));
        }
        result.filters.assign(filters);
    }
    if (options.hasOptionalServices()) {
        Vector<WebString> optionalServices;
        for (const String& optionalService : options.optionalServices()) {
            // TODO(jyasskin): https://crbug.com/500630: Pass the services
            // through BluetoothUUID.getService() per
            // https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetooth-requestdevice
            optionalServices.append(optionalService);
        }
        result.optionalServices.assign(optionalServices);
    }
    return 0;
}

ScriptPromise Bluetooth::requestDevice(ScriptState* scriptState, const RequestDeviceOptions& options)
{
    WebBluetooth* webbluetooth = Platform::current()->bluetooth();
    if (!webbluetooth)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));

    WebRequestDeviceOptions webOptions;
    DOMException* exception = convertRequestDeviceOptions(options, webOptions);
    if (exception)
        return ScriptPromise::rejectWithDOMException(scriptState, exception);

    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    webbluetooth->requestDevice(webOptions, new CallbackPromiseAdapter<BluetoothDevice, BluetoothError>(resolver));
    return promise;

}

}; // blink
