// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_element.h"

#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_impl.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_media_event_listener.h"

namespace blink {

MediaControlsTouchlessElement::MediaControlsTouchlessElement(
    MediaControlsTouchlessImpl& media_controls)
    : media_controls_(media_controls) {
  media_controls_->MediaEventListener().AddObserver(this);
}

HTMLMediaElement& MediaControlsTouchlessElement::MediaElement() const {
  return media_controls_->MediaElement();
}

void MediaControlsTouchlessElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(media_controls_);
}

}  // namespace blink
