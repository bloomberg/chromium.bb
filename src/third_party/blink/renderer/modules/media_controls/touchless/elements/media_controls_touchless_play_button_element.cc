// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_play_button_element.h"

#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_impl.h"

namespace blink {

namespace {

const char kPlayingCSSClass[] = "playing";
const char kPausedCSSClass[] = "paused";

}  // namespace

MediaControlsTouchlessPlayButtonElement::
    MediaControlsTouchlessPlayButtonElement(
        MediaControlsTouchlessImpl& controls)
    : MediaControlsTouchlessElement(controls) {
  SetShadowPseudoId(
      AtomicString("-internal-media-controls-touchless-play-button"));

  controls.MediaElement().paused() ? OnPause() : OnPlay();
}

void MediaControlsTouchlessPlayButtonElement::OnPlay() {
  classList().setValue(kPlayingCSSClass);
}

void MediaControlsTouchlessPlayButtonElement::OnPause() {
  classList().setValue(kPausedCSSClass);
}

void MediaControlsTouchlessPlayButtonElement::Trace(blink::Visitor* visitor) {
  MediaControlsTouchlessElement::Trace(visitor);
}

}  // namespace blink
