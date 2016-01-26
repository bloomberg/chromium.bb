// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothGATTRemoteServer.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothGATTService.h"
#include "modules/bluetooth/BluetoothSupplement.h"
#include "modules/bluetooth/BluetoothUUID.h"
#include "public/platform/modules/bluetooth/WebBluetooth.h"
#include "wtf/OwnPtr.h"

namespace blink {

BluetoothGATTRemoteServer::BluetoothGATTRemoteServer(ExecutionContext* context, PassOwnPtr<WebBluetoothGATTRemoteServer> webGATT)
    : ActiveDOMObject(context)
    , PageLifecycleObserver(toDocument(context)->page())
    , m_webGATT(webGATT)
{
    // See example in Source/platform/heap/ThreadState.h
    ThreadState::current()->registerPreFinalizer(this);
}

BluetoothGATTRemoteServer* BluetoothGATTRemoteServer::take(ScriptPromiseResolver* resolver, PassOwnPtr<WebBluetoothGATTRemoteServer> webGATT)
{
    ASSERT(webGATT);
    BluetoothGATTRemoteServer* server = new BluetoothGATTRemoteServer(resolver->executionContext(), webGATT);
    if (!server->page()->isPageVisible()) {
        server->disconnectIfConnected();
    }
    server->suspendIfNeeded();
    return server;
}

void BluetoothGATTRemoteServer::dispose()
{
    disconnectIfConnected();
}

void BluetoothGATTRemoteServer::stop()
{
    disconnectIfConnected();
}

void BluetoothGATTRemoteServer::pageVisibilityChanged()
{
    if (!page()->isPageVisible()) {
        disconnectIfConnected();
    }
}

void BluetoothGATTRemoteServer::disconnectIfConnected()
{
    if (m_webGATT->connected) {
        m_webGATT->connected = false;
        WebBluetooth* webbluetooth = BluetoothSupplement::fromExecutionContext(executionContext());
        webbluetooth->disconnect(m_webGATT->deviceId);
    }
}

DEFINE_TRACE(BluetoothGATTRemoteServer)
{
    ActiveDOMObject::trace(visitor);
    PageLifecycleObserver::trace(visitor);
}

void BluetoothGATTRemoteServer::disconnect(ScriptState* scriptState)
{
    m_webGATT->connected = false;
    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);
    webbluetooth->disconnect(m_webGATT->deviceId);
}

ScriptPromise BluetoothGATTRemoteServer::getPrimaryService(ScriptState* scriptState, const StringOrUnsignedLong& service, ExceptionState& exceptionState)
{
    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);

    String serviceUUID = BluetoothUUID::getService(service, exceptionState);
    if (exceptionState.hadException())
        return exceptionState.reject(scriptState);

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    webbluetooth->getPrimaryService(m_webGATT->deviceId, serviceUUID, new CallbackPromiseAdapter<BluetoothGATTService, BluetoothError>(resolver));

    return promise;
}

} // namespace blink
