// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlFullscreenButtonElement.h"

#include "core/InputTypeNames.h"
#include "core/events/Event.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLMediaElement.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "public/platform/Platform.h"

namespace blink {

MediaControlFullscreenButtonElement::MediaControlFullscreenButtonElement(
    MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls, kMediaEnterFullscreenButton) {
  EnsureUserAgentShadowRoot();
  setType(InputTypeNames::button);
  SetShadowPseudoId(AtomicString("-webkit-media-controls-fullscreen-button"));
  SetIsFullscreen(MediaElement().IsFullscreen());
  SetIsWanted(false);
}

void MediaControlFullscreenButtonElement::SetIsFullscreen(bool is_fullscreen) {
  SetDisplayType(is_fullscreen ? kMediaExitFullscreenButton
                               : kMediaEnterFullscreenButton);
}

bool MediaControlFullscreenButtonElement::WillRespondToMouseClickEvents() {
  return true;
}

WebLocalizedString::Name
MediaControlFullscreenButtonElement::GetOverflowStringName() const {
  if (MediaElement().IsFullscreen())
    return WebLocalizedString::kOverflowMenuExitFullscreen;
  return WebLocalizedString::kOverflowMenuEnterFullscreen;
}

bool MediaControlFullscreenButtonElement::HasOverflowButton() const {
  return true;
}

const char* MediaControlFullscreenButtonElement::GetNameForHistograms() const {
  return IsOverflowElement() ? "FullscreenOverflowButton" : "FullscreenButton";
}

void MediaControlFullscreenButtonElement::DefaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::click) {
    RecordClickMetrics();
    if (MediaElement().IsFullscreen())
      static_cast<MediaControlsImpl&>(GetMediaControls()).ExitFullscreen();
    else
      static_cast<MediaControlsImpl&>(GetMediaControls()).EnterFullscreen();
    event->SetDefaultHandled();
  }
  MediaControlInputElement::DefaultEventHandler(event);
}

void MediaControlFullscreenButtonElement::RecordClickMetrics() {
  bool is_embedded_experience_enabled =
      GetDocument().GetSettings() &&
      GetDocument().GetSettings()->GetEmbeddedMediaExperienceEnabled();

  if (MediaElement().IsFullscreen()) {
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.ExitFullscreen"));
    if (is_embedded_experience_enabled) {
      Platform::Current()->RecordAction(UserMetricsAction(
          "Media.Controls.ExitFullscreen.EmbeddedExperience"));
    }
  } else {
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.EnterFullscreen"));
    if (is_embedded_experience_enabled) {
      Platform::Current()->RecordAction(UserMetricsAction(
          "Media.Controls.EnterFullscreen.EmbeddedExperience"));
    }
  }
}

}  // namespace blink
