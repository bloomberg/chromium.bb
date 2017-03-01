// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/shadow/MediaControlsMediaEventListener.h"

#include "core/events/Event.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/shadow/MediaControls.h"
#include "core/html/track/TextTrackList.h"

namespace blink {

MediaControlsMediaEventListener::MediaControlsMediaEventListener(
    MediaControls* mediaControls)
    : EventListener(CPPEventListenerType), m_mediaControls(mediaControls) {
  // These events are always active because they are needed in order to attach
  // or detach the whole controls.
  mediaElement().addEventListener(EventTypeNames::DOMNodeInsertedIntoDocument,
                                  this, false);
  mediaElement().addEventListener(EventTypeNames::DOMNodeRemovedFromDocument,
                                  this, false);

  if (mediaElement().isConnected())
    attach();
}

void MediaControlsMediaEventListener::attach() {
  mediaElement().addEventListener(EventTypeNames::volumechange, this, false);
  mediaElement().addEventListener(EventTypeNames::focusin, this, false);
  mediaElement().addEventListener(EventTypeNames::timeupdate, this, false);
  mediaElement().addEventListener(EventTypeNames::play, this, false);
  mediaElement().addEventListener(EventTypeNames::pause, this, false);
  mediaElement().addEventListener(EventTypeNames::durationchange, this, false);
  mediaElement().addEventListener(EventTypeNames::error, this, false);
  mediaElement().addEventListener(EventTypeNames::loadedmetadata, this, false);

  // Listen to two different fullscreen events in order to make sure the new and
  // old APIs are handled.
  mediaElement().addEventListener(EventTypeNames::webkitfullscreenchange, this,
                                  false);
  m_mediaControls->document().addEventListener(EventTypeNames::fullscreenchange,
                                               this, false);

  // TextTracks events.
  TextTrackList* textTracks = mediaElement().textTracks();
  textTracks->addEventListener(EventTypeNames::addtrack, this, false);
  textTracks->addEventListener(EventTypeNames::change, this, false);
  textTracks->addEventListener(EventTypeNames::removetrack, this, false);
}

void MediaControlsMediaEventListener::detach() {
  m_mediaControls->document().removeEventListener(
      EventTypeNames::fullscreenchange, this, false);

  TextTrackList* textTracks = mediaElement().textTracks();
  textTracks->removeEventListener(EventTypeNames::addtrack, this, false);
  textTracks->removeEventListener(EventTypeNames::change, this, false);
  textTracks->removeEventListener(EventTypeNames::removetrack, this, false);
}

bool MediaControlsMediaEventListener::operator==(
    const EventListener& other) const {
  return this == &other;
}

HTMLMediaElement& MediaControlsMediaEventListener::mediaElement() {
  return m_mediaControls->mediaElement();
}

void MediaControlsMediaEventListener::handleEvent(
    ExecutionContext* executionContext,
    Event* event) {
  if (event->type() == EventTypeNames::DOMNodeInsertedIntoDocument) {
    m_mediaControls->onInsertedIntoDocument();
    return;
  }
  if (event->type() == EventTypeNames::DOMNodeRemovedFromDocument) {
    m_mediaControls->onRemovedFromDocument();
    return;
  }
  if (event->type() == EventTypeNames::volumechange) {
    m_mediaControls->onVolumeChange();
    return;
  }
  if (event->type() == EventTypeNames::focusin) {
    m_mediaControls->onFocusIn();
    return;
  }
  if (event->type() == EventTypeNames::timeupdate) {
    m_mediaControls->onTimeUpdate();
    return;
  }
  if (event->type() == EventTypeNames::durationchange) {
    m_mediaControls->onDurationChange();
    return;
  }
  if (event->type() == EventTypeNames::play) {
    m_mediaControls->onPlay();
    return;
  }
  if (event->type() == EventTypeNames::pause) {
    m_mediaControls->onPause();
    return;
  }
  if (event->type() == EventTypeNames::error) {
    m_mediaControls->onError();
    return;
  }
  if (event->type() == EventTypeNames::loadedmetadata) {
    m_mediaControls->onLoadedMetadata();
    return;
  }

  // Fullscreen handling.
  if (event->type() == EventTypeNames::fullscreenchange ||
      event->type() == EventTypeNames::webkitfullscreenchange) {
    if (mediaElement().isFullscreen())
      m_mediaControls->onEnteredFullscreen();
    else
      m_mediaControls->onExitedFullscreen();
    return;
  }

  // TextTracks events.
  if (event->type() == EventTypeNames::addtrack ||
      event->type() == EventTypeNames::removetrack) {
    m_mediaControls->onTextTracksAddedOrRemoved();
    return;
  }
  if (event->type() == EventTypeNames::change) {
    m_mediaControls->onTextTracksChanged();
    return;
  }

  NOTREACHED();
}

DEFINE_TRACE(MediaControlsMediaEventListener) {
  EventListener::trace(visitor);
  visitor->trace(m_mediaControls);
}

}  // namespace blink
