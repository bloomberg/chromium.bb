// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationReceiver.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Deprecation.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "core/frame/UseCounter.h"
#include "modules/presentation/NavigatorPresentation.h"
#include "modules/presentation/Presentation.h"
#include "modules/presentation/PresentationConnection.h"
#include "modules/presentation/PresentationConnectionList.h"
#include "public/platform/modules/presentation/WebPresentationClient.h"

namespace blink {

PresentationReceiver::PresentationReceiver(LocalFrame* frame,
                                           WebPresentationClient* client)
    : ContextClient(frame) {
  connection_list_ = new PresentationConnectionList(frame->GetDocument());

  if (client)
    client->SetReceiver(this);
}

// static
PresentationReceiver* PresentationReceiver::From(Document& document) {
  if (!document.GetFrame() || !document.GetFrame()->DomWindow())
    return nullptr;
  Navigator& navigator = *document.GetFrame()->DomWindow()->navigator();
  Presentation* presentation = NavigatorPresentation::presentation(navigator);
  if (!presentation)
    return nullptr;

  return presentation->receiver();
}

ScriptPromise PresentationReceiver::connectionList(ScriptState* script_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  RecordOriginTypeAccess(*execution_context);
  if (!connection_list_property_) {
    connection_list_property_ = new ConnectionListProperty(
        execution_context, this, ConnectionListProperty::kReady);
  }

  if (!connection_list_->IsEmpty() && connection_list_property_->GetState() ==
                                          ScriptPromisePropertyBase::kPending)
    connection_list_property_->Resolve(connection_list_);

  return connection_list_property_->Promise(script_state->World());
}

WebPresentationConnection* PresentationReceiver::OnReceiverConnectionAvailable(
    const WebPresentationInfo& presentation_info) {
  // take() will call PresentationReceiver::registerConnection()
  // and register the connection.
  auto connection = PresentationConnection::Take(this, presentation_info);

  // receiver.connectionList property not accessed
  if (!connection_list_property_)
    return connection;

  if (connection_list_property_->GetState() ==
      ScriptPromisePropertyBase::kPending) {
    connection_list_property_->Resolve(connection_list_);
  } else if (connection_list_property_->GetState() ==
             ScriptPromisePropertyBase::kResolved) {
    connection_list_->DispatchConnectionAvailableEvent(connection);
  }

  return connection;
}

void PresentationReceiver::DidChangeConnectionState(
    WebPresentationConnectionState state) {
  // TODO(zhaobin): remove or modify DCHECK when receiver supports more
  // connection state change.
  DCHECK(state == WebPresentationConnectionState::kTerminated);

  for (auto connection : connection_list_->connections()) {
    connection->NotifyTargetConnection(state);
    connection->DidChangeState(state, false /* shouldDispatchEvent */);
  }
}

void PresentationReceiver::TerminateConnection() {
  if (!GetFrame())
    return;

  auto* window = GetFrame()->DomWindow();
  if (!window || window->closed())
    return;

  window->close(window);
}

void PresentationReceiver::RemoveConnection(
    WebPresentationConnection* connection) {
  DCHECK(connection_list_);
  connection_list_->RemoveConnection(connection);
}

void PresentationReceiver::RegisterConnection(
    PresentationConnection* connection) {
  DCHECK(connection_list_);
  connection_list_->AddConnection(connection);
}

// static
void PresentationReceiver::RecordOriginTypeAccess(
    ExecutionContext& execution_context) {
  if (execution_context.IsSecureContext()) {
    UseCounter::Count(&execution_context,
                      WebFeature::kPresentationReceiverSecureOrigin);
  } else {
    Deprecation::CountDeprecation(
        &execution_context, WebFeature::kPresentationReceiverInsecureOrigin);
  }
}

DEFINE_TRACE(PresentationReceiver) {
  visitor->Trace(connection_list_);
  visitor->Trace(connection_list_property_);
  ContextClient::Trace(visitor);
}

}  // namespace blink
