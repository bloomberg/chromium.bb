// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlRemainingTimeDisplayElement.h"

#include "modules/media_controls/MediaControlsImpl.h"

namespace blink {

MediaControlRemainingTimeDisplayElement::
    MediaControlRemainingTimeDisplayElement(MediaControlsImpl& media_controls)
    : MediaControlTimeDisplayElement(media_controls,
                                     kMediaTimeRemainingDisplay) {
  SetShadowPseudoId(
      AtomicString("-webkit-media-controls-time-remaining-display"));
}

String MediaControlRemainingTimeDisplayElement::FormatTime() const {
  // For the duration display, we prepend a "/ " to deliminate the current time
  // from the duration, e.g. "0:12 / 3:45".
  return "/ " + MediaControlTimeDisplayElement::FormatTime();
}

}  // namespace blink
