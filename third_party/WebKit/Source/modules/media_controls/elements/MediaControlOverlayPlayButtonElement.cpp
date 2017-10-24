// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlOverlayPlayButtonElement.h"

#include "core/dom/ElementShadow.h"
#include "core/dom/events/Event.h"
#include "core/html/media/HTMLMediaElement.h"
#include "core/html/media/HTMLMediaSource.h"
#include "core/input_type_names.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "modules/media_controls/elements/MediaControlElementsHelper.h"
#include "platform/runtime_enabled_features.h"
#include "public/platform/Platform.h"

namespace {

bool IsModern() {
  return blink::RuntimeEnabledFeatures::ModernMediaControlsEnabled();
}

}  // namespace.

namespace blink {

// The DOM structure looks like:
//
// MediaControlOverlayPlayButtonElement
//   (-webkit-media-controls-overlay-play-button)
// +-div (-internal-media-controls-overlay-play-button-internal)
//   {if IsModern}
//   This contains the inner circle with the actual play/pause icon.
MediaControlOverlayPlayButtonElement::MediaControlOverlayPlayButtonElement(
    MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls, kMediaOverlayPlayButton) {
  EnsureUserAgentShadowRoot();
  setType(InputTypeNames::button);
  SetShadowPseudoId(AtomicString("-webkit-media-controls-overlay-play-button"));

  if (IsModern()) {
    ShadowRoot& shadow_root = Shadow()->OldestShadowRoot();
    MediaControlElementsHelper::CreateDiv(
        "-internal-media-controls-overlay-play-button-internal", &shadow_root);
  }
}

void MediaControlOverlayPlayButtonElement::UpdateDisplayType() {
  SetIsWanted(MediaElement().ShouldShowControls() &&
              (IsModern() || MediaElement().paused()));
  MediaControlInputElement::UpdateDisplayType();
}

const char* MediaControlOverlayPlayButtonElement::GetNameForHistograms() const {
  return "PlayOverlayButton";
}

void MediaControlOverlayPlayButtonElement::DefaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::click) {
    if (MediaElement().paused()) {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.PlayOverlay"));
    } else {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.PauseOverlay"));
    }

    // Allow play attempts for plain src= media to force a reload in the error
    // state. This allows potential recovery for transient network and decoder
    // resource issues.
    const String& url = MediaElement().currentSrc().GetString();
    if (MediaElement().error() && !HTMLMediaElement::IsMediaStreamURL(url) &&
        !HTMLMediaSource::Lookup(url)) {
      MediaElement().load();
    }

    MediaElement().TogglePlayState();
    UpdateDisplayType();
    event->SetDefaultHandled();
  }
  // TODO(mlamouri): should call MediaControlInputElement::DefaultEventHandler.
}

bool MediaControlOverlayPlayButtonElement::KeepEventInNode(Event* event) {
  return MediaControlElementsHelper::IsUserInteractionEvent(event);
}

}  // namespace blink
