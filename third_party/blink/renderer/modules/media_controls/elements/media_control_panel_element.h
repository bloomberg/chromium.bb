// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_PANEL_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_PANEL_ELEMENT_H_

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_div_element.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class Event;
class MediaControlsImpl;
class TimerBase;

class MediaControlPanelElement final : public MediaControlDivElement {
 public:
  explicit MediaControlPanelElement(MediaControlsImpl&);

 public:
  // Methods called by `MediaContrlsImpl`.
  void SetIsDisplayed(bool);
  bool IsOpaque() const;
  void MakeOpaque();
  void MakeTransparent();

 private:
  void DefaultEventHandler(Event*) override;
  bool KeepEventInNode(Event*) override;

  void StartTimer();
  void StopTimer();
  void TransitionTimerFired(TimerBase*);
  void DidBecomeVisible();

  bool is_displayed_ = false;
  bool opaque_ = true;

  TaskRunnerTimer<MediaControlPanelElement> transition_timer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_PANEL_ELEMENT_H_
