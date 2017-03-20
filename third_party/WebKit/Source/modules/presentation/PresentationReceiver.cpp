// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationReceiver.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "modules/presentation/PresentationConnection.h"
#include "modules/presentation/PresentationConnectionList.h"
#include "public/platform/modules/presentation/WebPresentationClient.h"

namespace blink {

PresentationReceiver::PresentationReceiver(LocalFrame* frame,
                                           WebPresentationClient* client)
    : ContextClient(frame) {
  recordOriginTypeAccess(frame->document());
  m_connectionList = new PresentationConnectionList(frame->document());

  if (client)
    client->setReceiver(this);
}

ScriptPromise PresentationReceiver::connectionList(ScriptState* scriptState) {
  if (!m_connectionListProperty)
    m_connectionListProperty =
        new ConnectionListProperty(scriptState->getExecutionContext(), this,
                                   ConnectionListProperty::Ready);

  if (!m_connectionList->isEmpty() &&
      m_connectionListProperty->getState() ==
          ScriptPromisePropertyBase::Pending)
    m_connectionListProperty->resolve(m_connectionList);

  return m_connectionListProperty->promise(scriptState->world());
}

WebPresentationConnection* PresentationReceiver::onReceiverConnectionAvailable(
    const WebPresentationInfo& presentationInfo) {
  // take() will call PresentationReceiver::registerConnection()
  // and register the connection.
  auto connection = PresentationConnection::take(this, presentationInfo);

  // receiver.connectionList property not accessed
  if (!m_connectionListProperty)
    return connection;

  if (m_connectionListProperty->getState() ==
      ScriptPromisePropertyBase::Pending) {
    m_connectionListProperty->resolve(m_connectionList);
  } else if (m_connectionListProperty->getState() ==
             ScriptPromisePropertyBase::Resolved) {
    m_connectionList->dispatchConnectionAvailableEvent(connection);
  }

  return connection;
}

void PresentationReceiver::didChangeConnectionState(
    WebPresentationConnectionState state) {
  // TODO(zhaobin): remove or modify DCHECK when receiver supports more
  // connection state change.
  DCHECK(state == WebPresentationConnectionState::Terminated);

  for (auto connection : m_connectionList->connections())
    connection->didChangeState(state, false /* shouldDispatchEvent */);
}

void PresentationReceiver::terminateConnection() {
  if (!frame())
    return;

  auto* window = frame()->domWindow();
  if (!window || window->closed())
    return;

  window->close(frame()->document());
}

void PresentationReceiver::registerConnection(
    PresentationConnection* connection) {
  DCHECK(m_connectionList);
  m_connectionList->addConnection(connection);
}

void PresentationReceiver::recordOriginTypeAccess(Document* document) const {
  DCHECK(document);
  if (document->isSecureContext()) {
    UseCounter::count(document, UseCounter::PresentationReceiverSecureOrigin);
  } else {
    UseCounter::count(document, UseCounter::PresentationReceiverInsecureOrigin);
  }
}

DEFINE_TRACE(PresentationReceiver) {
  visitor->trace(m_connectionList);
  visitor->trace(m_connectionListProperty);
  ContextClient::trace(visitor);
}

}  // namespace blink
