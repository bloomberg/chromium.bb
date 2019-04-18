// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_ELEMENTS_MEDIA_CONTROLS_TOUCHLESS_PLAY_BUTTON_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_ELEMENTS_MEDIA_CONTROLS_TOUCHLESS_PLAY_BUTTON_ELEMENT_H_

#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_element.h"

namespace blink {

class MediaControlsTouchlessPlayButtonElement
    : public MediaControlsTouchlessElement {
 public:
  MediaControlsTouchlessPlayButtonElement(MediaControlsTouchlessImpl&);

  // MediaControlsTouchlessMediaEventListenerObserver implementation.
  void OnPlay() override;
  void OnPause() override;

  void Trace(blink::Visitor*) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_ELEMENTS_MEDIA_CONTROLS_TOUCHLESS_PLAY_BUTTON_ELEMENT_H_
