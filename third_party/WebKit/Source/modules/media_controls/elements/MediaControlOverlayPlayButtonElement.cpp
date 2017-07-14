// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlOverlayPlayButtonElement.h"

#include "core/InputTypeNames.h"
#include "core/events/Event.h"
#include "core/html/HTMLMediaElement.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "modules/media_controls/elements/MediaControlElementsHelper.h"
#include "public/platform/Platform.h"

namespace blink {

MediaControlOverlayPlayButtonElement::MediaControlOverlayPlayButtonElement(
    MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls, kMediaOverlayPlayButton) {
  EnsureUserAgentShadowRoot();
  setType(InputTypeNames::button);
  SetShadowPseudoId(AtomicString("-webkit-media-controls-overlay-play-button"));
}

void MediaControlOverlayPlayButtonElement::UpdateDisplayType() {
  SetIsWanted(MediaElement().ShouldShowControls() && MediaElement().paused());
}

const char* MediaControlOverlayPlayButtonElement::GetNameForHistograms() const {
  return "PlayOverlayButton";
}

void MediaControlOverlayPlayButtonElement::DefaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::click && MediaElement().paused()) {
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.PlayOverlay"));
    MediaElement().Play();
    UpdateDisplayType();
    event->SetDefaultHandled();
  }
  // TODO(mlamouri): should call MediaControlInputElement::DefaultEventHandler.
}

bool MediaControlOverlayPlayButtonElement::KeepEventInNode(Event* event) {
  return MediaControlElementsHelper::IsUserInteractionEvent(event);
}

}  // namespace blink
