// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlMuteButtonElement.h"

#include "core/dom/events/Event.h"
#include "core/html/HTMLMediaElement.h"
#include "core/input_type_names.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "public/platform/Platform.h"

namespace blink {

MediaControlMuteButtonElement::MediaControlMuteButtonElement(
    MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls, kMediaMuteButton) {
  EnsureUserAgentShadowRoot();
  setType(InputTypeNames::button);
  SetShadowPseudoId(AtomicString("-webkit-media-controls-mute-button"));
}

bool MediaControlMuteButtonElement::WillRespondToMouseClickEvents() {
  return true;
}

void MediaControlMuteButtonElement::UpdateDisplayType() {
  // TODO(mlamouri): checking for volume == 0 because the mute button will look
  // 'muted' when the volume is 0 even if the element is not muted. This allows
  // the painting and the display type to actually match.
  bool muted = MediaElement().muted() || MediaElement().volume() == 0;
  SetDisplayType(muted ? kMediaUnMuteButton : kMediaMuteButton);
  SetClass("muted", muted);
  UpdateOverflowString();
}

WebLocalizedString::Name MediaControlMuteButtonElement::GetOverflowStringName()
    const {
  if (MediaElement().muted())
    return WebLocalizedString::kOverflowMenuUnmute;
  return WebLocalizedString::kOverflowMenuMute;
}

bool MediaControlMuteButtonElement::HasOverflowButton() const {
  return true;
}

const char* MediaControlMuteButtonElement::GetNameForHistograms() const {
  return IsOverflowElement() ? "MuteOverflowButton" : "MuteButton";
}

void MediaControlMuteButtonElement::DefaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::click) {
    if (MediaElement().muted()) {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.Unmute"));
    } else {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.Mute"));
    }

    MediaElement().setMuted(!MediaElement().muted());
    event->SetDefaultHandled();
  }

  MediaControlInputElement::DefaultEventHandler(event);
}

}  // namespace blink
