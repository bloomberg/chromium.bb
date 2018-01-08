// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationController.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/frame/Deprecation.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "modules/presentation/PresentationAvailabilityCallbacks.h"
#include "modules/presentation/PresentationAvailabilityObserver.h"
#include "modules/presentation/PresentationAvailabilityState.h"
#include "modules/presentation/PresentationConnection.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/presentation/WebPresentationClient.h"
#include "public/platform/modules/presentation/WebPresentationError.h"

namespace blink {

PresentationController::PresentationController(LocalFrame& frame,
                                               WebPresentationClient* client)
    : Supplement<LocalFrame>(frame),
      ContextLifecycleObserver(frame.GetDocument()),
      client_(client),
      controller_binding_(this) {}

PresentationController::~PresentationController() = default;

// static
PresentationController* PresentationController::Create(
    LocalFrame& frame,
    WebPresentationClient* client) {
  return new PresentationController(frame, client);
}

// static
const char* PresentationController::SupplementName() {
  return "PresentationController";
}

// static
PresentationController* PresentationController::From(LocalFrame& frame) {
  return static_cast<PresentationController*>(
      Supplement<LocalFrame>::From(frame, SupplementName()));
}

// static
void PresentationController::ProvideTo(LocalFrame& frame,
                                       WebPresentationClient* client) {
  Supplement<LocalFrame>::ProvideTo(
      frame, PresentationController::SupplementName(),
      PresentationController::Create(frame, client));
}

WebPresentationClient* PresentationController::Client() {
  return client_;
}

// static
PresentationController* PresentationController::FromContext(
    ExecutionContext* execution_context) {
  if (!execution_context)
    return nullptr;

  DCHECK(execution_context->IsDocument());
  Document* document = ToDocument(execution_context);
  if (!document->GetFrame())
    return nullptr;

  return PresentationController::From(*document->GetFrame());
}

// static
WebPresentationClient* PresentationController::ClientFromContext(
    ExecutionContext* execution_context) {
  PresentationController* controller =
      PresentationController::FromContext(execution_context);
  return controller ? controller->Client() : nullptr;
}

void PresentationController::Trace(blink::Visitor* visitor) {
  visitor->Trace(presentation_);
  visitor->Trace(connections_);
  Supplement<LocalFrame>::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void PresentationController::SetPresentation(Presentation* presentation) {
  presentation_ = presentation;
}

void PresentationController::RegisterConnection(
    ControllerPresentationConnection* connection) {
  connections_.insert(connection);
}

PresentationAvailabilityState* PresentationController::GetAvailabilityState() {
  if (!availability_state_) {
    availability_state_.reset(
        new PresentationAvailabilityState(GetPresentationService().get()));
  }

  return availability_state_.get();
}

void PresentationController::AddAvailabilityObserver(
    PresentationAvailabilityObserver* observer) {
  GetAvailabilityState()->AddObserver(observer);
}

void PresentationController::RemoveAvailabilityObserver(
    PresentationAvailabilityObserver* observer) {
  GetAvailabilityState()->RemoveObserver(observer);
}

void PresentationController::OnScreenAvailabilityUpdated(
    const KURL& url,
    mojom::blink::ScreenAvailability availability) {
  GetAvailabilityState()->UpdateAvailability(url, availability);
}

void PresentationController::OnConnectionStateChanged(
    mojom::blink::PresentationInfoPtr presentation_info,
    mojom::blink::PresentationConnectionState state) {
  PresentationConnection* connection = FindConnection(*presentation_info);
  if (!connection)
    return;

  connection->DidChangeState(state);
}

void PresentationController::OnConnectionClosed(
    mojom::blink::PresentationInfoPtr presentation_info,
    mojom::blink::PresentationConnectionCloseReason reason,
    const String& message) {
  PresentationConnection* connection = FindConnection(*presentation_info);
  if (!connection)
    return;

  connection->DidClose(reason, message);
}

void PresentationController::OnDefaultPresentationStarted(
    mojom::blink::PresentationInfoPtr presentation_info) {
  DCHECK(presentation_info);
  if (!presentation_ || !presentation_->defaultRequest())
    return;

  PresentationRequest::RecordStartOriginTypeAccess(*GetExecutionContext());
  auto* connection = ControllerPresentationConnection::Take(
      this, *presentation_info, presentation_->defaultRequest());
  connection->Init();
}

void PresentationController::ContextDestroyed(ExecutionContext*) {
  client_ = nullptr;
  controller_binding_.Close();
}

ControllerPresentationConnection*
PresentationController::FindExistingConnection(
    const blink::WebVector<blink::WebURL>& presentation_urls,
    const blink::WebString& presentation_id) {
  for (const auto& connection : connections_) {
    for (const auto& presentation_url : presentation_urls) {
      if (connection->GetState() !=
              mojom::blink::PresentationConnectionState::TERMINATED &&
          connection->Matches(presentation_id, presentation_url)) {
        return connection.Get();
      }
    }
  }
  return nullptr;
}

mojom::blink::PresentationServicePtr&
PresentationController::GetPresentationService() {
  if (!presentation_service_ && GetFrame() && GetFrame()->Client()) {
    auto* interface_provider = GetFrame()->Client()->GetInterfaceProvider();
    interface_provider->GetInterface(mojo::MakeRequest(&presentation_service_));

    mojom::blink::PresentationControllerPtr controller_ptr;
    controller_binding_.Bind(mojo::MakeRequest(&controller_ptr));
    presentation_service_->SetController(std::move(controller_ptr));
  }
  return presentation_service_;
}

ControllerPresentationConnection* PresentationController::FindConnection(
    const mojom::blink::PresentationInfo& presentation_info) const {
  for (const auto& connection : connections_) {
    if (connection->Matches(presentation_info.id, presentation_info.url))
      return connection.Get();
  }

  return nullptr;
}

}  // namespace blink
