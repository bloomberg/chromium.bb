// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlToggleClosedCaptionsButtonElement.h"

#include "core/dom/events/Event.h"
#include "core/html/media/HTMLMediaElement.h"
#include "core/html/track/TextTrack.h"
#include "core/html/track/TextTrackList.h"
#include "core/input_type_names.h"
#include "modules/media_controls/MediaControlsImpl.h"

namespace blink {

MediaControlToggleClosedCaptionsButtonElement::
    MediaControlToggleClosedCaptionsButtonElement(
        MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls, kMediaShowClosedCaptionsButton) {
  setType(InputTypeNames::button);
  SetShadowPseudoId(
      AtomicString("-webkit-media-controls-toggle-closed-captions-button"));
}

bool MediaControlToggleClosedCaptionsButtonElement::
    WillRespondToMouseClickEvents() {
  return true;
}

void MediaControlToggleClosedCaptionsButtonElement::UpdateDisplayType() {
  bool captions_visible = MediaElement().TextTracksVisible();
  SetDisplayType(captions_visible ? kMediaHideClosedCaptionsButton
                                  : kMediaShowClosedCaptionsButton);
  SetClass("visible", captions_visible);
  UpdateOverflowString();

  MediaControlInputElement::UpdateDisplayType();
}

WebLocalizedString::Name
MediaControlToggleClosedCaptionsButtonElement::GetOverflowStringName() const {
  return WebLocalizedString::kOverflowMenuCaptions;
}

bool MediaControlToggleClosedCaptionsButtonElement::HasOverflowButton() const {
  return true;
}

String
MediaControlToggleClosedCaptionsButtonElement::GetOverflowMenuSubtitleString()
    const {
  if (!MediaElement().HasClosedCaptions() ||
      !MediaElement().TextTracksAreReady()) {
    // Don't show any subtitle if no text tracks are available.
    return String();
  }

  TextTrackList* track_list = MediaElement().textTracks();
  for (unsigned i = 0; i < track_list->length(); i++) {
    TextTrack* track = track_list->AnonymousIndexedGetter(i);
    if (track && track->mode() == TextTrack::ShowingKeyword())
      return GetMediaControls().GetTextTrackLabel(track);
  }

  // Return the label for no text track.
  return GetMediaControls().GetTextTrackLabel(nullptr);
}

const char*
MediaControlToggleClosedCaptionsButtonElement::GetNameForHistograms() const {
  return IsOverflowElement() ? "ClosedCaptionOverflowButton"
                             : "ClosedCaptionButton";
}

void MediaControlToggleClosedCaptionsButtonElement::DefaultEventHandler(
    Event* event) {
  if (event->type() == EventTypeNames::click) {
    if (MediaElement().textTracks()->length() == 1) {
      // If only one track exists, toggle it on/off
      if (MediaElement().textTracks()->HasShowingTracks())
        GetMediaControls().DisableShowingTextTracks();
      else
        GetMediaControls().ShowTextTrackAtIndex(0);
    } else {
      GetMediaControls().ToggleTextTrackList();
    }

    UpdateDisplayType();
    event->SetDefaultHandled();
  }

  MediaControlInputElement::DefaultEventHandler(event);
}

}  // namespace blink
