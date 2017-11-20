// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlOverlayEnclosureElement.h"

#include "core/dom/events/Event.h"
#include "modules/media_controls/MediaControlsImpl.h"

namespace blink {

MediaControlOverlayEnclosureElement::MediaControlOverlayEnclosureElement(
    MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls, kMediaControlsPanel) {
  SetShadowPseudoId(AtomicString("-webkit-media-controls-overlay-enclosure"));
}

void MediaControlOverlayEnclosureElement::DefaultEventHandler(Event* event) {
  // When the user interacts with the media element, the Cast overlay button
  // needs to be shown.
  if (event->type() == EventTypeNames::gesturetap ||
      event->type() == EventTypeNames::click ||
      event->type() == EventTypeNames::pointerover ||
      event->type() == EventTypeNames::pointermove) {
    GetMediaControls().ShowOverlayCastButtonIfNeeded();
  }

  MediaControlDivElement::DefaultEventHandler(event);
}

}  // namespace blink
