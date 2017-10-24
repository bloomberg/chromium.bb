// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlButtonPanelElement.h"

#include "modules/media_controls/MediaControlsImpl.h"

namespace blink {

MediaControlButtonPanelElement::MediaControlButtonPanelElement(
    MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls, kMediaControlsPanel) {
  SetShadowPseudoId(AtomicString("-internal-media-controls-button-panel"));
}

}  // namespace blink
