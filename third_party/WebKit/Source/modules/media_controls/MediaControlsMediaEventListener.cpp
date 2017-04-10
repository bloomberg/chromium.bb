// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/MediaControlsMediaEventListener.h"

#include "core/events/Event.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/track/TextTrackList.h"
#include "modules/media_controls/MediaControlsImpl.h"

namespace blink {

MediaControlsMediaEventListener::MediaControlsMediaEventListener(
    MediaControlsImpl* media_controls)
    : EventListener(kCPPEventListenerType), media_controls_(media_controls) {
  if (GetMediaElement().isConnected())
    Attach();
}

void MediaControlsMediaEventListener::Attach() {
  DCHECK(GetMediaElement().isConnected());

  GetMediaElement().addEventListener(EventTypeNames::volumechange, this, false);
  GetMediaElement().addEventListener(EventTypeNames::focusin, this, false);
  GetMediaElement().addEventListener(EventTypeNames::timeupdate, this, false);
  GetMediaElement().addEventListener(EventTypeNames::play, this, false);
  GetMediaElement().addEventListener(EventTypeNames::playing, this, false);
  GetMediaElement().addEventListener(EventTypeNames::pause, this, false);
  GetMediaElement().addEventListener(EventTypeNames::durationchange, this,
                                     false);
  GetMediaElement().addEventListener(EventTypeNames::error, this, false);
  GetMediaElement().addEventListener(EventTypeNames::loadedmetadata, this,
                                     false);

  // Listen to two different fullscreen events in order to make sure the new and
  // old APIs are handled.
  GetMediaElement().addEventListener(EventTypeNames::webkitfullscreenchange,
                                     this, false);
  media_controls_->GetDocument().addEventListener(
      EventTypeNames::fullscreenchange, this, false);

  // TextTracks events.
  TextTrackList* text_tracks = GetMediaElement().textTracks();
  text_tracks->addEventListener(EventTypeNames::addtrack, this, false);
  text_tracks->addEventListener(EventTypeNames::change, this, false);
  text_tracks->addEventListener(EventTypeNames::removetrack, this, false);

  // Keypress events.
  if (media_controls_->PanelElement()) {
    media_controls_->PanelElement()->addEventListener(EventTypeNames::keypress,
                                                      this, false);
  }
}

void MediaControlsMediaEventListener::Detach() {
  DCHECK(!GetMediaElement().isConnected());

  media_controls_->GetDocument().removeEventListener(
      EventTypeNames::fullscreenchange, this, false);

  TextTrackList* text_tracks = GetMediaElement().textTracks();
  text_tracks->removeEventListener(EventTypeNames::addtrack, this, false);
  text_tracks->removeEventListener(EventTypeNames::change, this, false);
  text_tracks->removeEventListener(EventTypeNames::removetrack, this, false);

  if (media_controls_->PanelElement()) {
    media_controls_->PanelElement()->removeEventListener(
        EventTypeNames::keypress, this, false);
  }
}

bool MediaControlsMediaEventListener::operator==(
    const EventListener& other) const {
  return this == &other;
}

HTMLMediaElement& MediaControlsMediaEventListener::GetMediaElement() {
  return media_controls_->MediaElement();
}

void MediaControlsMediaEventListener::handleEvent(
    ExecutionContext* execution_context,
    Event* event) {
  if (event->type() == EventTypeNames::volumechange) {
    media_controls_->OnVolumeChange();
    return;
  }
  if (event->type() == EventTypeNames::focusin) {
    media_controls_->OnFocusIn();
    return;
  }
  if (event->type() == EventTypeNames::timeupdate) {
    media_controls_->OnTimeUpdate();
    return;
  }
  if (event->type() == EventTypeNames::durationchange) {
    media_controls_->OnDurationChange();
    return;
  }
  if (event->type() == EventTypeNames::play) {
    media_controls_->OnPlay();
    return;
  }
  if (event->type() == EventTypeNames::playing) {
    media_controls_->OnPlaying();
    return;
  }
  if (event->type() == EventTypeNames::pause) {
    media_controls_->OnPause();
    return;
  }
  if (event->type() == EventTypeNames::error) {
    media_controls_->OnError();
    return;
  }
  if (event->type() == EventTypeNames::loadedmetadata) {
    media_controls_->OnLoadedMetadata();
    return;
  }

  // Fullscreen handling.
  if (event->type() == EventTypeNames::fullscreenchange ||
      event->type() == EventTypeNames::webkitfullscreenchange) {
    if (GetMediaElement().IsFullscreen())
      media_controls_->OnEnteredFullscreen();
    else
      media_controls_->OnExitedFullscreen();
    return;
  }

  // TextTracks events.
  if (event->type() == EventTypeNames::addtrack ||
      event->type() == EventTypeNames::removetrack) {
    media_controls_->OnTextTracksAddedOrRemoved();
    return;
  }
  if (event->type() == EventTypeNames::change) {
    media_controls_->OnTextTracksChanged();
    return;
  }

  // Keypress events.
  if (event->type() == EventTypeNames::keypress) {
    if (event->currentTarget() == media_controls_->PanelElement())
      media_controls_->OnPanelKeypress();
    return;
  }

  NOTREACHED();
}

DEFINE_TRACE(MediaControlsMediaEventListener) {
  EventListener::Trace(visitor);
  visitor->Trace(media_controls_);
}

}  // namespace blink
