// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlPanelEnclosureElement.h"

#include "modules/media_controls/MediaControlsImpl.h"

namespace blink {

MediaControlPanelEnclosureElement::MediaControlPanelEnclosureElement(
    MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls, kMediaControlsPanel) {
  SetShadowPseudoId(AtomicString("-webkit-media-controls-enclosure"));
}

}  // namespace blink
