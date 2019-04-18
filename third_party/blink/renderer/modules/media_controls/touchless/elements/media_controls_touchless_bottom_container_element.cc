// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_bottom_container_element.h"

#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_time_display_element.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_timeline_element.h"

namespace blink {

MediaControlsTouchlessBottomContainerElement::
    MediaControlsTouchlessBottomContainerElement(
        MediaControlsTouchlessImpl& media_controls)
    : MediaControlsTouchlessElement(media_controls) {
  SetShadowPseudoId(
      AtomicString("-internal-media-controls-touchless-bottom-container"));

  MediaControlsTouchlessTimeDisplayElement* time_display_element =
      MakeGarbageCollected<MediaControlsTouchlessTimeDisplayElement>(
          media_controls);
  MediaControlsTouchlessTimelineElement* timeline_element =
      MakeGarbageCollected<MediaControlsTouchlessTimelineElement>(
          media_controls);

  ParserAppendChild(time_display_element);
  ParserAppendChild(timeline_element);
}

void MediaControlsTouchlessBottomContainerElement::Trace(
    blink::Visitor* visitor) {
  MediaControlsTouchlessElement::Trace(visitor);
}

}  // namespace blink
