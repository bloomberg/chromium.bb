// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_ELEMENTS_MEDIA_CONTROLS_TOUCHLESS_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_ELEMENTS_MEDIA_CONTROLS_TOUCHLESS_ELEMENT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_media_event_listener_observer.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class HTMLMediaElement;
class MediaControlsTouchlessImpl;

class MediaControlsTouchlessElement
    : public HTMLDivElement,
      public MediaControlsTouchlessMediaEventListenerObserver {
  USING_GARBAGE_COLLECTED_MIXIN(MediaControlsTouchlessElement);

 public:
  HTMLMediaElement& MediaElement() const;

  void Trace(blink::Visitor* visitor) override;

  void MakeOpaque(bool /** True if control should hide after timer fired */);
  void MakeTransparent(bool = false /** True if hide immediately */);

  // Non-touch media event listener observer implementation.
  void OnFocusIn() override {}
  void OnTimeUpdate() override {}
  void OnDurationChange() override {}
  void OnSeeking() override {}
  void OnLoadingProgress() override {}
  void OnPlay() override {}
  void OnPause() override {}
  void OnEnterFullscreen() override {}
  void OnExitFullscreen() override {}
  void OnError() override {}
  void OnLoadedMetadata() override {}
  void OnKeyPress(KeyboardEvent* event) override {}
  void OnKeyDown(KeyboardEvent* event) override {}
  void OnKeyUp(KeyboardEvent* event) override {}

 protected:
  MediaControlsTouchlessElement(MediaControlsTouchlessImpl& media_controls);

 private:
  void EnsureHideControlTimer();
  void HideControlTimerFired(TimerBase*);
  void StartHideControlTimer();
  void StopHideControlTimer();

  Member<MediaControlsTouchlessImpl> media_controls_;

  std::unique_ptr<TaskRunnerTimer<MediaControlsTouchlessElement>>
      hide_control_timer_;

  DISALLOW_COPY_AND_ASSIGN(MediaControlsTouchlessElement);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_ELEMENTS_MEDIA_CONTROLS_TOUCHLESS_ELEMENT_H_
