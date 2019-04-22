// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_play_button_element.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_source.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

MediaControlPlayButtonElement::MediaControlPlayButtonElement(
    MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls) {
  setType(input_type_names::kButton);
  SetShadowPseudoId(AtomicString("-webkit-media-controls-play-button"));
}

bool MediaControlPlayButtonElement::WillRespondToMouseClickEvents() {
  return true;
}

void MediaControlPlayButtonElement::UpdateDisplayType() {
  WebLocalizedString::Name state =
      MediaElement().paused() ? WebLocalizedString::kAXMediaPlayButton
                              : WebLocalizedString::kAXMediaPauseButton;
  setAttribute(html_names::kAriaLabelAttr,
               WTF::AtomicString(GetLocale().QueryString(state)));
  SetClass("pause", MediaElement().paused());
  UpdateOverflowString();

  MediaControlInputElement::UpdateDisplayType();
}

WebLocalizedString::Name MediaControlPlayButtonElement::GetOverflowStringName()
    const {
  if (MediaElement().paused())
    return WebLocalizedString::kOverflowMenuPlay;
  return WebLocalizedString::kOverflowMenuPause;
}

bool MediaControlPlayButtonElement::HasOverflowButton() const {
  return true;
}

bool MediaControlPlayButtonElement::IsControlPanelButton() const {
  return true;
}

const char* MediaControlPlayButtonElement::GetNameForHistograms() const {
  return IsOverflowElement() ? "PlayPauseOverflowButton" : "PlayPauseButton";
}

void MediaControlPlayButtonElement::DefaultEventHandler(Event& event) {
  if (!IsDisabled() && (event.type() == event_type_names::kClick ||
                        event.type() == event_type_names::kGesturetap)) {
    if (MediaElement().paused()) {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.Play"));
    } else {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.Pause"));
    }

    // Allow play attempts for plain src= media to force a reload in the error
    // state. This allows potential recovery for transient network and decoder
    // resource issues.
    const String& url = MediaElement().currentSrc().GetString();
    if (MediaElement().error() && !HTMLMediaSource::Lookup(url))
      MediaElement().load();

    MediaElement().TogglePlayState();
    UpdateDisplayType();
    event.SetDefaultHandled();
  }
  MediaControlInputElement::DefaultEventHandler(event);
}

}  // namespace blink
