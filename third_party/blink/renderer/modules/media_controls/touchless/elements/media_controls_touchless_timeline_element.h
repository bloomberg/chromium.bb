// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_ELEMENTS_MEDIA_CONTROLS_TOUCHLESS_TIMELINE_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_ELEMENTS_MEDIA_CONTROLS_TOUCHLESS_TIMELINE_ELEMENT_H_

#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_element.h"

namespace blink {

class MediaControlsTouchlessTimelineElement
    : public HTMLDivElement,
      public MediaControlsTouchlessElement {
  USING_GARBAGE_COLLECTED_MIXIN(MediaControlsTouchlessTimelineElement);

 public:
  explicit MediaControlsTouchlessTimelineElement(MediaControlsTouchlessImpl&);

  // MediaControlsTouchlessMediaEventListenerObserver overrides
  void OnTimeUpdate() override;
  void OnDurationChange() override;
  void OnLoadingProgress() override;

  void Trace(blink::Visitor*) override;

 private:
  void SetBarWidth(HTMLDivElement* bar, double percent);

  void UpdateBars();
  void UpdateBarsCSS();

  Member<HTMLDivElement> loaded_bar_;
  Member<HTMLDivElement> progress_bar_;
  double current_time_;
  double duration_;

  // Used for setting the width of the progress and loaded bars. This is
  // a percentage of the duration of the media.
  double progress_percent_;
  double loaded_percent_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_ELEMENTS_MEDIA_CONTROLS_TOUCHLESS_TIMELINE_ELEMENT_H_
