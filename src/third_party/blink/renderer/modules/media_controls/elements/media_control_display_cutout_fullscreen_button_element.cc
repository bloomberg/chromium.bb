// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_display_cutout_fullscreen_button_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"

namespace blink {

MediaControlDisplayCutoutFullscreenButtonElement::
    MediaControlDisplayCutoutFullscreenButtonElement(
        MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls,
                               kMediaDisplayCutoutFullscreenButton) {
  setType(InputTypeNames::button);
  SetShadowPseudoId(AtomicString(
      "-internal-media-controls-display-cutout-fullscreen-button"));
  SetIsWanted(false);
}

bool MediaControlDisplayCutoutFullscreenButtonElement::
    WillRespondToMouseClickEvents() {
  return true;
}

void MediaControlDisplayCutoutFullscreenButtonElement::DefaultEventHandler(
    Event& event) {
  if (event.type() == EventTypeNames::click) {
    // The button shouldn't be visible if not in fullscreen.
    DCHECK(MediaElement().IsFullscreen());

    GetDocument().GetViewportData().SetExpandIntoDisplayCutout(
        !GetDocument().GetViewportData().GetExpandIntoDisplayCutout());
    event.SetDefaultHandled();
  }
  HTMLInputElement::DefaultEventHandler(event);
}

const char*
MediaControlDisplayCutoutFullscreenButtonElement::GetNameForHistograms() const {
  return "DisplayCutoutFullscreenButton";
}

}  // namespace blink
