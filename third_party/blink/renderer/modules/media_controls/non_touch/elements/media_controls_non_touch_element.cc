// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/non_touch/elements/media_controls_non_touch_element.h"

#include "third_party/blink/renderer/modules/media_controls/non_touch/media_controls_non_touch_impl.h"
#include "third_party/blink/renderer/modules/media_controls/non_touch/media_controls_non_touch_media_event_listener.h"

namespace blink {

MediaControlsNonTouchElement::MediaControlsNonTouchElement(
    MediaControlsNonTouchImpl& media_controls)
    : media_controls_(media_controls) {
  media_controls_->MediaEventListener().AddObserver(this);
}

void MediaControlsNonTouchElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(media_controls_);
}

}  // namespace blink
