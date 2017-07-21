// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationController.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/frame/Deprecation.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "modules/presentation/PresentationConnection.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/presentation/WebPresentationClient.h"

namespace blink {

PresentationController::PresentationController(LocalFrame& frame,
                                               WebPresentationClient* client)
    : Supplement<LocalFrame>(frame),
      ContextLifecycleObserver(frame.GetDocument()),
      client_(client) {
  if (client_)
    client_->SetController(this);
}

PresentationController::~PresentationController() {
  if (client_)
    client_->SetController(nullptr);
}

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

DEFINE_TRACE(PresentationController) {
  visitor->Trace(presentation_);
  visitor->Trace(connections_);
  Supplement<LocalFrame>::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

WebPresentationConnection* PresentationController::DidStartDefaultPresentation(
    const WebPresentationInfo& presentation_info) {
  if (!presentation_ || !presentation_->defaultRequest())
    return nullptr;

  PresentationRequest::RecordStartOriginTypeAccess(*GetExecutionContext());
  return PresentationConnection::Take(this, presentation_info,
                                      presentation_->defaultRequest());
}

void PresentationController::DidChangeConnectionState(
    const WebPresentationInfo& presentation_info,
    WebPresentationConnectionState state) {
  PresentationConnection* connection = FindConnection(presentation_info);
  if (!connection)
    return;
  connection->DidChangeState(state);
}

void PresentationController::DidCloseConnection(
    const WebPresentationInfo& presentation_info,
    WebPresentationConnectionCloseReason reason,
    const WebString& message) {
  PresentationConnection* connection = FindConnection(presentation_info);
  if (!connection)
    return;
  connection->DidClose(reason, message);
}

void PresentationController::SetPresentation(Presentation* presentation) {
  presentation_ = presentation;
}

void PresentationController::SetDefaultRequestUrl(
    const WTF::Vector<KURL>& urls) {
  if (!client_)
    return;

  WebVector<WebURL> presentation_urls(urls.size());
  for (size_t i = 0; i < urls.size(); ++i) {
    if (urls[i].IsValid())
      presentation_urls[i] = urls[i];
  }

  client_->SetDefaultPresentationUrls(presentation_urls);
}

void PresentationController::RegisterConnection(
    PresentationConnection* connection) {
  connections_.insert(connection);
}

void PresentationController::ContextDestroyed(ExecutionContext*) {
  if (client_) {
    client_->SetController(nullptr);
    client_ = nullptr;
  }
}

PresentationConnection* PresentationController::FindExistingConnection(
    const blink::WebVector<blink::WebURL>& presentation_urls,
    const blink::WebString& presentation_id) {
  for (const auto& connection : connections_) {
    for (const auto& presentation_url : presentation_urls) {
      if (connection->GetState() !=
              WebPresentationConnectionState::kTerminated &&
          connection->Matches(presentation_id, presentation_url)) {
        return connection.Get();
      }
    }
  }
  return nullptr;
}

PresentationConnection* PresentationController::FindConnection(
    const WebPresentationInfo& presentation_info) {
  for (const auto& connection : connections_) {
    if (connection->Matches(presentation_info))
      return connection.Get();
  }

  return nullptr;
}

}  // namespace blink
