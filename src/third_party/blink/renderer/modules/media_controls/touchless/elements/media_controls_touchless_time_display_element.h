// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_ELEMENTS_MEDIA_CONTROLS_TOUCHLESS_TIME_DISPLAY_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_ELEMENTS_MEDIA_CONTROLS_TOUCHLESS_TIME_DISPLAY_ELEMENT_H_

#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_element.h"

namespace blink {

class MediaControlsTouchlessTimeDisplayElement
    : public HTMLDivElement,
      public MediaControlsTouchlessElement {
  USING_GARBAGE_COLLECTED_MIXIN(MediaControlsTouchlessTimeDisplayElement);

 public:
  explicit MediaControlsTouchlessTimeDisplayElement(
      MediaControlsTouchlessImpl&);

  // MediaControlsTouchlessMediaEventListenerObserver overrides
  void OnTimeUpdate() override;
  void OnDurationChange() override;

  void Trace(blink::Visitor* visitor) override;

 private:
  void UpdateTimeDisplay();

  double current_time_;
  double duration_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_ELEMENTS_MEDIA_CONTROLS_TOUCHLESS_TIME_DISPLAY_ELEMENT_H_
