// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/non_touch/elements/media_controls_non_touch_play_button_element.h"

#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/media_controls/non_touch/media_controls_non_touch_impl.h"

namespace blink {

namespace {

const char kPlayingCSSClass[] = "playing";
const char kPausedCSSClass[] = "paused";

}  // namespace

MediaControlsNonTouchPlayButtonElement::MediaControlsNonTouchPlayButtonElement(
    MediaControlsNonTouchImpl& controls)
    : HTMLDivElement(controls.GetDocument()),
      MediaControlsNonTouchElement(controls) {
  SetShadowPseudoId(
      AtomicString("-internal-media-controls-non-touch-play-button"));

  controls.MediaElement().paused() ? OnPause() : OnPlay();
}

void MediaControlsNonTouchPlayButtonElement::OnPlay() {
  classList().setValue(kPlayingCSSClass);
}

void MediaControlsNonTouchPlayButtonElement::OnPause() {
  classList().setValue(kPausedCSSClass);
}

void MediaControlsNonTouchPlayButtonElement::Trace(blink::Visitor* visitor) {
  HTMLDivElement::Trace(visitor);
  MediaControlsNonTouchElement::Trace(visitor);
}

}  // namespace blink
