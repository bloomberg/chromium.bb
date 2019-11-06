// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_ELEMENTS_MEDIA_CONTROLS_TOUCHLESS_BOTTOM_CONTAINER_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_ELEMENTS_MEDIA_CONTROLS_TOUCHLESS_BOTTOM_CONTAINER_ELEMENT_H_

#include "third_party/blink/renderer/modules/media_controls/media_controls_shared_helper.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_element.h"

namespace blink {

class MediaControlsTouchlessImpl;
class MediaControlsTouchlessTimelineElement;
class MediaControlsTouchlessTimeDisplayElement;
class LayoutObject;

class MediaControlsTouchlessBottomContainerElement
    : public MediaControlsTouchlessElement {
 public:
  MediaControlsTouchlessBottomContainerElement(MediaControlsTouchlessImpl&);
  LayoutObject* TimelineLayoutObject();
  LayoutObject* TimeDisplayLayoutObject();

  void MakeOpaque(bool);
  void Trace(blink::Visitor*) override;

 private:
  void HandleTransitionEndEvent();
  void SetDisplayed(bool);

  Member<MediaControlsTouchlessTimelineElement> timeline_element_;
  Member<MediaControlsTouchlessTimeDisplayElement> time_display_element_;
  Member<MediaControlsSharedHelpers::TransitionEventListener> event_listener_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_ELEMENTS_MEDIA_CONTROLS_TOUCHLESS_BOTTOM_CONTAINER_ELEMENT_H_
