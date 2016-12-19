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
  m_mediaControls->m_mediaElement->addEventListener(
      EventTypeNames::volumechange, this, false);
  m_mediaControls->m_mediaElement->addEventListener(EventTypeNames::focusin,
                                                    this, false);
  m_mediaControls->m_mediaElement->addEventListener(EventTypeNames::timeupdate,
                                                    this, false);
  m_mediaControls->m_mediaElement->addEventListener(EventTypeNames::play, this,
                                                    false);
  m_mediaControls->m_mediaElement->addEventListener(EventTypeNames::pause, this,
                                                    false);
  m_mediaControls->m_mediaElement->addEventListener(
      EventTypeNames::durationchange, this, false);
  m_mediaControls->m_mediaElement->addEventListener(EventTypeNames::error, this,
                                                    false);
  m_mediaControls->m_mediaElement->addEventListener(
      EventTypeNames::loadedmetadata, this, false);

  // TextTracks events.
  TextTrackList* textTracks = m_mediaControls->m_mediaElement->textTracks();
  textTracks->addEventListener(EventTypeNames::addtrack, this, false);
  textTracks->addEventListener(EventTypeNames::change, this, false);
  textTracks->addEventListener(EventTypeNames::removetrack, this, false);
}

bool MediaControlsMediaEventListener::operator==(
    const EventListener& other) const {
  return this == &other;
}

void MediaControlsMediaEventListener::handleEvent(
    ExecutionContext* executionContext,
    Event* event) {
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
