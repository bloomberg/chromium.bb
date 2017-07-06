// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlTimeDisplayElement.h"

#include "modules/media_controls/MediaControlsImpl.h"

namespace blink {

MediaControlTimeDisplayElement::MediaControlTimeDisplayElement(
    MediaControlsImpl& media_controls,
    MediaControlElementType display_type)
    : MediaControlDivElement(media_controls, display_type) {}

void MediaControlTimeDisplayElement::SetCurrentValue(double time) {
  current_value_ = time;
}

double MediaControlTimeDisplayElement::CurrentValue() const {
  return current_value_;
}

}  // namespace blink
