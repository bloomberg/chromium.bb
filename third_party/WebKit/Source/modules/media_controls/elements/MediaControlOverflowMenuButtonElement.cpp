// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlOverflowMenuButtonElement.h"

#include "core/InputTypeNames.h"
#include "core/events/Event.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "public/platform/Platform.h"

namespace blink {

MediaControlOverflowMenuButtonElement::MediaControlOverflowMenuButtonElement(
    MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls, kMediaOverflowButton) {
  EnsureUserAgentShadowRoot();
  setType(InputTypeNames::button);
  SetShadowPseudoId(AtomicString("-internal-media-controls-overflow-button"));
  SetIsWanted(false);
}

bool MediaControlOverflowMenuButtonElement::WillRespondToMouseClickEvents() {
  return true;
}

const char* MediaControlOverflowMenuButtonElement::GetNameForHistograms()
    const {
  return "OverflowButton";
}

void MediaControlOverflowMenuButtonElement::DefaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::click) {
    if (static_cast<MediaControlsImpl&>(GetMediaControls())
            .OverflowMenuVisible()) {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.OverflowClose"));
    } else {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.OverflowOpen"));
    }

    static_cast<MediaControlsImpl&>(GetMediaControls()).ToggleOverflowMenu();
    event->SetDefaultHandled();
  }

  MediaControlInputElement::DefaultEventHandler(event);
}

}  // namespace blink
