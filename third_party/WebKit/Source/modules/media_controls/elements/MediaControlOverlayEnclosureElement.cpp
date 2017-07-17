// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlOverlayEnclosureElement.h"

#include "core/events/Event.h"
#include "modules/media_controls/MediaControlsImpl.h"

namespace blink {

MediaControlOverlayEnclosureElement::MediaControlOverlayEnclosureElement(
    MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls, kMediaControlsPanel) {
  SetShadowPseudoId(AtomicString("-webkit-media-controls-overlay-enclosure"));
}

EventDispatchHandlingState*
MediaControlOverlayEnclosureElement::PreDispatchEventHandler(Event* event) {
  // When the media element is clicked or touched we want to make the overlay
  // cast button visible (if the other requirements are right) even if
  // JavaScript is doing its own handling of the event.  Doing it in
  // preDispatchEventHandler prevents any interference from JavaScript.
  // Note that we can't simply test for click, since JS handling of touch events
  // can prevent their translation to click events.
  if (event && (event->type() == EventTypeNames::click ||
                event->type() == EventTypeNames::touchstart)) {
    GetMediaControls().ShowOverlayCastButtonIfNeeded();
  }
  return MediaControlDivElement::PreDispatchEventHandler(event);
}

}  // namespace blink
