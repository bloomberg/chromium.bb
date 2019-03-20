// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/non_touch/elements/media_controls_non_touch_overlay_element.h"

#include "third_party/blink/renderer/modules/media_controls/non_touch/elements/media_controls_non_touch_play_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/non_touch/elements/media_controls_non_touch_seek_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/non_touch/elements/media_controls_non_touch_volume_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/non_touch/media_controls_non_touch_impl.h"

namespace blink {

MediaControlsNonTouchOverlayElement::MediaControlsNonTouchOverlayElement(
    MediaControlsNonTouchImpl& media_controls)
    : HTMLDivElement(media_controls.GetDocument()),
      MediaControlsNonTouchElement(media_controls) {
  SetShadowPseudoId(AtomicString("-internal-media-controls-non-touch-overlay"));

  MediaControlsNonTouchPlayButtonElement* play_button =
      MakeGarbageCollected<MediaControlsNonTouchPlayButtonElement>(
          media_controls);

  MediaControlsNonTouchVolumeButtonElement* volume_up_button =
      MakeGarbageCollected<MediaControlsNonTouchVolumeButtonElement>(
          media_controls, true);
  MediaControlsNonTouchVolumeButtonElement* volume_down_button =
      MakeGarbageCollected<MediaControlsNonTouchVolumeButtonElement>(
          media_controls, false);

  MediaControlsNonTouchSeekButtonElement* seek_forward_button =
      MakeGarbageCollected<MediaControlsNonTouchSeekButtonElement>(
          media_controls, true);
  MediaControlsNonTouchSeekButtonElement* seek_backward_button =
      MakeGarbageCollected<MediaControlsNonTouchSeekButtonElement>(
          media_controls, false);

  ParserAppendChild(volume_up_button);
  ParserAppendChild(seek_backward_button);
  ParserAppendChild(play_button);
  ParserAppendChild(seek_forward_button);
  ParserAppendChild(volume_down_button);
}

void MediaControlsNonTouchOverlayElement::Trace(blink::Visitor* visitor) {
  HTMLDivElement::Trace(visitor);
  MediaControlsNonTouchElement::Trace(visitor);
}

}  // namespace blink
