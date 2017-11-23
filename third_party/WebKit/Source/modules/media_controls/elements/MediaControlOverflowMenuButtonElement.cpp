// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlOverflowMenuButtonElement.h"

#include "core/dom/events/Event.h"
#include "core/input_type_names.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "public/platform/Platform.h"

namespace blink {

MediaControlOverflowMenuButtonElement::MediaControlOverflowMenuButtonElement(
    MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls, kMediaOverflowButton) {
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
    if (GetMediaControls().OverflowMenuVisible()) {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.OverflowClose"));
    } else {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.OverflowOpen"));
    }

    GetMediaControls().ToggleOverflowMenu();
    event->SetDefaultHandled();
  }

  MediaControlInputElement::DefaultEventHandler(event);
}

}  // namespace blink
