// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_OVERLAY_PLAY_BUTTON_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_OVERLAY_PLAY_BUTTON_ELEMENT_H_

#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_input_element.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class Event;
class MediaControlsImpl;
class MouseEvent;

class MODULES_EXPORT MediaControlOverlayPlayButtonElement final
    : public MediaControlInputElement {
 public:
  explicit MediaControlOverlayPlayButtonElement(MediaControlsImpl&);

  // MediaControlInputElement overrides.
  void UpdateDisplayType() override;

  void OnMediaKeyboardEvent(Event* event) { DefaultEventHandler(*event); }

  WebSize GetSizeOrDefault() const final;

  void SetIsDisplayed(bool);

  void Trace(blink::Visitor*) override;

 protected:
  const char* GetNameForHistograms() const override;

 private:
  friend class MediaControlOverlayPlayButtonElementTest;

  void TapTimerFired(TimerBase*);

  void DefaultEventHandler(Event&) override;
  bool KeepEventInNode(const Event&) const override;

  bool ShouldCausePlayPause(const Event&) const;
  bool IsMouseEventOnInternalButton(const MouseEvent&) const;
  void MaybePlayPause();
  void MaybeJump(int);

  TaskRunnerTimer<MediaControlOverlayPlayButtonElement> tap_timer_;
  base::Optional<bool> tap_was_touch_event_;

  Member<HTMLDivElement> internal_button_;

  bool displayed_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_OVERLAY_PLAY_BUTTON_ELEMENT_H_
