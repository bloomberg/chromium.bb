// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/MediaSession.h"

#include "core/dom/Document.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/Event.h"
#include "core/frame/LocalFrame.h"
#include "modules/EventTargetModules.h"
#include "modules/mediasession/MediaMetadata.h"
#include "modules/mediasession/MediaMetadataSanitizer.h"
#include "platform/UserGestureIndicator.h"
#include "public/platform/InterfaceProvider.h"
#include "wtf/Optional.h"
#include <memory>

namespace blink {

namespace {

using ::blink::mojom::blink::MediaSessionAction;

const AtomicString& mojomActionToEventName(MediaSessionAction action) {
  switch (action) {
    case MediaSessionAction::PLAY:
      return EventTypeNames::play;
    case MediaSessionAction::PAUSE:
      return EventTypeNames::pause;
    case MediaSessionAction::PREVIOUS_TRACK:
      return EventTypeNames::previoustrack;
    case MediaSessionAction::NEXT_TRACK:
      return EventTypeNames::nexttrack;
    case MediaSessionAction::SEEK_BACKWARD:
      return EventTypeNames::seekbackward;
    case MediaSessionAction::SEEK_FORWARD:
      return EventTypeNames::seekforward;
    default:
      NOTREACHED();
  }
  return WTF::emptyAtom;
}

WTF::Optional<MediaSessionAction> eventNameToMojomAction(
    const AtomicString& eventName) {
  if (EventTypeNames::play == eventName)
    return MediaSessionAction::PLAY;
  if (EventTypeNames::pause == eventName)
    return MediaSessionAction::PAUSE;
  if (EventTypeNames::previoustrack == eventName)
    return MediaSessionAction::PREVIOUS_TRACK;
  if (EventTypeNames::nexttrack == eventName)
    return MediaSessionAction::NEXT_TRACK;
  if (EventTypeNames::seekbackward == eventName)
    return MediaSessionAction::SEEK_BACKWARD;
  if (EventTypeNames::seekforward == eventName)
    return MediaSessionAction::SEEK_FORWARD;

  NOTREACHED();
  return WTF::nullopt;
}

const AtomicString& mediaSessionPlaybackStateToString(
    mojom::blink::MediaSessionPlaybackState state) {
  DEFINE_STATIC_LOCAL(const AtomicString, noneValue, ("none"));
  DEFINE_STATIC_LOCAL(const AtomicString, pausedValue, ("paused"));
  DEFINE_STATIC_LOCAL(const AtomicString, playingValue, ("playing"));

  switch (state) {
    case mojom::blink::MediaSessionPlaybackState::NONE:
      return noneValue;
    case mojom::blink::MediaSessionPlaybackState::PAUSED:
      return pausedValue;
    case mojom::blink::MediaSessionPlaybackState::PLAYING:
      return playingValue;
  }
  NOTREACHED();
  return WTF::emptyAtom;
}

mojom::blink::MediaSessionPlaybackState stringToMediaSessionPlaybackState(
    const String& stateName) {
  if (stateName == "none")
    return mojom::blink::MediaSessionPlaybackState::NONE;
  if (stateName == "paused")
    return mojom::blink::MediaSessionPlaybackState::PAUSED;
  DCHECK_EQ(stateName, "playing");
  return mojom::blink::MediaSessionPlaybackState::PLAYING;
}

}  // anonymous namespace

MediaSession::MediaSession(ExecutionContext* executionContext)
    : ContextLifecycleObserver(executionContext),
      m_playbackState(mojom::blink::MediaSessionPlaybackState::NONE),
      m_clientBinding(this) {}

MediaSession* MediaSession::create(ExecutionContext* executionContext) {
  return new MediaSession(executionContext);
}

void MediaSession::dispose() {
  m_clientBinding.Close();
}

void MediaSession::setPlaybackState(const String& playbackState) {
  m_playbackState = stringToMediaSessionPlaybackState(playbackState);
  mojom::blink::MediaSessionService* service = getService();
  if (service)
    service->SetPlaybackState(m_playbackState);
}

String MediaSession::playbackState() {
  return mediaSessionPlaybackStateToString(m_playbackState);
}

void MediaSession::setMetadata(MediaMetadata* metadata) {
  if (metadata)
    metadata->setSession(this);

  if (m_metadata)
    m_metadata->setSession(nullptr);

  m_metadata = metadata;
  onMetadataChanged();
}

MediaMetadata* MediaSession::metadata() const {
  return m_metadata;
}

void MediaSession::onMetadataChanged() {
  mojom::blink::MediaSessionService* service = getService();
  if (!service)
    return;

  service->SetMetadata(MediaMetadataSanitizer::sanitizeAndConvertToMojo(
      m_metadata, getExecutionContext()));
}

const WTF::AtomicString& MediaSession::interfaceName() const {
  return EventTargetNames::MediaSession;
}

ExecutionContext* MediaSession::getExecutionContext() const {
  return ContextLifecycleObserver::getExecutionContext();
}

mojom::blink::MediaSessionService* MediaSession::getService() {
  if (m_service)
    return m_service.get();
  if (!getExecutionContext())
    return nullptr;

  DCHECK(getExecutionContext()->isDocument())
      << "MediaSession::getService() is only available from a frame";
  Document* document = toDocument(getExecutionContext());
  if (!document->frame())
    return nullptr;

  InterfaceProvider* interfaceProvider = document->frame()->interfaceProvider();
  if (!interfaceProvider)
    return nullptr;

  interfaceProvider->getInterface(mojo::GetProxy(&m_service));
  if (m_service.get())
    m_service->SetClient(m_clientBinding.CreateInterfacePtrAndBind());

  return m_service.get();
}

bool MediaSession::addEventListenerInternal(
    const AtomicString& eventType,
    EventListener* listener,
    const AddEventListenerOptionsResolved& options) {
  if (mojom::blink::MediaSessionService* service = getService()) {
    auto mojomAction = eventNameToMojomAction(eventType);
    DCHECK(mojomAction.has_value());
    service->EnableAction(mojomAction.value());
  }
  return EventTarget::addEventListenerInternal(eventType, listener, options);
}

bool MediaSession::removeEventListenerInternal(
    const AtomicString& eventType,
    const EventListener* listener,
    const EventListenerOptions& options) {
  if (mojom::blink::MediaSessionService* service = getService()) {
    auto mojomAction = eventNameToMojomAction(eventType);
    DCHECK(mojomAction.has_value());
    service->DisableAction(mojomAction.value());
  }
  return EventTarget::removeEventListenerInternal(eventType, listener, options);
}

void MediaSession::DidReceiveAction(
    blink::mojom::blink::MediaSessionAction action) {
  DCHECK(getExecutionContext()->isDocument());
  Document* document = toDocument(getExecutionContext());
  UserGestureIndicator gestureIndicator(
      DocumentUserGestureToken::create(document));
  dispatchEvent(Event::create(mojomActionToEventName(action)));
}

DEFINE_TRACE(MediaSession) {
  visitor->trace(m_metadata);
  EventTargetWithInlineData::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink
