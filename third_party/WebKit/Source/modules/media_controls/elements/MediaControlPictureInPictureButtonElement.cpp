// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlPictureInPictureButtonElement.h"

#include "core/dom/events/Event.h"
#include "core/html/media/HTMLMediaElement.h"
#include "core/html/media/HTMLMediaSource.h"
#include "core/input_type_names.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "public/platform/Platform.h"

namespace blink {

MediaControlPictureInPictureButtonElement::
    MediaControlPictureInPictureButtonElement(MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls, kMediaPlayButton) {
  setType(InputTypeNames::button);
  SetShadowPseudoId(
      AtomicString("-internal-media-controls-picture-in-picture-button"));
}

bool MediaControlPictureInPictureButtonElement::
    WillRespondToMouseClickEvents() {
  return true;
}

WebLocalizedString::Name
MediaControlPictureInPictureButtonElement::GetOverflowStringName() const {
  return WebLocalizedString::kOverflowMenuPictureInPicture;
}

bool MediaControlPictureInPictureButtonElement::HasOverflowButton() const {
  return true;
}

const char* MediaControlPictureInPictureButtonElement::GetNameForHistograms()
    const {
  return IsOverflowElement() ? "PictureInPictureOverflowButton"
                             : "PictureInPictureButton";
}

void MediaControlPictureInPictureButtonElement::DefaultEventHandler(
    Event* event) {
  // TODO(apacible): On click, trigger picture in picture.
  MediaControlInputElement::DefaultEventHandler(event);
}

}  // namespace blink
