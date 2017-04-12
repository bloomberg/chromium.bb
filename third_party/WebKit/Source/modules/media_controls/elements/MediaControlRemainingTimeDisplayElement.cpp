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

}  // namespace blink
