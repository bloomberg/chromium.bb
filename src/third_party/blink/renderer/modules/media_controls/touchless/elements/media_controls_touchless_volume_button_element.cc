// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_volume_button_element.h"

#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_impl.h"

namespace blink {

MediaControlsTouchlessVolumeButtonElement::
    MediaControlsTouchlessVolumeButtonElement(
        MediaControlsTouchlessImpl& controls,
        bool up)
    : MediaControlsTouchlessElement(controls) {
  SetShadowPseudoId(AtomicString(
      up ? "-internal-media-controls-touchless-volume-up-button"
         : "-internal-media-controls-touchless-volume-down-button"));
}

void MediaControlsTouchlessVolumeButtonElement::Trace(blink::Visitor* visitor) {
  MediaControlsTouchlessElement::Trace(visitor);
}

}  // namespace blink
