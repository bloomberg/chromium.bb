// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlDivElement.h"

#include "modules/media_controls/MediaControlsImpl.h"

namespace blink {

MediaControlDivElement::MediaControlDivElement(
    MediaControlsImpl& media_controls,
    MediaControlElementType display_type)
    : HTMLDivElement(media_controls.GetDocument()),
      MediaControlElementBase(media_controls, display_type, this) {}

bool MediaControlDivElement::IsMediaControlElement() const {
  return true;
}

DEFINE_TRACE(MediaControlDivElement) {
  HTMLDivElement::Trace(visitor);
  MediaControlElementBase::Trace(visitor);
}

}  // namespace blink
