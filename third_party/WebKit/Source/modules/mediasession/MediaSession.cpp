// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/MediaSession.h"

#include "bindings/modules/v8/MediaSessionActionHandler.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "modules/mediasession/MediaMetadata.h"
#include "modules/mediasession/MediaMetadataSanitizer.h"
#include "platform/UserGestureIndicator.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "wtf/Optional.h"
#include <memory>

namespace blink {

namespace {

using ::blink::mojom::blink::MediaSessionAction;

const AtomicString& mojomActionToActionName(MediaSessionAction action) {
  DEFINE_STATIC_LOCAL(const AtomicString, playActionName, ("play"));
  DEFINE_STATIC_LOCAL(const AtomicString, pauseActionName, ("pause"));
  DEFINE_STATIC_LOCAL(const AtomicString, previousTrackActionName,
                      ("previoustrack"));
  DEFINE_STATIC_LOCAL(const AtomicString, nextTrackActionName, ("nexttrack"));
  DEFINE_STATIC_LOCAL(const AtomicString, seekBackwardActionName,
                      ("seekbackward"));
  DEFINE_STATIC_LOCAL(const AtomicString, seekForwardActionName,
                      ("seekforward"));

  switch (action) {
    case MediaSessionAction::PLAY:
      return playActionName;
    case MediaSessionAction::PAUSE:
      return pauseActionName;
    case MediaSessionAction::PREVIOUS_TRACK:
      return previousTrackActionName;
    case MediaSessionAction::NEXT_TRACK:
      return nextTrackActionName;
    case MediaSessionAction::SEEK_BACKWARD:
      return seekBackwardActionName;
    case MediaSessionAction::SEEK_FORWARD:
      return seekForwardActionName;
    default:
      NOTREACHED();
  }
  return WTF::emptyAtom;
}

WTF::Optional<MediaSessionAction> actionNameToMojomAction(
    const String& actionName) {
  if ("play" == actionName)
    return MediaSessionAction::PLAY;
  if ("pause" == actionName)
    return MediaSessionAction::PAUSE;
  if ("previoustrack" == actionName)
    return MediaSessionAction::PREVIOUS_TRACK;
  if ("nexttrack" == actionName)
    return MediaSessionAction::NEXT_TRACK;
  if ("seekbackward" == actionName)
    return MediaSessionAction::SEEK_BACKWARD;
  if ("seekforward" == actionName)
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
    : ContextClient(executionContext),
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

void MediaSession::setActionHandler(const String& action,
                                    MediaSessionActionHandler* handler) {
  if (handler) {
    auto addResult = m_actionHandlers.set(
        action, TraceWrapperMember<MediaSessionActionHandler>(this, handler));

    if (!addResult.isNewEntry)
      return;

    notifyActionChange(action, ActionChangeType::ActionEnabled);
  } else {
    if (m_actionHandlers.find(action) == m_actionHandlers.end())
      return;

    m_actionHandlers.erase(action);

    notifyActionChange(action, ActionChangeType::ActionDisabled);
  }
}

void MediaSession::notifyActionChange(const String& action,
                                      ActionChangeType type) {
  mojom::blink::MediaSessionService* service = getService();
  if (!service)
    return;

  auto mojomAction = actionNameToMojomAction(action);
  DCHECK(mojomAction.has_value());

  switch (type) {
    case ActionChangeType::ActionEnabled:
      service->EnableAction(mojomAction.value());
      break;
    case ActionChangeType::ActionDisabled:
      service->DisableAction(mojomAction.value());
      break;
  }
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

  interfaceProvider->getInterface(mojo::MakeRequest(&m_service));
  if (m_service.get()) {
    // Record the eTLD+1 of the frame using the API.
    Platform::current()->recordRapporURL("Media.Session.APIUsage.Origin",
                                         document->url());
    m_service->SetClient(m_clientBinding.CreateInterfacePtrAndBind());
  }

  return m_service.get();
}

void MediaSession::DidReceiveAction(
    blink::mojom::blink::MediaSessionAction action) {
  DCHECK(getExecutionContext()->isDocument());
  Document* document = toDocument(getExecutionContext());
  UserGestureIndicator gestureIndicator(
      DocumentUserGestureToken::create(document));

  auto iter = m_actionHandlers.find(mojomActionToActionName(action));
  if (iter == m_actionHandlers.end())
    return;

  iter->value->call(this);
}

DEFINE_TRACE(MediaSession) {
  visitor->trace(m_metadata);
  visitor->trace(m_actionHandlers);
  ContextClient::trace(visitor);
}

DEFINE_TRACE_WRAPPERS(MediaSession) {
  for (auto handler : m_actionHandlers.values())
    visitor->traceWrappers(handler);
}

}  // namespace blink
