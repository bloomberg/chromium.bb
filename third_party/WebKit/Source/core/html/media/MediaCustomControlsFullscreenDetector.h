// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaCustomControlsFullscreenDetector_h
#define MediaCustomControlsFullscreenDetector_h

#include "core/CoreExport.h"
#include "core/events/EventListener.h"
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

  void attach();
  void detach();
  void contextDestroyed();

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class MediaCustomControlsFullscreenDetectorTest;
  friend class HTMLMediaElementEventListenersTest;

  // EventListener implementation.
  void handleEvent(ExecutionContext*, Event*) override;

  HTMLVideoElement& videoElement() { return *m_videoElement; }

  void onCheckViewportIntersectionTimerFired(TimerBase*);

  bool isVideoOrParentFullscreen();

  static bool computeIsDominantVideoForTests(const IntRect& targetRect,
                                             const IntRect& rootRect,
                                             const IntRect& intersectionRect);

  // `m_videoElement` owns |this|.
  Member<HTMLVideoElement> m_videoElement;
  TaskRunnerTimer<MediaCustomControlsFullscreenDetector>
      m_checkViewportIntersectionTimer;
};

}  // namespace blink

#endif  // MediaCustomControlsFullscreenDetector_h
