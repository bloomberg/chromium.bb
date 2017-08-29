// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlOverflowMenuListElement.h"

#include "core/dom/events/Event.h"
#include "modules/media_controls/MediaControlsImpl.h"

namespace blink {

MediaControlOverflowMenuListElement::MediaControlOverflowMenuListElement(
    MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls, kMediaOverflowList) {
  SetShadowPseudoId(
      AtomicString("-internal-media-controls-overflow-menu-list"));
  SetIsWanted(false);
}

void MediaControlOverflowMenuListElement::DefaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::click)
    event->SetDefaultHandled();

  MediaControlDivElement::DefaultEventHandler(event);
}

}  // namespace blink
