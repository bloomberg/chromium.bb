// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaCustomControlsFullscreenDetector_h
#define MediaCustomControlsFullscreenDetector_h

#include "core/CoreExport.h"
#include "core/dom/events/EventListener.h"
#include "platform/Timer.h"

namespace blink {

class HTMLVideoElement;
class IntRect;
class TimerBase;

class CORE_EXPORT MediaCustomControlsFullscreenDetector final
    : public EventListener {
  WTF_MAKE_NONCOPYABLE(MediaCustomControlsFullscreenDetector);

 public:
  explicit MediaCustomControlsFullscreenDetector(HTMLVideoElement&);

  // EventListener implementation.
  bool operator==(const EventListener&) const override;

  void Attach();
  void Detach();
  void ContextDestroyed();

  virtual void Trace(blink::Visitor*);

 private:
  friend class MediaCustomControlsFullscreenDetectorTest;
  friend class HTMLMediaElementEventListenersTest;

  // EventListener implementation.
  void handleEvent(ExecutionContext*, Event*) override;

  HTMLVideoElement& VideoElement() { return *video_element_; }

  void OnCheckViewportIntersectionTimerFired(TimerBase*);

  bool IsVideoOrParentFullscreen();

  static bool ComputeIsDominantVideoForTests(const IntRect& target_rect,
                                             const IntRect& root_rect,
                                             const IntRect& intersection_rect);

  // `m_videoElement` owns |this|.
  Member<HTMLVideoElement> video_element_;
  TaskRunnerTimer<MediaCustomControlsFullscreenDetector>
      check_viewport_intersection_timer_;
};

}  // namespace blink

#endif  // MediaCustomControlsFullscreenDetector_h
