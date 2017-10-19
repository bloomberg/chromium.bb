// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlDivElement.h"

#include "modules/media_controls/MediaControlsImpl.h"

namespace blink {

void MediaControlDivElement::SetOverflowElementIsWanted(bool) {}

void MediaControlDivElement::MaybeRecordDisplayed() {
  // No-op. At the moment, usage is only recorded in the context of CTR. It
  // could be recorded for MediaControlDivElement but there is no need for it at
  // the moment.
}

MediaControlDivElement::MediaControlDivElement(
    MediaControlsImpl& media_controls,
    MediaControlElementType display_type)
    : HTMLDivElement(media_controls.GetDocument()),
      MediaControlElementBase(media_controls, display_type, this) {}

bool MediaControlDivElement::IsMediaControlElement() const {
  return true;
}

void MediaControlDivElement::Trace(blink::Visitor* visitor) {
  HTMLDivElement::Trace(visitor);
  MediaControlElementBase::Trace(visitor);
}

}  // namespace blink
