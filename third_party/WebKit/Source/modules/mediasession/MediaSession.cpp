// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/MediaSession.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "modules/EventTargetModules.h"
#include "modules/mediasession/MediaMetadata.h"
#include "modules/mediasession/MediaMetadataSanitizer.h"
#include "public/platform/InterfaceProvider.h"
#include <memory>

namespace blink {

MediaSession::MediaSession(ScriptState* scriptState)
    : m_scriptState(scriptState) {}

MediaSession* MediaSession::create(ScriptState* scriptState) {
  return new MediaSession(scriptState);
}

void MediaSession::setMetadata(MediaMetadata* metadata) {
  if (mojom::blink::MediaSessionService* service =
          getService(m_scriptState.get())) {
    service->SetMetadata(
        MediaMetadataSanitizer::sanitizeAndConvertToMojo(metadata));
  }
}

MediaMetadata* MediaSession::metadata() const {
  return m_metadata;
}

const WTF::AtomicString& MediaSession::interfaceName() const {
  return EventTargetNames::MediaSession;
}

ExecutionContext* MediaSession::getExecutionContext() const {
  return m_scriptState->getExecutionContext();
}

mojom::blink::MediaSessionService* MediaSession::getService(
    ScriptState* scriptState) {
  if (!m_service) {
    InterfaceProvider* interfaceProvider = nullptr;
    DCHECK(scriptState->getExecutionContext()->isDocument())
        << "MediaSession::getService() is only available from a frame";
    Document* document = toDocument(scriptState->getExecutionContext());
    if (document->frame())
      interfaceProvider = document->frame()->interfaceProvider();

    if (interfaceProvider)
      interfaceProvider->getInterface(mojo::GetProxy(&m_service));
  }
  return m_service.get();
}

bool MediaSession::addEventListenerInternal(
    const AtomicString& eventType,
    EventListener* listener,
    const AddEventListenerOptionsResolved& options) {
  // TODO(zqzhang): Notify MediaSessionService the handler has been set. See
  // https://crbug.com/656563
  return EventTarget::addEventListenerInternal(eventType, listener, options);
}

bool MediaSession::removeEventListenerInternal(
    const AtomicString& eventType,
    const EventListener* listener,
    const EventListenerOptions& options) {
  // TODO(zqzhang): Notify MediaSessionService the handler has been unset. See
  // https://crbug.com/656563
  return EventTarget::removeEventListenerInternal(eventType, listener, options);
}

DEFINE_TRACE(MediaSession) {
  visitor->trace(m_metadata);
  EventTargetWithInlineData::trace(visitor);
}

}  // namespace blink
