// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_seek_button_element.h"

#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_impl.h"

namespace blink {

MediaControlsTouchlessSeekButtonElement::
    MediaControlsTouchlessSeekButtonElement(
        MediaControlsTouchlessImpl& controls,
        bool forward)
    : MediaControlsTouchlessElement(controls) {
  SetShadowPseudoId(AtomicString(
      forward ? "-internal-media-controls-touchless-seek-forward-button"
              : "-internal-media-controls-touchless-seek-backward-button"));
}

void MediaControlsTouchlessSeekButtonElement::Trace(blink::Visitor* visitor) {
  MediaControlsTouchlessElement::Trace(visitor);
}

}  // namespace blink
