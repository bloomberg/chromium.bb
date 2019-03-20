// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_NON_TOUCH_ELEMENTS_MEDIA_CONTROLS_NON_TOUCH_OVERLAY_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_NON_TOUCH_ELEMENTS_MEDIA_CONTROLS_NON_TOUCH_OVERLAY_ELEMENT_H_

#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/modules/media_controls/non_touch/elements/media_controls_non_touch_element.h"

namespace blink {

class MediaControlsNonTouchImpl;

class MediaControlsNonTouchOverlayElement
    : public HTMLDivElement,
      public MediaControlsNonTouchElement {
  USING_GARBAGE_COLLECTED_MIXIN(MediaControlsNonTouchOverlayElement);

 public:
  MediaControlsNonTouchOverlayElement(MediaControlsNonTouchImpl&);
  void Trace(blink::Visitor*) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_NON_TOUCH_ELEMENTS_MEDIA_CONTROLS_NON_TOUCH_OVERLAY_ELEMENT_H_
