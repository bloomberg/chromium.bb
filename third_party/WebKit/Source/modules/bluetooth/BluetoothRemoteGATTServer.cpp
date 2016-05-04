// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothRemoteGATTServer.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothRemoteGATTService.h"
#include "modules/bluetooth/BluetoothSupplement.h"
#include "modules/bluetooth/BluetoothUUID.h"
#include "public/platform/modules/bluetooth/WebBluetooth.h"
#include "wtf/OwnPtr.h"

namespace blink {

BluetoothRemoteGATTServer::BluetoothRemoteGATTServer(BluetoothDevice* device)
    : m_device(device)
    , m_connected(false)
{
}

BluetoothRemoteGATTServer* BluetoothRemoteGATTServer::create(BluetoothDevice* device)
{
    return new BluetoothRemoteGATTServer(device);
}

DEFINE_TRACE(BluetoothRemoteGATTServer)
{
    visitor->trace(m_device);
}

class ConnectCallback : public WebBluetoothRemoteGATTServerConnectCallbacks {
public:
    ConnectCallback(BluetoothDevice* device, ScriptPromiseResolver* resolver)
        : m_device(device)
        , m_resolver(resolver) {}

    void onSuccess() override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        m_device->gatt()->setConnected(true);
        m_resolver->resolve(m_device->gatt());
    }

    void onError(const WebBluetoothError& e) override
    {
        if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
            return;
        m_resolver->reject(BluetoothError::take(m_resolver, e));
    }
private:
    Persistent<BluetoothDevice> m_device;
    Persistent<ScriptPromiseResolver> m_resolver;
};

ScriptPromise BluetoothRemoteGATTServer::connect(ScriptState* scriptState)
{
    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);
    if (!webbluetooth)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    webbluetooth->connect(device()->id(), new ConnectCallback(device(), resolver));
    return promise;
}

void BluetoothRemoteGATTServer::disconnect(ScriptState* scriptState)
{
    m_connected = false;
    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);
    webbluetooth->disconnect(device()->id());
}

ScriptPromise BluetoothRemoteGATTServer::getPrimaryService(ScriptState* scriptState, const StringOrUnsignedLong& service, ExceptionState& exceptionState)
{
#if OS(MACOSX)
    // TODO(jlebel): Remove when getPrimaryService is implemented.
    return ScriptPromise::rejectWithDOMException(scriptState,
        DOMException::create(NotSupportedError,
            "getPrimaryService is not implemented yet. See https://goo.gl/J6ASzs"));
#endif // OS(MACOSX)

    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);

    String serviceUUID = BluetoothUUID::getService(service, exceptionState);
    if (exceptionState.hadException())
        return exceptionState.reject(scriptState);

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    webbluetooth->getPrimaryService(device()->id(), serviceUUID, new CallbackPromiseAdapter<BluetoothRemoteGATTService, BluetoothError>(resolver));

    return promise;
}

} // namespace blink
