// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/non_touch/elements/media_controls_non_touch_seek_button_element.h"

#include "third_party/blink/renderer/modules/media_controls/non_touch/media_controls_non_touch_impl.h"

namespace blink {

MediaControlsNonTouchSeekButtonElement::MediaControlsNonTouchSeekButtonElement(
    MediaControlsNonTouchImpl& controls,
    bool forward)
    : HTMLDivElement(controls.GetDocument()),
      MediaControlsNonTouchElement(controls) {
  SetShadowPseudoId(AtomicString(
      forward ? "-internal-media-controls-non-touch-seek-forward-button"
              : "-internal-media-controls-non-touch-seek-backward-button"));
}

void MediaControlsNonTouchSeekButtonElement::Trace(blink::Visitor* visitor) {
  HTMLDivElement::Trace(visitor);
  MediaControlsNonTouchElement::Trace(visitor);
}

}  // namespace blink
