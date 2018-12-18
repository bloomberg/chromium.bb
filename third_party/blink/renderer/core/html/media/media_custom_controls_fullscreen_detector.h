// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_CUSTOM_CONTROLS_FULLSCREEN_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_CUSTOM_CONTROLS_FULLSCREEN_DETECTOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class HTMLVideoElement;
class IntRect;
class TimerBase;

class CORE_EXPORT MediaCustomControlsFullscreenDetector final
    : public NativeEventListener {
 public:
  explicit MediaCustomControlsFullscreenDetector(HTMLVideoElement&);

  void Attach();
  void Detach();
  void ContextDestroyed();

  // EventListener implementation.
  void Invoke(ExecutionContext*, Event*) override;

  void Trace(blink::Visitor*) override;

 private:
  friend class MediaCustomControlsFullscreenDetectorTest;
  friend class HTMLMediaElementEventListenersTest;

  HTMLVideoElement& VideoElement() { return *video_element_; }

  void OnCheckViewportIntersectionTimerFired(TimerBase*);

  bool IsVideoOrParentFullscreen();

  static bool ComputeIsDominantVideoForTests(const IntRect& target_rect,
                                             const IntRect& root_rect,
                                             const IntRect& intersection_rect);

  // `video_element_` owns |this|.
  Member<HTMLVideoElement> video_element_;
  TaskRunnerTimer<MediaCustomControlsFullscreenDetector>
      check_viewport_intersection_timer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_CUSTOM_CONTROLS_FULLSCREEN_DETECTOR_H_
