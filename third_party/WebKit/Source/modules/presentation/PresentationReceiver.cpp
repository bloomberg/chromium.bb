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
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Navigator.h"
#include "core/frame/UseCounter.h"
#include "modules/presentation/NavigatorPresentation.h"
#include "modules/presentation/Presentation.h"
#include "modules/presentation/PresentationConnection.h"
#include "modules/presentation/PresentationConnectionList.h"
#include "public/platform/modules/presentation/WebPresentationClient.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {

PresentationReceiver::PresentationReceiver(LocalFrame* frame,
                                           WebPresentationClient* client)
    : ContextLifecycleObserver(frame->GetDocument()),
      receiver_binding_(this),
      client_(client) {
  connection_list_ = new PresentationConnectionList(frame->GetDocument());

  if (client)
    client->SetReceiver(this);
}

// static
PresentationReceiver* PresentationReceiver::From(Document& document) {
  if (!document.IsInMainFrame() || !document.GetFrame()->DomWindow())
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

void PresentationReceiver::Init() {
  DCHECK(!receiver_binding_.is_bound());

  mojom::blink::PresentationServicePtr presentation_service;
  auto* interface_provider = GetFrame()->Client()->GetInterfaceProvider();
  interface_provider->GetInterface(mojo::MakeRequest(&presentation_service));

  mojom::blink::PresentationReceiverPtr receiver_ptr;
  receiver_binding_.Bind(mojo::MakeRequest(&receiver_ptr));
  presentation_service->SetReceiver(std::move(receiver_ptr));
}

void PresentationReceiver::OnReceiverTerminated() {
  for (auto& connection : connection_list_->connections())
    connection->OnReceiverTerminated();
}

void PresentationReceiver::Terminate() {
  if (!GetFrame())
    return;

  auto* window = GetFrame()->DomWindow();
  if (!window || window->closed())
    return;

  window->close(window);
}

void PresentationReceiver::RemoveConnection(
    ReceiverPresentationConnection* connection) {
  DCHECK(connection_list_);
  connection_list_->RemoveConnection(connection);
}

void PresentationReceiver::OnReceiverConnectionAvailable(
    mojom::blink::PresentationInfoPtr info,
    mojom::blink::PresentationConnectionPtr controller_connection,
    mojom::blink::PresentationConnectionRequest receiver_connection_request) {
  // Take() will call PresentationReceiver::registerConnection()
  // and register the connection.
  auto* connection = ReceiverPresentationConnection::Take(
      this, *info, std::move(controller_connection),
      std::move(receiver_connection_request));

  // Only notify receiver.connectionList property if it has been acccessed
  // previously.
  if (!connection_list_property_)
    return;

  if (connection_list_property_->GetState() ==
      ScriptPromisePropertyBase::kPending) {
    connection_list_property_->Resolve(connection_list_);
  } else if (connection_list_property_->GetState() ==
             ScriptPromisePropertyBase::kResolved) {
    connection_list_->DispatchConnectionAvailableEvent(connection);
  }
}

void PresentationReceiver::RegisterConnection(
    ReceiverPresentationConnection* connection) {
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

void PresentationReceiver::ContextDestroyed(ExecutionContext*) {
  receiver_binding_.Close();
  if (client_) {
    client_->SetReceiver(nullptr);
    client_ = nullptr;
  }
}

DEFINE_TRACE(PresentationReceiver) {
  visitor->Trace(connection_list_);
  visitor->Trace(connection_list_property_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
